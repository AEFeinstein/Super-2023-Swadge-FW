#include "paint_share.h"

#include <stddef.h>
#include <string.h>

#include "p2pConnection.h"
#include "display.h"
#include "bresenham.h"

#include "mode_paint.h"
#include "paint_common.h"
#include "paint_nvs.h"
#include "paint_util.h"

/**
 * Share mode!
 *
 * So, how will this work?
 * On the SENDING swadge:
 * - Select "Share" mode from the paint menu
 * - The image from the most recent slot is displayed (at a smaller scale than in draw mode)
 * - The user can page with Left and Right, or Select to switch between used slots
 * - The user can begin sharing by pressing A or Start
 * - Once sharing begins, the swadge opens a P2P connection
 * - When a receiving swadge is found, sharing begins immediately.
 * - The sender sends a metadata packet, which includes canvas dimensions, and palette
 * - We wait for confirmation that the metadata was acked (TODO: and handled properly.)
 * - Now, we begin sending packets. We wait until each one is ACKed before sending another (TODO: don't send another packet until the receiver asks for more)
 * - Each packet contains an absolute sequence number and as many bytes of pixel data as will fit (palette-indexed and packed into 2 pixels per byte)
 * - Once the last packet has been acked, we're done! Return to share mode
 */

#define SHARE_LEFT_MARGIN 10
#define SHARE_RIGHT_MARGIN 10
#define SHARE_TOP_MARGIN 25
#define SHARE_BOTTOM_MARGIN 5

#define SHARE_PROGRESS_LEFT 30
#define SHARE_PROGESS_RIGHT 30

#define SHARE_PROGRESS_SPEED 25000

#define SHARE_BG_COLOR c444
#define SHARE_CANVAS_BORDER c000
#define SHARE_PROGRESS_BORDER c000
#define SHARE_PROGRESS_BG c555
#define SHARE_PROGRESS_FG c350

const uint8_t SHARE_PACKET_CANVAS_DATA = 0;
const uint8_t SHARE_PACKET_PIXEL_DATA = 1;
const uint8_t SHARE_PACKET_PIXEL_REQUEST = 2;
const uint8_t SHARE_PACKET_RECEIVE_COMPLETE = 3;
const uint8_t SHARE_PACKET_ABORT = 4;
const uint8_t SHARE_PACKET_HELLO = 5;

// The canvas data packet has PAINT_MAX_COLORS bytes of palette, plus 2 uint16_ts of width/height. Also 1 for size
const uint8_t PACKET_LEN_CANVAS_DATA = sizeof(uint8_t) * PAINT_MAX_COLORS + sizeof(uint16_t) * 2;

static const char strOverwriteSlot[] = "Overwrite Slot %d";
static const char strEmptySlot[] = "Save in Slot %d";
static const char strShareSlot[] = "Share Slot %d";

bool paintShareIsSender;

void paintShareEnterMode(display_t* disp);
void paintShareExitMode(void);
void paintShareMainLoop(int64_t elapsedUs);
void paintShareButtonCb(buttonEvt_t* evt);
void paintShareRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void paintShareSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

void paintShareP2pConnCb(p2pInfo* p2p, connectionEvt_t evt);
void paintShareP2pSendCb(p2pInfo* p2pInfo, messageStatus_t status);
void paintShareP2pMsgRecvCb(p2pInfo* p2pInfo, const uint8_t* payload, uint8_t len);

void paintShareSendPixelRequest(void);
void paintShareSendReceiveComplete(void);

void paintShareRetry(void);

void paintShareDoLoad(void);
void paintShareDoSave(void);

bool paintShareDetectWrongMode(void);

// Use a different swadge mode so the main game doesn't take as much battery
swadgeMode modePaintShare =
{
    .modeName = "MFPaint.net",
    .fnEnterMode = paintShareEnterMode,
    .fnExitMode = paintShareExitMode,
    .fnMainLoop = paintShareMainLoop,
    .fnButtonCallback = paintShareButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = paintShareRecvCb,
    .fnEspNowSendCb = paintShareSendCb,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

static bool isSender(void)
{
    return paintShareIsSender;
}

void paintShareInitP2p(void)
{
    paintState->connectionStarted = true;
    paintState->shareSeqNum = 0;
    paintState->shareNewPacket = false;

    p2pDeinit(&paintState->p2pInfo);
    p2pInitialize(&paintState->p2pInfo, 'P', paintShareP2pConnCb, paintShareP2pMsgRecvCb, -15);
    p2pStartConnection(&paintState->p2pInfo);
}

void paintShareDeinitP2p(void)
{
    paintState->connectionStarted = false;
    p2pDeinit(&paintState->p2pInfo);

    paintState->shareSeqNum = 0;
    paintState->shareNewPacket = false;
}

void paintShareEnterMode(display_t* disp)
{
    paintState = calloc(1, sizeof(paintMenu_t));

    PAINT_LOGD("Entering Share Mode");
    paintLoadIndex();

    paintState->connectionStarted = false;

    if (!loadFont("radiostars.font", &paintState->toolbarFont))
    {
        PAINT_LOGE("Unable to load font!");
    }


    // Set the display
    paintState->canvas.disp = paintState->disp = disp;
    paintState->shareNewPacket = false;
    paintState->shareUpdateScreen = true;
    paintState->shareTime = 0;


    if (isSender())
    {
        //////// Load an image...

        PAINT_LOGD("Sender: Selecting slot");
        paintState->shareState = SHARE_SEND_SELECT_SLOT;

        // Start on the most recently saved slot
        paintState->shareSaveSlot = paintGetRecentSlot();
        PAINT_LOGD("paintState->shareSaveSlot = %d", paintState->shareSaveSlot);

        paintState->clearScreen = true;
    }
    else
    {
        PAINT_LOGD("Receiver: Waiting for connection");
        paintState->shareState = SHARE_RECV_WAIT_FOR_CONN;
    }
}

void paintShareSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    p2pSendCb(&paintState->p2pInfo, mac_addr, status);
}

void paintShareRenderProgressBar(int64_t elapsedUs, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    // okay, we're gonna have a real progress bar, not one of those lying fake progress bars
    // 1. While waiting to connect, draw an indeterminate progress bar, like [||  ||  ||  ||] -> [ ||  ||  ||  |] -> [  ||  ||  ||  ]
    // 2. Once connected, we use an absolute progress bar. Basically it's out of the total number of bytes
    // 3. Canvas data: Ok, we'll treat each packet as though it's the same size. After sending, we add 2 to the progress. After ACK, we add 1. After pixel data request, we add another (or is that just the next one)
    // 4. Pixel data: 2 for send, +1 for ack, +1 for receiving

    bool indeterminate = false;
    uint16_t progress = 0;

    switch (paintState->shareState)
    {
        case SHARE_SEND_SELECT_SLOT:
        // No draw
        return;

        case SHARE_SEND_WAIT_FOR_CONN:
        case SHARE_RECV_WAIT_FOR_CONN:
        case SHARE_RECV_WAIT_CANVAS_DATA:
        // We're not connected yet, or don't yet know how much data to expect
        indeterminate = true;
        break;

        case SHARE_SEND_CANVAS_DATA:
        // Haven't sent anything yet, progress at 0
        progress = 0;
        break;

        case SHARE_SEND_WAIT_CANVAS_DATA_ACK:
        // Sent the canvas data, progress at 2
        progress = 2;
        break;

        case SHARE_SEND_WAIT_FOR_PIXEL_REQUEST:
        // Sent (canvas or pixel data, depending on seqnum), progress at 4*(seqnum+1) + 3
        progress = 4 * (paintState->shareSeqNum + 1) + 3;
        break;

        case SHARE_SEND_PIXEL_DATA:
        case SHARE_RECV_PIXEL_DATA:
        // WWaiting to send or receive pixel data, progress at 4*(seqnum+1) + 0
        progress = 4 * (paintState->shareSeqNum + 1);
        break;

        case SHARE_SEND_WAIT_PIXEL_DATA_ACK:
        // Sent pixel data, progress at 4*(seqnum+1) + 2
        progress = 4 * (paintState->shareSeqNum + 1) + 2;
        break;

        case SHARE_RECV_SELECT_SLOT:
        case SHARE_SEND_COMPLETE:
        // We're done, draw the whole progress bar for fun
        progress = 0xFFFF;
        break;
    }

    // Draw border
    plotRect(paintState->disp, x, y, x + w, y + h, SHARE_PROGRESS_BORDER);
    plotRectFilled(paintState->disp, x + 1, y + 1, x + w, y + h, SHARE_PROGRESS_BG);

    if (!indeterminate)
    {
        paintState->shareTime = 0;
        // if we have the canvas dimensinos, we can calculate the max progress
        // (WIDTH / ((totalPacketNum + 1) * 4)) * (currentPacketNum) * 4 + (sent: 2, acked: 3, req'd: 4)
        uint16_t maxProgress = ((paintState->canvas.h * paintState->canvas.w + PAINT_SHARE_PX_PER_PACKET - 1) / PAINT_SHARE_PX_PER_PACKET + 1);

        PAINT_LOGD("Progress: %02d / %02d", progress, maxProgress);
        // Now, we just draw a box at (progress * (width) / maxProgress)
        uint16_t size = (progress > maxProgress ? maxProgress : progress) * w / maxProgress;
        plotRectFilled(paintState->disp, x + 1, y + 1, x + size, y + h, SHARE_PROGRESS_FG);
    }
    else
    {
        uint8_t segCount = 4;
        uint8_t segW = ((w - 2) / segCount / 2);
        uint8_t offset = (elapsedUs / SHARE_PROGRESS_SPEED) % ((w - 2) / segCount);

        for (uint8_t i = 0; i < segCount; i++)
        {
            uint16_t x0 = (offset + i * (segW * 2)) % (w - 2);
            uint16_t x1 = (offset + i * (segW * 2) + segW) % (w - 2);

            if (x0 >= x1)
            {
                // Split the segment into two parts
                // From x0 to MAX
                plotRectFilled(paintState->disp, x + 1 + x0, y + 1, x + w, y + h, SHARE_PROGRESS_FG);

                if (x1 != 0)
                {
                    // From 0 to x1
                    // Don't draw this if x1 == 0, because then x + 1 == x + 1 + x1, and there would be no box
                    plotRectFilled(paintState->disp, x + 1, y + 1, x + 1 + x1, y + h, SHARE_PROGRESS_FG);
                }
            }
            else
            {
                plotRectFilled(paintState->disp, x + 1 + x0, y + 1, x + 1 + x1, y + h, SHARE_PROGRESS_FG);
            }
        }
    }
}

void paintRenderShareMode(int64_t elapsedUs)
{

    if (paintState->canvas.h != 0 && paintState->canvas.w != 0)
    {
        // Top part of screen
        fillDisplayArea(paintState->disp, 0, 0, paintState->disp->w, paintState->canvas.y, SHARE_BG_COLOR);

        // Left side of screen
        fillDisplayArea(paintState->disp, 0, 0, paintState->canvas.x, paintState->disp->h, SHARE_BG_COLOR);

        // Right side of screen
        fillDisplayArea(paintState->disp, paintState->canvas.x + paintState->canvas.w * paintState->canvas.xScale, 0, paintState->disp->w, paintState->disp->h, SHARE_BG_COLOR);

        // Bottom of screen
        fillDisplayArea(paintState->disp, 0, paintState->canvas.y + paintState->canvas.h * paintState->canvas.yScale, paintState->disp->w, paintState->disp->h, SHARE_BG_COLOR);

        // Border the canvas
        plotRect(paintState->disp, paintState->canvas.x - 1, paintState->canvas.y - 1, paintState->canvas.x + paintState->canvas.w * paintState->canvas.xScale + 1, paintState->canvas.y + paintState->canvas.h * paintState->canvas.yScale + 1, SHARE_CANVAS_BORDER);
    }
    else
    {
        // There's no canvas, so just... clear everything
        fillDisplayArea(paintState->disp, 0, 0, paintState->disp->w, paintState->disp->h, SHARE_BG_COLOR);
    }

    const char text[32];

    switch (paintState->shareState)
    {
        case SHARE_RECV_SELECT_SLOT:
        {
            snprintf(text, sizeof(text), paintGetSlotInUse(paintState->shareSaveSlot) ? strOverwriteSlot : strEmptySlot, paintState->shareSaveSlot + 1);
            break;
        }
        case SHARE_SEND_SELECT_SLOT:
        {
            snprintf(text, sizeof(text), strShareSlot, paintState->shareSaveSlot + 1);
            break;
        }
        case SHARE_SEND_WAIT_FOR_CONN:
        case SHARE_RECV_WAIT_FOR_CONN:
        {
            snprintf(text, sizeof(text), "Connecting...");
            break;
        }

        case SHARE_SEND_CANVAS_DATA:
        case SHARE_SEND_WAIT_CANVAS_DATA_ACK:
        case SHARE_SEND_WAIT_FOR_PIXEL_REQUEST:
        case SHARE_SEND_PIXEL_DATA:
        case SHARE_SEND_WAIT_PIXEL_DATA_ACK:
        {
            snprintf(text, sizeof(text), "Sending...");
            break;
        }

        case SHARE_SEND_COMPLETE:
        {
            snprintf(text, sizeof(text), "Complete");
            break;
        }

        case SHARE_RECV_WAIT_CANVAS_DATA:
        case SHARE_RECV_PIXEL_DATA:
        {
            snprintf(text, sizeof(text), "Receiving...");
            break;
        }
    }

    // debug lines
    //plotLine(paintState->disp, 0, SHARE_TOP_MARGIN, paintState->disp->w, SHARE_TOP_MARGIN, c000, 2);
    //plotLine(paintState->disp, 0, paintState->disp->h - SHARE_BOTTOM_MARGIN, paintState->disp->w, paintState->disp->h - SHARE_BOTTOM_MARGIN, c000, 2);

    paintShareRenderProgressBar(elapsedUs, SHARE_PROGRESS_LEFT, 0, paintState->disp->w - SHARE_PROGESS_RIGHT - SHARE_PROGRESS_LEFT, SHARE_TOP_MARGIN);

    // Draw the text over the progress bar
    uint16_t w = textWidth(&paintState->toolbarFont, text);
    drawText(paintState->disp, &paintState->toolbarFont, c000, text, (paintState->disp->w - w) / 2, 4);
}

void paintShareSendCanvas(void)
{
    PAINT_LOGI("Sending canvas metadata...");
    // Set the length to the canvas data packet length, plus one for the packet type
    paintState->sharePacketLen = PACKET_LEN_CANVAS_DATA + 1;
    paintState->sharePacket[0] = SHARE_PACKET_CANVAS_DATA;

    for (uint8_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        paintState->sharePacket[i + 1] = paintState->canvas.palette[i];
    }

    // pack the canvas dimensions in big-endian
    // Height MSB
    paintState->sharePacket[PAINT_MAX_COLORS + 1] = ((uint8_t)((paintState->canvas.h >> 8) & 0xFF));
    // Height LSB
    paintState->sharePacket[PAINT_MAX_COLORS + 2] = ((uint8_t)((paintState->canvas.h >> 0) & 0xFF));
    // Width MSB
    paintState->sharePacket[PAINT_MAX_COLORS + 3] = ((uint8_t)((paintState->canvas.w >> 8) & 0xFF));
    // Height LSB
    paintState->sharePacket[PAINT_MAX_COLORS + 4] = ((uint8_t)((paintState->canvas.w >> 0) & 0xFF));

    paintState->shareState = SHARE_SEND_WAIT_CANVAS_DATA_ACK;
    paintState->shareNewPacket = false;

    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
}

void paintShareHandleCanvas(void)
{
    PAINT_LOGD("Handling %d bytes of canvas data", paintState->sharePacketLen);
    paintState->shareNewPacket = false;

    if (paintState->sharePacket[0] != SHARE_PACKET_CANVAS_DATA)
    {
        PAINT_LOGE("Canvas data has wrong type %d!!!", paintState->sharePacket[0]);
        return;
    }

    for (uint8_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        paintState->canvas.palette[i] = paintState->sharePacket[i + 1];
        PAINT_LOGD("paletteMap[%d] = %d", paintState->canvas.palette[i], i);
    }

    paintState->canvas.h = (paintState->sharePacket[PAINT_MAX_COLORS + 1] << 8) | (paintState->sharePacket[PAINT_MAX_COLORS + 2]);
    paintState->canvas.w = (paintState->sharePacket[PAINT_MAX_COLORS + 3] << 8) | (paintState->sharePacket[PAINT_MAX_COLORS + 4]);

    PAINT_LOGD("Canvas dimensions: %d x %d", paintState->canvas.w, paintState->canvas.h);

    uint8_t scale = paintGetMaxScale(paintState->canvas.disp, paintState->canvas.w, paintState->canvas.h, SHARE_LEFT_MARGIN + SHARE_RIGHT_MARGIN, SHARE_TOP_MARGIN + SHARE_BOTTOM_MARGIN);
    paintState->canvas.xScale = scale;
    paintState->canvas.yScale = scale;

    paintState->canvas.x = SHARE_LEFT_MARGIN + (paintState->canvas.disp->w - SHARE_LEFT_MARGIN - SHARE_RIGHT_MARGIN - paintState->canvas.w * paintState->canvas.xScale) / 2;
    paintState->canvas.y = SHARE_TOP_MARGIN + (paintState->canvas.disp->h - SHARE_TOP_MARGIN - SHARE_BOTTOM_MARGIN - paintState->canvas.h * paintState->canvas.yScale) / 2;

    paintState->disp->clearPx();
    plotRectFilledScaled(paintState->disp, 0, 0, paintState->canvas.w, paintState->canvas.h, c555, paintState->canvas.x, paintState->canvas.y, paintState->canvas.xScale, paintState->canvas.yScale);

    paintState->shareState = SHARE_RECV_PIXEL_DATA;
    paintShareSendPixelRequest();
}

void paintShareSendPixels(void)
{
    paintState->sharePacketLen = PAINT_SHARE_PX_PACKET_LEN + 3;
    // Packet type header
    paintState->sharePacket[0] = SHARE_PACKET_PIXEL_DATA;

    // Packet seqnum
    paintState->sharePacket[1] = (uint8_t)((paintState->shareSeqNum >> 8) & 0xFF);
    paintState->sharePacket[2] = (uint8_t)((paintState->shareSeqNum >> 0) & 0xFF);

    uint16_t x0, y0, x1, y1;
    for (uint8_t i = 0; i < PAINT_SHARE_PX_PACKET_LEN; i++)
    {
        if (PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum + i * 2 >= (paintState->canvas.w * paintState->canvas.h))
        {
            PAINT_LOGD("Breaking on last packet because %d * %d + %d * 2 >= %d * %d ---> %d >= %d", PAINT_SHARE_PX_PER_PACKET, paintState->shareSeqNum, i, paintState->canvas.w, paintState->canvas.h, PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum + i * 2, paintState->canvas.w * paintState->canvas.h);
            paintState->sharePacketLen = i + 3;
            break;
        }
        // TODO dedupe this and the nvs functions into a paintSerialize() or something
        x0 = paintState->canvas.x + ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2)) % paintState->canvas.w * paintState->canvas.xScale;
        y0 = paintState->canvas.y + ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2)) / paintState->canvas.w * paintState->canvas.yScale;
        x1 = paintState->canvas.x + ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2 + 1)) % paintState->canvas.w * paintState->canvas.xScale;
        y1 = paintState->canvas.y + ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2 + 1)) / paintState->canvas.w * paintState->canvas.yScale;

        //PAINT_LOGD("Mapping px(%d, %d) (%d) --> %x", x0, y0, paintState->disp->getPx(x0, y0), paintState->sharePaletteMap[(uint8_t)(paintState->disp->getPx(x0, y0))]);
        paintState->sharePacket[i + 3] = paintState->sharePaletteMap[(uint8_t)paintState->disp->getPx(x0, y0)] << 4 | paintState->sharePaletteMap[(uint8_t)paintState->disp->getPx(x1, y1)];
    }

    paintState->shareState = SHARE_SEND_WAIT_PIXEL_DATA_ACK;
    PAINT_LOGD("p2pSendMsg(%p, %d)", paintState->sharePacket, paintState->sharePacketLen);
    PAINT_LOGD("SENDING DATA:");
    for (uint8_t i = 0; i < paintState->sharePacketLen; i+= 4)
    {
        PAINT_LOGD("%04d %02x %02x %02x %02x", i, paintState->sharePacket[i], paintState->sharePacket[i+1], paintState->sharePacket[i+2], paintState->sharePacket[i+3]);
    }

    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
}

void paintShareHandlePixels(void)
{
    PAINT_LOGD("Handling %d bytes of pixel data", paintState->sharePacketLen);
    paintState->shareNewPacket = false;

    if (paintState->sharePacket[0] != ((uint8_t)SHARE_PACKET_PIXEL_DATA))
    {
        PAINT_LOGE("Received pixel data with incorrect type %d", paintState->sharePacket[0]);
        PAINT_LOGE("First 16 bytes: %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x", paintState->sharePacket[0], paintState->sharePacket[1], paintState->sharePacket[2], paintState->sharePacket[3], paintState->sharePacket[4], paintState->sharePacket[5], paintState->sharePacket[6], paintState->sharePacket[7], paintState->sharePacket[8], paintState->sharePacket[9], paintState->sharePacket[10], paintState->sharePacket[11], paintState->sharePacket[12], paintState->sharePacket[13], paintState->sharePacket[14], paintState->sharePacket[15]);
        return;
    }

    paintState->shareSeqNum = (paintState->sharePacket[1] << 8) | paintState->sharePacket[2];

    PAINT_LOGD("Packet seqnum is %d (%x << 8 | %x)", paintState->shareSeqNum, paintState->sharePacket[1], paintState->sharePacket[2]);

    uint16_t x0, y0, x1, y1;
    for (uint8_t i = 0; i < paintState->sharePacketLen - 3; i++)
    {
        x0 = ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2)) % paintState->canvas.w;
        y0 = ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2)) / paintState->canvas.w;
        x1 = ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2 + 1)) % paintState->canvas.w;
        y1 = ((PAINT_SHARE_PX_PER_PACKET * paintState->shareSeqNum) + (i * 2 + 1)) / paintState->canvas.w;

        setPxScaled(paintState->disp, x0, y0, paintState->canvas.palette[paintState->sharePacket[i + 3] >> 4], paintState->canvas.x, paintState->canvas.y, paintState->canvas.xScale, paintState->canvas.yScale);
        setPxScaled(paintState->disp, x1, y1, paintState->canvas.palette[paintState->sharePacket[i + 3] & 0xF], paintState->canvas.x, paintState->canvas.y, paintState->canvas.xScale, paintState->canvas.yScale);
    }

    PAINT_LOGD("We've received %d / %d pixels", paintState->shareSeqNum * PAINT_SHARE_PX_PER_PACKET + (paintState->sharePacketLen - 3) * 2, paintState->canvas.h * paintState->canvas.w);

    if (paintState->shareSeqNum * PAINT_SHARE_PX_PER_PACKET + (paintState->sharePacketLen - 3) * 2 >= paintState->canvas.h * paintState->canvas.w)
    {
        PAINT_LOGD("I think we're done receiving");
        // We don't reeeally care if the sender acks this packet.
        // I mean, it would be polite to make sure it gets there, but there's not really a point
        paintState->shareState = SHARE_RECV_SELECT_SLOT;
        paintShareSendReceiveComplete();
    }
    else
    {
        PAINT_LOGD("Done handling pixel packet, may we please have some more?");
        paintShareSendPixelRequest();
    }
}

void paintShareSendPixelRequest(void)
{
    paintState->sharePacket[0] = SHARE_PACKET_PIXEL_REQUEST;
    paintState->sharePacketLen = 1;
    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
    paintState->shareUpdateScreen = true;
}

void paintShareSendReceiveComplete(void)
{
    paintState->sharePacket[0] = SHARE_PACKET_RECEIVE_COMPLETE;
    paintState->sharePacketLen = 1;
    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
    paintState->shareUpdateScreen = true;
}

void paintShareSendAbort(void)
{
    paintState->sharePacket[0] = SHARE_PACKET_ABORT;
    paintState->sharePacketLen = 1;
    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
    paintState->shareUpdateScreen = true;
}

void paintShareSendHello(void)
{
    paintState->sharePacket[0] = SHARE_PACKET_HELLO;
    paintState->sharePacket[1] = isSender() ? 1 : 0;
    paintState->sharePacketLen = 2;
    p2pSendMsg(&paintState->p2pInfo, paintState->sharePacket, paintState->sharePacketLen, true, paintShareP2pSendCb);
    paintState->shareUpdateScreen = true;
}

void paintShareMsgSendOk(void)
{
    paintState->shareUpdateScreen = true;
    switch (paintState->shareState)
    {
        case SHARE_SEND_SELECT_SLOT:
        case SHARE_SEND_WAIT_FOR_CONN:
        case SHARE_SEND_CANVAS_DATA:
        break;

        case SHARE_SEND_WAIT_CANVAS_DATA_ACK:
        {
            PAINT_LOGD("Got ACK for canvas data!");
            paintState->shareState = SHARE_SEND_WAIT_FOR_PIXEL_REQUEST;
            break;
        }

        case SHARE_SEND_WAIT_FOR_PIXEL_REQUEST:
        break;

        case SHARE_SEND_PIXEL_DATA:
        break;

        case SHARE_SEND_WAIT_PIXEL_DATA_ACK:
        {
            PAINT_LOGD("Got ACK for pixel data packet %d", paintState->shareSeqNum);

            if (PAINT_SHARE_PX_PER_PACKET * (paintState->shareSeqNum + 1) >= (paintState->canvas.w * paintState->canvas.h))
            {
                PAINT_LOGD("Probably done sending! But waiting for confirmation...");
            }
            paintState->shareState = SHARE_SEND_WAIT_FOR_PIXEL_REQUEST;
            paintState->shareSeqNum++;
            break;
        }

        case SHARE_SEND_COMPLETE:
        {

            break;
        }



        case SHARE_RECV_WAIT_FOR_CONN:
        {
            break;
        }

        case SHARE_RECV_WAIT_CANVAS_DATA:
        {
            break;
        }

        case SHARE_RECV_PIXEL_DATA:
        {
            break;
        }

        case SHARE_RECV_SELECT_SLOT:
        {
            break;
        }
    }
}

void paintShareMsgSendFail(void)
{
    paintShareRetry();
}


void paintBeginShare(void)
{
    paintState->shareState = SHARE_SEND_WAIT_FOR_CONN;

    paintState->shareSeqNum = 0;

    PAINT_LOGD("Sender: Waiting for connection...");
}

void paintShareExitMode(void)
{
    paintState->screen = PAINT_MENU;
    p2pDeinit(&paintState->p2pInfo);
    free(paintState);
}

// Go back to the previous state so we retry the last thing
void paintShareRetry(void)
{
    PAINT_LOGE("Retrying something!");
    // is that all?
    switch (paintState->shareState)
    {
        case SHARE_SEND_SELECT_SLOT:
        case SHARE_SEND_WAIT_FOR_CONN:
        break;

        case SHARE_SEND_CANVAS_DATA:
        {
            paintState->shareNewPacket = true;
            break;
        }

        case SHARE_SEND_WAIT_FOR_PIXEL_REQUEST:
        break;

        case SHARE_SEND_WAIT_CANVAS_DATA_ACK:
        {
            paintState->shareState = SHARE_SEND_CANVAS_DATA;
            paintState->shareNewPacket = true;
            break;
        }

        case SHARE_SEND_PIXEL_DATA:
        {
            paintState->shareNewPacket = true;
            break;
        }

        case SHARE_SEND_WAIT_PIXEL_DATA_ACK:
        {
            paintState->shareState = SHARE_SEND_PIXEL_DATA;
            paintState->shareNewPacket = true;
            break;
        }

        case SHARE_SEND_COMPLETE:
        break;



        case SHARE_RECV_WAIT_FOR_CONN:
        case SHARE_RECV_WAIT_CANVAS_DATA:
        case SHARE_RECV_PIXEL_DATA:
        case SHARE_RECV_SELECT_SLOT:
        break;
    }
    paintState->shareNewPacket = true;
}

void paintShareMainLoop(int64_t elapsedUs)
{
    // Handle the sending of the packets and the other things
    if (paintState->clearScreen)
    {
        PAINT_LOGD("Redrawing!!!");
        paintShareDoLoad();
        paintState->clearScreen = false;
    }

    paintState->shareTime += elapsedUs;

    switch (paintState->shareState)
    {
        case SHARE_SEND_SELECT_SLOT:
        break;

        case SHARE_SEND_WAIT_FOR_CONN:
        paintState->shareUpdateScreen = true;
        if (!paintState->connectionStarted)
        {
            paintShareInitP2p();
        }
        break;

        case SHARE_SEND_CANVAS_DATA:
        {
            if (paintState->shareNewPacket)
            {
                paintShareSendCanvas();
            }
            break;
        }

        case SHARE_SEND_WAIT_CANVAS_DATA_ACK:
        {
            break;
        }

        case SHARE_SEND_WAIT_FOR_PIXEL_REQUEST:
        {
            if (paintState->shareNewPacket)
            {
                if (paintState->sharePacket[0] == SHARE_PACKET_PIXEL_REQUEST)
                {
                    paintState->shareNewPacket = false;
                    paintState->shareState = SHARE_SEND_PIXEL_DATA;
                }
                else if (paintState->sharePacket[0] == SHARE_PACKET_RECEIVE_COMPLETE)
                {
                    PAINT_LOGD("We've received confirmation! All data was received successfully");
                    paintState->shareNewPacket = false;
                    paintState->shareState = SHARE_SEND_COMPLETE;
                    paintState->shareUpdateScreen = true;
                }
            }
            break;
        }

        case SHARE_SEND_PIXEL_DATA:
        {
            paintShareSendPixels();
            break;
        }

        case SHARE_SEND_WAIT_PIXEL_DATA_ACK:
        {
            break;
        }

        case SHARE_SEND_COMPLETE:
        {
            paintShareDeinitP2p();
            break;
        }



        case SHARE_RECV_WAIT_FOR_CONN:
        {
            paintState->shareUpdateScreen = true;
            if (!paintState->connectionStarted)
            {
                PAINT_LOGD("Reiniting p2p...");
                paintShareInitP2p();
            }
            break;
        }

        case SHARE_RECV_WAIT_CANVAS_DATA:
        {
            if (paintState->shareNewPacket)
            {
                paintShareHandleCanvas();
            }
            paintState->shareUpdateScreen = true;

            break;
        }

        case SHARE_RECV_PIXEL_DATA:
        {
            if (paintState->shareNewPacket)
            {
                paintShareHandlePixels();
            }
            paintState->shareUpdateScreen = true;
            break;
        }

        case SHARE_RECV_SELECT_SLOT:
        {
            paintShareDeinitP2p();
            break;
        }
    }

    if (paintState->shareUpdateScreen)
    {
        paintRenderShareMode(paintState->shareTime);
        paintState->shareUpdateScreen = false;
    }
}

void paintShareButtonCb(buttonEvt_t* evt)
{
    if (paintState->shareState == SHARE_SEND_SELECT_SLOT)
    {
        if (evt->down)
        {
            switch (evt->button)
            {
                case LEFT:
                {
                    // Load previous image
                    paintState->shareSaveSlot = paintGetPrevSlotInUse(paintState->shareSaveSlot);
                    paintState->clearScreen = true;
                    paintState->shareUpdateScreen = true;
                    break;
                }
                case RIGHT:
                {
                    // Load next image
                    paintState->shareSaveSlot = paintGetNextSlotInUse(paintState->shareSaveSlot);
                    paintState->clearScreen = true;
                    paintState->shareUpdateScreen = true;
                    break;
                }

                case BTN_A:
                {
                    // Begin sharing!
                    paintBeginShare();
                    paintState->shareUpdateScreen = true;
                    break;
                }

                case UP:
                case DOWN:
                case BTN_B:
                // Do Nothing!
                case SELECT:
                case START:
                // Or do something on button up to avoid conflict with exit mode
                break;
            }
        }
        else
        {
            if (evt->button == SELECT)
            {
                paintState->shareSaveSlot = paintGetNextSlotInUse(paintState->shareSaveSlot);
                paintState->clearScreen = true;
                paintState->shareUpdateScreen = true;
            }
            else if (evt->button == START)
            {
                paintBeginShare();
                paintState->shareUpdateScreen = true;
            }
        }
    }
    else if (paintState->shareState == SHARE_RECV_SELECT_SLOT)
    {
        if (evt->down)
        {
            switch (evt->button)
            {
                case LEFT:
                {
                    paintState->shareSaveSlot = PREV_WRAP(paintState->shareSaveSlot, PAINT_SAVE_SLOTS);
                    paintState->shareUpdateScreen = true;
                    break;
                }

                case RIGHT:
                {
                    paintState->shareSaveSlot = NEXT_WRAP(paintState->shareSaveSlot, PAINT_SAVE_SLOTS);
                    paintState->shareUpdateScreen = true;
                    break;
                }

                case BTN_A:
                {
                    paintShareDoSave();
                    switchToSwadgeMode(&modePaint);
                    break;
                }

                case UP:
                case DOWN:
                case BTN_B:
                // Do Nothing!
                case SELECT:
                case START:
                // Or do something on button-up instead, to avoid overlap with SELECT+START
                break;
            }
        }
        else
        {
            if (evt->button == START)
            {
                paintShareDoSave();
                switchToSwadgeMode(&modePaint);
            }
            else if (evt->button == SELECT)
            {
                paintState->shareSaveSlot = NEXT_WRAP(paintState->shareSaveSlot, PAINT_SAVE_SLOTS);
                paintState->shareUpdateScreen = true;
            }
        }
        // Does the receiver get any buttons?
        // Yes! They need to pick their destination slot before starting P2P
    } else if (paintState->shareState == SHARE_SEND_COMPLETE)
    {
        paintState->shareState = SHARE_SEND_SELECT_SLOT;
    }
}

void paintShareRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    p2pRecvCb(&paintState->p2pInfo, mac_addr, (const uint8_t*)data, len, rssi);
}

void paintShareP2pSendCb(p2pInfo* p2pInfo, messageStatus_t status)
{
    switch (status)
    {
        case MSG_ACKED:
        PAINT_LOGD("ACK");
        paintShareMsgSendOk();
        break;

        case MSG_FAILED:
        PAINT_LOGE("FAILED!!!");
        paintShareMsgSendFail();
        break;
    }
}

void paintShareP2pConnCb(p2pInfo* p2p, connectionEvt_t evt)
{
    switch(evt)
    {
        case CON_STARTED:
        PAINT_LOGD("CON_STARTED");
        //paintShareSendHello();
        break;

        case RX_GAME_START_ACK:
        PAINT_LOGD("RX_GAME_START_ACK");
        break;

        case RX_GAME_START_MSG:
        PAINT_LOGD("RX_GAME_START_MSG");
        break;

        case CON_ESTABLISHED:
        {
            PAINT_LOGD("CON_ESTABLISHED");
            if (paintState->shareState == SHARE_SEND_WAIT_FOR_CONN)
            {
                PAINT_LOGD("state = SHARE_SEND_CANVAS_DATA");
                paintState->shareState = SHARE_SEND_CANVAS_DATA;
                paintState->shareNewPacket = true;
            }
            else if (paintState->shareState == SHARE_RECV_WAIT_FOR_CONN)
            {
                PAINT_LOGD("state = SHARE_RECV_WAIT_CANVAS_DATA");
                paintState->shareState = SHARE_RECV_WAIT_CANVAS_DATA;
            }
            break;
        }

        case CON_LOST:
        PAINT_LOGD("CON_LOST");
        paintShareDeinitP2p();
        if (isSender())
        {
            paintState->shareState = SHARE_SEND_WAIT_FOR_CONN;
        }
        else
        {
            paintState->shareState = SHARE_RECV_WAIT_FOR_CONN;
        }
        break;
    }
}

bool paintShareDetectWrongMode(void)
{
    if (isSender())
    {
        if (paintState->sharePacket[0] == SHARE_PACKET_CANVAS_DATA || paintState->sharePacket[0] == SHARE_PACKET_PIXEL_DATA || paintState->sharePacket[0] == SHARE_PACKET_ABORT || (paintState->sharePacket[0] == SHARE_PACKET_HELLO && paintState->sharePacket[1] == 1))
        {
            if (p2pGetPlayOrder(&paintState->p2pInfo) == GOING_FIRST)
            {
                paintState->shareState = SHARE_SEND_SELECT_SLOT;
                paintState->shareUpdateScreen = true;
            }
            else
            {
                paintState->shareState = SHARE_SEND_WAIT_FOR_CONN;
            }

            if (paintState->sharePacket[0] != SHARE_PACKET_ABORT)
            {
                // if we aren't doing this because of an abort packet, send one to the other side
                paintShareSendAbort();
            }
            paintShareDeinitP2p();
            return true;
        }
    }
    else
    {
        if (paintState->sharePacket[0] == SHARE_PACKET_PIXEL_REQUEST || paintState->sharePacket[0] == SHARE_PACKET_ABORT || (paintState->sharePacket[0] == SHARE_PACKET_HELLO && paintState->sharePacket[1] == 0))
        {
            paintState->shareState = SHARE_RECV_WAIT_FOR_CONN;
            paintState->shareUpdateScreen = true;

            if (paintState->sharePacket[0] != SHARE_PACKET_ABORT)
            {
                paintShareSendAbort();
            }
            paintShareDeinitP2p();
            return true;
        }
    }

    return false;
}

void paintShareP2pMsgRecvCb(p2pInfo* p2p, const uint8_t* payload, uint8_t len)
{
    // no buffer overruns for me thanks
    PAINT_LOGV("Receiving %d bytes via P2P callback", len);
    memcpy(paintState->sharePacket, payload, len);

    if (!paintShareDetectWrongMode())
    {
        PAINT_LOGV("RECEIVED DATA:");
        for (uint8_t i = 0; i < len; i+= 4)
        {
            PAINT_LOGV("%04d %02x %02x %02x %02x", i, payload[i], payload[i+1], payload[i+2], payload[i+3]);
        }
        paintState->sharePacketLen = len;
        paintState->shareNewPacket = true;
    }
}

void paintShareDoLoad(void)
{
    paintState->disp->clearPx();
    // Load just image dimensions;

    if (!paintLoadDimensions(&paintState->canvas, paintState->shareSaveSlot))
    {
        PAINT_LOGE("Failed to load dimensions, stopping load");
        return;
    }

    // With the image dimensions, calculate the max scale that will fit on the screen
    uint8_t scale = paintGetMaxScale(paintState->canvas.disp, paintState->canvas.w, paintState->canvas.h, SHARE_LEFT_MARGIN + SHARE_RIGHT_MARGIN, SHARE_TOP_MARGIN + SHARE_BOTTOM_MARGIN);
    PAINT_LOGD("Loading image at scale %d", scale);
    paintState->canvas.xScale = scale;
    paintState->canvas.yScale = scale;

    // Center the canvas on the empty area of the screen
    paintState->canvas.x = SHARE_LEFT_MARGIN + (paintState->canvas.disp->w - SHARE_LEFT_MARGIN - SHARE_RIGHT_MARGIN - paintState->canvas.w * paintState->canvas.xScale) / 2;
    paintState->canvas.y = SHARE_TOP_MARGIN + (paintState->canvas.disp->h - SHARE_TOP_MARGIN - SHARE_BOTTOM_MARGIN - paintState->canvas.h * paintState->canvas.yScale) / 2;

    // Load the actual image!
    // If all goes well, it will be drawn centered and as big as possible
    paintLoad(&paintState->canvas, paintState->shareSaveSlot);

    for (uint8_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        paintState->sharePaletteMap[(uint8_t)(paintState->canvas.palette[i])] = i;
    }
}

void paintShareDoSave(void)
{
    paintSave(&paintState->canvas, paintState->shareSaveSlot);
}