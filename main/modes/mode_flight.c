/*
 * mode_flight.c
 *
 *  Created on: Sept 15, 2020
 *      Author: <>< CNLohr

 * Consider doing an optimizing video discussing:
   -> Vertex Caching
   -> Occlusion Testing
   -> Fast line-drawing algorithm
   -> Weird code structure of drawPixelUnsafeC
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "cndraw.h"
#include "esp_timer.h"
#include "swadgeMode.h"
#include "swadge_esp32.h"
#include "led_util.h" // leds
#include "meleeMenu.h"
#include "mode_main_menu.h"
#include "nvs_manager.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "swadge_util.h"
#include "text_entry.h"
#include "mode_flight.h"
#include "flight/3denv.h"

// TODO     linkedInfo_t* invYmnu;
// XXX TODO HOW TO DO SAVE DATA
static flightSimSaveData_t savedata;
static int didFlightsimDataLoad;

// Thruster speed, etc.
#define THRUSTER_ACCEL   4
#define THRUSTER_MAX     40 //NOTE: THRUSTER_MAX must be divisble by THRUSTER_ACCEL
#define THRUSTER_DECAY   4
#define FLIGHT_SPEED_DEC 11
#define FLIGHT_MAX_SPEED 50
#define FLIGHT_MIN_SPEED 8

flightSimSaveData_t * getFlightSaveData();
void setFlightSaveData( flightSimSaveData_t * sd );

flightSimSaveData_t * getFlightSaveData()
{
    if( !didFlightsimDataLoad )
    {
        size_t size = sizeof(savedata);
        bool r = readNvsBlob( "flightsim", &savedata, &size );
        if( !r || size != sizeof( savedata ) )
        {
            memset( &savedata, 0, sizeof( savedata ) );
        }
        didFlightsimDataLoad = 1;
    }
    return &savedata;
}

void setFlightSaveData( flightSimSaveData_t * sd )
{
    if( sd != &savedata )
        memcpy( &savedata, sd, sizeof( savedata ) );
    writeNvsBlob( "flightsim", &savedata, sizeof( savedata ) );
}

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

//XXX TODO: Refactor - these should probably be unified.
#define MAXRINGS 15
#define MAX_DONUTS 14
#define MAX_BEANS 69


#define CROSSHAIR_COLOR 200
#define CNDRAW_BLACK 0
#define CNDRAW_WHITE 18 // actually greenish
#define PROMPT_COLOR 92


#define TFT_WIDTH 280
#define TFT_HEIGHT 240

typedef enum
{
    FLIGHT_MENU,
    FLIGHT_GAME,
    FLIGHT_GAME_OVER,
    FLIGHT_HIGH_SCORE_ENTRY,
    FLIGHT_SHOW_HIGH_SCORES,
    FLIGHT_PERFTEST,
} flightModeScreen;

typedef struct
{
    uint16_t nrvertnums;
    uint16_t nrfaces;
    uint16_t indices_per_face;
    int16_t center[3];
    int16_t radius;
    uint16_t label;
    int16_t indices_and_vertices[1];
} tdModel;


typedef enum
{
    FLIGHT_LED_NONE,
    FLIGHT_LED_MENU_TICK,
    FLIGHT_LED_GAME_START,
    FLIGHT_LED_BEAN,
    FLIGHT_LED_DONUT,
    FLIGHT_LED_ENDING,
} flLEDAnimation;

typedef struct
{
    flightModeScreen mode;
    display_t * disp;
    int frames, tframes;
    uint8_t buttonState;

    int16_t planeloc[3];
    int32_t planeloc_fine[3];
    int16_t hpr[3];
    int16_t speed;
    int16_t pitchmoment;
    int16_t yawmoment;
    bool perfMotion;
    bool oob;

    int enviromodels;
    const tdModel ** environment;

    meleeMenu_t * menu;
    font_t ibm;
    font_t radiostars;
    font_t meleeMenuFont;

    int beans;
    int ondonut;
    uint32_t timeOfStart;
    uint32_t timeGot100Percent;
    int wintime;
    bool inverty;
    uint8_t menuEntryForInvertY;

    flLEDAnimation ledAnimation;
    uint8_t        ledAnimationTime;

    char highScoreNameBuffer[FLIGHT_HIGH_SCORE_NAME_LEN];
    uint8_t beangotmask[MAXRINGS];
} flight_t;


int renderlinecolor = CNDRAW_WHITE;

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void flightRender();
static void flightBackground(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum );
static void flightEnterMode(display_t * disp);
static void flightExitMode(void);
static void flightButtonCallback( buttonEvt_t* evt );
static void flightUpdate(void* arg __attribute__((unused)));
static void flightMenuCb(const char* menuItem);
static void flightStartGame(flightModeScreen mode);
static void flightGameUpdate( flight_t * tflight );
static void flightUpdateLEDs(flight_t * tflight);
static void flightLEDAnimate( flLEDAnimation anim );
int tdModelVisibilitycheck( const tdModel * m );
void tdDrawModel( display_t * disp, const tdModel * m );
static int flightTimeHighScorePlace( int wintime, bool is100percent );
static void flightTimeHighScoreInsert( int insertplace, bool is100percent, char * name, int timeCentiseconds );

//Forward libc declarations.
#ifndef EMU
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *));
int abs(int j);
#endif

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode modeFlight =
{
    .modeName = "Flyin Donut",
    .fnEnterMode = flightEnterMode,
    .fnExitMode = flightExitMode,
    .fnButtonCallback = flightButtonCallback,
    .fnBackgroundDrawCallback = flightBackground,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnMainLoop = flightRender,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .overrideUsb = false
};
flight_t* flight;

static const char fl_title[]  = "Flightsim";
static const char fl_flight_env[] = "Take Flight";
static const char fl_flight_invertY0_env[] = "[ ] Y Invert";
static const char fl_flight_invertY1_env[] = "[*] Y Invert";
static const char fl_flight_perf[] = "Perf Test";
static const char str_quit[] = "Quit";
static const char str_high_scores[] = "High Scores";

/*============================================================================
 * Functions
 *==========================================================================*/

static void flightBackground(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
    fillDisplayArea( disp, x, y, x+w, h+y, CNDRAW_BLACK );
}

/**
 * Initializer for flight
 */
static void flightEnterMode(display_t * disp)
{
    // Alloc and clear everything
    flight = malloc(sizeof(flight_t));
    memset(flight, 0, sizeof(flight_t));

    // Hmm this seems not to be obeyed, at least not well?
    setFrameRateUs( 33333 );

    flight->mode = FLIGHT_MENU;
    flight->disp = disp;

    const uint16_t * data = model3d;//(uint16_t*)getAsset( "3denv.obj", &retlen );
    data+=2; //header
    flight->enviromodels = *(data++);
    flight->environment = malloc( sizeof(const tdModel *) * flight->enviromodels );
    int i;
    for( i = 0; i < flight->enviromodels; i++ )
    {
        const tdModel * m = flight->environment[i] = (const tdModel*)data;
        data += 8 + m->nrvertnums + m->nrfaces * m->indices_per_face;
    }

    loadFont("ibm_vga8.font", &flight->ibm);
    loadFont("radiostars.font", &flight->radiostars);
    loadFont("mm.font", &flight->meleeMenuFont);

    flight->menu = initMeleeMenu(fl_title, &flight->meleeMenuFont, flightMenuCb);
    flight->menu->allowLEDControl = 0; // we manage the LEDs

    flight->inverty = getFlightSaveData()->flightInvertY;

    addRowToMeleeMenu(flight->menu, fl_flight_env);
    addRowToMeleeMenu(flight->menu, fl_flight_perf);
    flight->menuEntryForInvertY = addRowToMeleeMenu( flight->menu, flight->inverty?fl_flight_invertY1_env:fl_flight_invertY0_env );
    addRowToMeleeMenu(flight->menu, str_quit);
    addRowToMeleeMenu(flight->menu, str_high_scores);
}

/**
 * Called when flight is exited
 */
static void flightExitMode(void)
{
    deinitMeleeMenu(flight->menu);
    freeFont(&flight->meleeMenuFont);
    freeFont(&flight->radiostars);
    freeFont(&flight->ibm);
    if( flight->environment )
    {
        free( flight->environment );
    }
    free(flight);
}

/**
 * TODO
 *
 * @param menuItem
 */
static void flightMenuCb(const char* menuItem)
{
    
    if( fl_flight_perf == menuItem )
    {
        flightStartGame(FLIGHT_PERFTEST);
    }
    else if (fl_flight_env == menuItem)
    {
        flightStartGame(FLIGHT_GAME);
    }
    else if ( fl_flight_invertY0_env == menuItem )
    {
        // XXX TODO SAVE DEFAULT FOR FLIGHT DATA
        flight->inverty = 1;
        flight->menu->rows[flight->menuEntryForInvertY] = fl_flight_invertY1_env;
    }
    else if ( fl_flight_invertY1_env == menuItem )
    {
        // XXX TODO SAVE DEFAULT FOR FLIGHT DATA
        flight->inverty = 0;
        flight->menu->rows[flight->menuEntryForInvertY] = fl_flight_invertY0_env;
    }
    else if ( str_high_scores == menuItem )
    {
        flight->mode = FLIGHT_SHOW_HIGH_SCORES;
    }
    else if (str_quit == menuItem)
    {
        switchToSwadgeMode(&modeMainMenu);
    }
}

static void flightEndGame()
{
    if( flightTimeHighScorePlace( flight->wintime, flight->beans == MAX_BEANS ) < NUM_FLIGHTSIM_TOP_SCORES )
    {
        flight->mode = FLIGHT_HIGH_SCORE_ENTRY;
        textEntryStart( flight->disp, FLIGHT_HIGH_SCORE_NAME_LEN+1, flight->highScoreNameBuffer );
    }
    else
    {
        flight->mode = FLIGHT_MENU;
    }
    //called when ending animation complete.
}

static void flightLEDAnimate( flLEDAnimation anim )
{
    flight->ledAnimation = anim;
    flight->ledAnimationTime = 0;
}

static void flightUpdateLEDs(flight_t * tflight)
{
    led_t leds[NUM_LEDS] = {{0}};

    uint8_t        ledAnimationTime = tflight->ledAnimationTime++;

    switch( tflight->ledAnimation )
    {
    default:
    case FLIGHT_LED_NONE:
        tflight->ledAnimationTime = 0;
        break;
    case FLIGHT_LED_ENDING:
        leds[0] = SafeEHSVtoHEXhelper(ledAnimationTime*4+0, 255, 2200-10*ledAnimationTime, 1 );
        leds[1] = SafeEHSVtoHEXhelper(ledAnimationTime*4+32, 255, 2200-10*ledAnimationTime, 1 );
        leds[2] = SafeEHSVtoHEXhelper(ledAnimationTime*4+64, 255, 2200-10*ledAnimationTime, 1 );
        leds[3] = SafeEHSVtoHEXhelper(ledAnimationTime*4+96, 255, 2200-10*ledAnimationTime, 1 );
        leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*4+128, 255, 2200-10*ledAnimationTime, 1 );
        leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*4+160, 255, 2200-10*ledAnimationTime, 1 );
        leds[6] = SafeEHSVtoHEXhelper(ledAnimationTime*4+192, 255, 2200-10*ledAnimationTime, 1 );
        leds[7] = SafeEHSVtoHEXhelper(ledAnimationTime*4+224, 255, 2200-10*ledAnimationTime, 1 );
        if( ledAnimationTime == 255 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_GAME_START:
    case FLIGHT_LED_DONUT:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(ledAnimationTime*8+0, 255, 200-10*ledAnimationTime, 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(ledAnimationTime*8+60, 255, 200-10*ledAnimationTime, 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*8+120, 255, 200-10*ledAnimationTime, 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*8+180, 255, 200-10*ledAnimationTime, 1 );
        if( ledAnimationTime == 30 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_MENU_TICK:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-2), 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-6), 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-10), 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-14), 1 );
        if( ledAnimationTime == 50 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_BEAN:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-2), 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-6), 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-10), 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-14), 1 );
        if( ledAnimationTime == 30 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    }

    setLeds(leds, sizeof(leds));
}

/**
 * TODO
 *
 * @param type
 * @param type
 * @param difficulty
 */
static void flightStartGame( flightModeScreen mode )
{
    flight->mode = mode;
    flight->frames = 0;


    flight->ondonut = 0; //Set to 14 to b-line it to the end for testing.
    flight->beans = 0; //Set to MAX_BEANS for 100% instant.
    flight->timeOfStart = (uint32_t)esp_timer_get_time();//-1000000*190; (Do this to force extra coursetime)
    flight->timeGot100Percent = 0;
    flight->wintime = 0;
    flight->speed = 0;

    //Starting location/orientation
    flight->planeloc[0] = 24*48;
    flight->planeloc[1] = 18*48; //Start pos * 48 since 48 is the fixed scale.
    flight->planeloc[2] = 60*48;
    
    flight->planeloc_fine[0] = flight->planeloc[0] << FLIGHT_SPEED_DEC;
    flight->planeloc_fine[1] = flight->planeloc[1] << FLIGHT_SPEED_DEC;
    flight->planeloc_fine[2] = flight->planeloc[2] << FLIGHT_SPEED_DEC;
    
    flight->hpr[0] = 2061;
    flight->hpr[1] = 190;
    flight->hpr[2] = 0;
    flight->pitchmoment = 0;
    flight->yawmoment = 0;

    memset(flight->beangotmask, 0, sizeof( flight->beangotmask) );

    flightLEDAnimate( FLIGHT_LED_GAME_START );
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void flightUpdate(void* arg __attribute__((unused)))
{
    display_t * disp = flight->disp;
    static const char * EnglishNumberSuffix[] = { "st", "nd", "rd", "th" };
    switch(flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
               drawMeleeMenu(flight->disp, flight->menu);
            break;
        }
        case FLIGHT_PERFTEST:
        case FLIGHT_GAME:
        {
            // Increment the frame count
            flight->frames++;
            flightGameUpdate( flight );
            break;
        }
        case FLIGHT_GAME_OVER:
        {
            flight->frames++;
            flightGameUpdate( flight );
            if( flight->frames > 200 ) flight->frames = 200; //Keep it at 200, so we can click any button to continue.
            break;
        }
        case FLIGHT_SHOW_HIGH_SCORES:
        {
            fillDisplayArea( disp, 0, 0, disp->w, disp->h, CNDRAW_BLACK );

            char buffer[32];
            flightSimSaveData_t * sd = getFlightSaveData();
            int line;

            drawText( flight->disp, &flight->ibm, CNDRAW_WHITE, "ANY %", 47+20, 30 );
            drawText( flight->disp, &flight->ibm, CNDRAW_WHITE, "100 %", 166+20, 30 );

            for( line = 0; line < NUM_FLIGHTSIM_TOP_SCORES; line++ )
            {
                int anyp = 0;
                snprintf( buffer, sizeof(buffer), "%d%s", line+1, EnglishNumberSuffix[line] );
                drawText( flight->disp, &flight->radiostars, CNDRAW_WHITE, buffer, 4, (line+1)*20+30 );

                for( anyp = 0; anyp < 2; anyp++ )
                {
                    int cs = sd->timeCentiseconds[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
                    char * name = sd->displayName[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
                    char namebuff[FLIGHT_HIGH_SCORE_NAME_LEN+1];    //Force pad of null.
                    memcpy( namebuff, name, FLIGHT_HIGH_SCORE_NAME_LEN );
                    namebuff[FLIGHT_HIGH_SCORE_NAME_LEN] = 0;
                    snprintf( buffer, sizeof(buffer), "%4s %3d.%02d", namebuff, cs/100,cs%100 );
                    drawText( flight->disp, &flight->radiostars, CNDRAW_WHITE, buffer, -3 + (anyp?166:47), (line+1)*20+30 );
                }
            }
            break;
        }
        case FLIGHT_HIGH_SCORE_ENTRY:
        {
            int place = flightTimeHighScorePlace( flight->wintime, flight->beans >= MAX_BEANS );
            textEntryDraw();

            char placeStr[32] = {0};
            snprintf(placeStr, sizeof(placeStr), "%d%s %s", place + 1, EnglishNumberSuffix[place],
                (flight->beans == MAX_BEANS)?"100%":"ANY%" );
            int16_t width = textWidth(&flight->ibm, placeStr);
            drawText( flight->disp, &flight->ibm, CNDRAW_WHITE, placeStr, (flight->disp->w - width) / 2, 2);
            break;
        }
    }

    flightUpdateLEDs( flight );

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void tdIdentity( int16_t * matrix );
void Perspective( int fovx, int aspect, int zNear, int zFar, int16_t * out );
int LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 );
void SetupMatrix( void );
void tdMultiply( int16_t * fin1, int16_t * fin2, int16_t * fout );
void tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z );
void tdScale( int16_t * f, int16_t x, int16_t y, int16_t z );
void td4Transform( int16_t * pin, int16_t * f, int16_t * pout );
void tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z );
void Draw3DSegment( display_t * disp, const int16_t * c1, const int16_t * c2 );
uint16_t tdSQRT( uint32_t inval );
int16_t tdDist( const int16_t * a, const int16_t * b );


//From https://github.com/cnlohr/channel3/blob/master/user/3d.c

int16_t ModelviewMatrix[16];
int16_t ProjectionMatrix[16];

uint16_t tdSQRT( uint32_t inval )
{
    uint32_t res = 0;
    uint32_t one = 1UL << 30;
    while (one > inval)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (inval >= res + one)
        {
            inval = inval - (res + one);
            res = res +  (one<<1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

int16_t tdDist( const int16_t * a, const int16_t * b )
{
    int32_t dx = a[0] - b[0];
    int32_t dy = a[1] - b[1];
    int32_t dz = a[2] - b[2];
    return tdSQRT( dx*dx+dy*dy+dz*dz );
}

void tdIdentity( int16_t * matrix )
{
    matrix[0] = 256; matrix[1] = 0; matrix[2] = 0; matrix[3] = 0;
    matrix[4] = 0; matrix[5] = 256; matrix[6] = 0; matrix[7] = 0;
    matrix[8] = 0; matrix[9] = 0; matrix[10] = 256; matrix[11] = 0;
    matrix[12] = 0; matrix[13] = 0; matrix[14] = 0; matrix[15] = 256;
}


#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15

/*
int vTransform( flight_t * flightsim, int16_t * xformed, const int16_t * input )
{
    int16_t x = input[0];
    int16_t y = input[1];
    int16_t z = input[2];
    if( x == 0 && y == 0 && z == 0 ) return 0;

    x -= flightsim->planeloc[0];
    y -= flightsim->planeloc[1];
    z -= flightsim->planeloc[2];

    xformed[0] = (x>>7) + 20;
    xformed[1] = (y>>7) + 20;
    xformed[2] = (z>>7) + 20;
    return 1;
}
*/

void Perspective( int fovx, int aspect, int zNear, int zFar, int16_t * out )
{
    int16_t f = fovx;
    out[0] = f*256/aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0;
    out[10] = 256*(zFar + zNear)/(zNear - zFar);
    out[11] = 2*zFar*zNear  /(zNear - zFar);
    out[12] = 0; out[13] = 0; out[14] = -256; out[15] = 0;
}


void SetupMatrix( void )
{
    tdIdentity( ProjectionMatrix );
    tdIdentity( ModelviewMatrix );

    Perspective( 1200, 128 /* 0.5 */, 50, 8192, ProjectionMatrix );
}

void tdMultiply( int16_t * fin1, int16_t * fin2, int16_t * fout )
{
    int16_t fotmp[16];

    fotmp[m00] = ((int32_t)fin1[m00] * (int32_t)fin2[m00] + (int32_t)fin1[m01] * (int32_t)fin2[m10] + (int32_t)fin1[m02] * (int32_t)fin2[m20] + (int32_t)fin1[m03] * (int32_t)fin2[m30])>>8;
    fotmp[m01] = ((int32_t)fin1[m00] * (int32_t)fin2[m01] + (int32_t)fin1[m01] * (int32_t)fin2[m11] + (int32_t)fin1[m02] * (int32_t)fin2[m21] + (int32_t)fin1[m03] * (int32_t)fin2[m31])>>8;
    fotmp[m02] = ((int32_t)fin1[m00] * (int32_t)fin2[m02] + (int32_t)fin1[m01] * (int32_t)fin2[m12] + (int32_t)fin1[m02] * (int32_t)fin2[m22] + (int32_t)fin1[m03] * (int32_t)fin2[m32])>>8;
    fotmp[m03] = ((int32_t)fin1[m00] * (int32_t)fin2[m03] + (int32_t)fin1[m01] * (int32_t)fin2[m13] + (int32_t)fin1[m02] * (int32_t)fin2[m23] + (int32_t)fin1[m03] * (int32_t)fin2[m33])>>8;

    fotmp[m10] = ((int32_t)fin1[m10] * (int32_t)fin2[m00] + (int32_t)fin1[m11] * (int32_t)fin2[m10] + (int32_t)fin1[m12] * (int32_t)fin2[m20] + (int32_t)fin1[m13] * (int32_t)fin2[m30])>>8;
    fotmp[m11] = ((int32_t)fin1[m10] * (int32_t)fin2[m01] + (int32_t)fin1[m11] * (int32_t)fin2[m11] + (int32_t)fin1[m12] * (int32_t)fin2[m21] + (int32_t)fin1[m13] * (int32_t)fin2[m31])>>8;
    fotmp[m12] = ((int32_t)fin1[m10] * (int32_t)fin2[m02] + (int32_t)fin1[m11] * (int32_t)fin2[m12] + (int32_t)fin1[m12] * (int32_t)fin2[m22] + (int32_t)fin1[m13] * (int32_t)fin2[m32])>>8;
    fotmp[m13] = ((int32_t)fin1[m10] * (int32_t)fin2[m03] + (int32_t)fin1[m11] * (int32_t)fin2[m13] + (int32_t)fin1[m12] * (int32_t)fin2[m23] + (int32_t)fin1[m13] * (int32_t)fin2[m33])>>8;

    fotmp[m20] = ((int32_t)fin1[m20] * (int32_t)fin2[m00] + (int32_t)fin1[m21] * (int32_t)fin2[m10] + (int32_t)fin1[m22] * (int32_t)fin2[m20] + (int32_t)fin1[m23] * (int32_t)fin2[m30])>>8;
    fotmp[m21] = ((int32_t)fin1[m20] * (int32_t)fin2[m01] + (int32_t)fin1[m21] * (int32_t)fin2[m11] + (int32_t)fin1[m22] * (int32_t)fin2[m21] + (int32_t)fin1[m23] * (int32_t)fin2[m31])>>8;
    fotmp[m22] = ((int32_t)fin1[m20] * (int32_t)fin2[m02] + (int32_t)fin1[m21] * (int32_t)fin2[m12] + (int32_t)fin1[m22] * (int32_t)fin2[m22] + (int32_t)fin1[m23] * (int32_t)fin2[m32])>>8;
    fotmp[m23] = ((int32_t)fin1[m20] * (int32_t)fin2[m03] + (int32_t)fin1[m21] * (int32_t)fin2[m13] + (int32_t)fin1[m22] * (int32_t)fin2[m23] + (int32_t)fin1[m23] * (int32_t)fin2[m33])>>8;

    fotmp[m30] = ((int32_t)fin1[m30] * (int32_t)fin2[m00] + (int32_t)fin1[m31] * (int32_t)fin2[m10] + (int32_t)fin1[m32] * (int32_t)fin2[m20] + (int32_t)fin1[m33] * (int32_t)fin2[m30])>>8;
    fotmp[m31] = ((int32_t)fin1[m30] * (int32_t)fin2[m01] + (int32_t)fin1[m31] * (int32_t)fin2[m11] + (int32_t)fin1[m32] * (int32_t)fin2[m21] + (int32_t)fin1[m33] * (int32_t)fin2[m31])>>8;
    fotmp[m32] = ((int32_t)fin1[m30] * (int32_t)fin2[m02] + (int32_t)fin1[m31] * (int32_t)fin2[m12] + (int32_t)fin1[m32] * (int32_t)fin2[m22] + (int32_t)fin1[m33] * (int32_t)fin2[m32])>>8;
    fotmp[m33] = ((int32_t)fin1[m30] * (int32_t)fin2[m03] + (int32_t)fin1[m31] * (int32_t)fin2[m13] + (int32_t)fin1[m32] * (int32_t)fin2[m23] + (int32_t)fin1[m33] * (int32_t)fin2[m33])>>8;

    memcpy( fout, fotmp, sizeof( fotmp ) );
}


void tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];
    tdIdentity(ftmp);
    ftmp[m03] += x;
    ftmp[m13] += y;
    ftmp[m23] += z;
    tdMultiply( f, ftmp, f );
}

void tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];

    //x,y,z must be negated for some reason
    const int16_t * stable = sin1024;
    int16_t cx = stable[((x>=270)?(x-270):(x+90))]>>2;
    int16_t sx = stable[x]>>2;
    int16_t cy = stable[((y>=270)?(y-270):(y+90))]>>2;
    int16_t sy = stable[y]>>2;
    int16_t cz = stable[((z>=270)?(z-270):(z+90))]>>2;
    int16_t sz = stable[z]>>2;

    //Row major
    //manually transposed
    ftmp[m00] = (cy*cz)>>8;
    ftmp[m10] = ((((sx*sy)>>8)*cz)-(cx*sz))>>8;
    ftmp[m20] = ((((cx*sy)>>8)*cz)+(sx*sz))>>8;
    ftmp[m30] = 0;

    ftmp[m01] = (cy*sz)>>8;
    ftmp[m11] = ((((sx*sy)>>8)*sz)+(cx*cz))>>8;
    ftmp[m21] = ((((cx*sy)>>8)*sz)-(sx*cz))>>8;
    ftmp[m31] = 0;

    ftmp[m02] = -sy;
    ftmp[m12] = (sx*cy)>>8;
    ftmp[m22] = (cx*cy)>>8;
    ftmp[m32] = 0;

    ftmp[m03] = 0;
    ftmp[m13] = 0;
    ftmp[m23] = 0;
    ftmp[m33] = 1;

    tdMultiply( f, ftmp, f );
}

void tdScale( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    f[m00] = (f[m00] * x)>>8;
    f[m01] = (f[m01] * x)>>8;
    f[m02] = (f[m02] * x)>>8;
//    f[m03] = (f[m03] * x)>>8;

    f[m10] = (f[m10] * y)>>8;
    f[m11] = (f[m11] * y)>>8;
    f[m12] = (f[m12] * y)>>8;
//    f[m13] = (f[m13] * y)>>8;

    f[m20] = (f[m20] * z)>>8;
    f[m21] = (f[m21] * z)>>8;
    f[m22] = (f[m22] * z)>>8;
//    f[m23] = (f[m23] * z)>>8;
}

void td4Transform( int16_t * pin, int16_t * f, int16_t * pout )
{
    int16_t ptmp[3];
    ptmp[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03])>>8;
    ptmp[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13])>>8;
    ptmp[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23])>>8;
    pout[3] = (pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33])>>8;
    pout[0] = ptmp[0];
    pout[1] = ptmp[1];
    pout[2] = ptmp[2];
}


int LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 )
{
    int16_t tmppt[4] = { coords_3v[0], coords_3v[1], coords_3v[2], 256 };
    td4Transform( tmppt, ModelviewMatrix, tmppt );
    td4Transform( tmppt, ProjectionMatrix, tmppt );
    if( tmppt[3] >= -4 ) { return -1; }
    int calcx = ((256 * tmppt[0] / tmppt[3])/16+(TFT_WIDTH/2));
    int calcy = ((256 * tmppt[1] / tmppt[3])/8+(TFT_HEIGHT/2));
    if( calcx < -16000 || calcx > 16000 || calcy < -16000 || calcy > 16000 ) return -2;
    *o1 = calcx;
    *o2 = calcy;
    return 0;
}

// Note: Function unused.  For illustration purposes.
void Draw3DSegment( display_t * disp, const int16_t * c1, const int16_t * c2 )
{
    int16_t sx0, sy0, sx1, sy1;
    if( LocalToScreenspace( c1, &sx0, &sy0 ) ||
        LocalToScreenspace( c2, &sx1, &sy1 ) ) return;

    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
    speedyLine( disp, sx0, sy0, sx1, sy1, CNDRAW_WHITE );
    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 );

    //plotLine( sx0, sy0, sx1, sy1, CNDRAW_WHITE );
}

int tdModelVisibilitycheck( const tdModel * m )
{

    //For computing visibility check
    int16_t tmppt[4] = { m->center[0], m->center[1], m->center[2], 256 }; //No multiplier seems to work right here.
    td4Transform( tmppt, ModelviewMatrix, tmppt );
    td4Transform( tmppt, ProjectionMatrix, tmppt );
    if( tmppt[3] < -2 )
    {
        int scx = ((256 * tmppt[0] / tmppt[3])/16+(TFT_WIDTH/2));
        int scy = ((256 * tmppt[1] / tmppt[3])/8+(TFT_HEIGHT/2));
       // int scz = ((65536 * tmppt[2] / tmppt[3]));
        int scd = ((-256 * 2 * m->radius / tmppt[3])/8);
        scd += 3; //Slack
        if( scx < -scd || scy < -scd || scx >= TFT_WIDTH + scd || scy >= TFT_HEIGHT + scd )
        {
            return -1;
        }
        else
        {
            return -tmppt[3];
        }
    }
    else
    {
        return -2;
    }
}

void tdDrawModel( display_t * disp, const tdModel * m )
{
    int i;

    int nrv = m->nrvertnums;
    int nri = m->nrfaces*m->indices_per_face;
    const int16_t * verticesmark = (const int16_t*)&m->indices_and_vertices[nri];

    if( tdModelVisibilitycheck( m ) < 0 )
    {
        return;
    }


    //This looks a little odd, but what we're doing is caching our vertex computations
    //so we don't have to re-compute every time round.
    //f( "%d\n", nrv );
    int16_t cached_verts[nrv];

    for( i = 0; i < nrv; i+=3 )
    {
        int16_t * cv1 = &cached_verts[i];
        if( LocalToScreenspace( &verticesmark[i], cv1, cv1+1 ) )
            cv1[2] = 2;
        else
            cv1[2] = 1;
    }

    if( m->indices_per_face == 2 )
    {
        for( i = 0; i < nri; i+=2 )
        {
            int i1 = m->indices_and_vertices[i];
            int i2 = m->indices_and_vertices[i+1];
            int16_t * cv1 = &cached_verts[i1];
            int16_t * cv2 = &cached_verts[i2];

            if( cv1[2] != 2 && cv2[2] != 2 )
            {
                speedyLine( disp, cv1[0], cv1[1], cv2[0], cv2[1], renderlinecolor );
            }
        }
    }
    else if( m->indices_per_face == 3 )
    {
        for( i = 0; i < nri; i+=3 )
        {
            int i1 = m->indices_and_vertices[i];
            int i2 = m->indices_and_vertices[i+1];
            int i3 = m->indices_and_vertices[i+2];
            int16_t * cv1 = &cached_verts[i1];
            int16_t * cv2 = &cached_verts[i2];
            int16_t * cv3 = &cached_verts[i3];
            //printf( "%d/%d/%d  %d %d %d\n", i1, i2, i3, cv1[2], cv2[2], cv3[2] );

            if( cv1[2] != 2 && cv2[2] != 2 && cv3[2] != 2 )
            {

                //Perform screen-space cross product to determine if we're looking at a backface.
                int Ux = cv3[0] - cv1[0];
                int Uy = cv3[1] - cv1[1];
                int Vx = cv2[0] - cv1[0];
                int Vy = cv2[1] - cv1[1];
                if( Ux*Vy-Uy*Vx >= 0 )
                    outlineTriangle( disp, cv1[0], cv1[1], cv2[0], cv2[1], cv3[0], cv3[1], CNDRAW_BLACK, CNDRAW_WHITE );
            }
        }
    }
}


struct ModelRangePair
{
    const tdModel * model;
    int       mrange;
};

//Do not put this in icache.
int mdlctcmp( const void * va, const void * vb );
int mdlctcmp( const void * va, const void * vb )
{
    const struct ModelRangePair * a = (const struct ModelRangePair *)va;
    const struct ModelRangePair * b = (const struct ModelRangePair *)vb;
    return b->mrange - a->mrange;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void flightRender()
{

#ifndef EMU
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('R');
#endif
    flightUpdate( 0 );

    flight_t * tflight = flight;
    display_t * disp = tflight->disp;
    tflight->tframes++;
    if( tflight->mode != FLIGHT_GAME && tflight->mode != FLIGHT_GAME_OVER && tflight->mode != FLIGHT_PERFTEST ) return;

    // First clear the OLED

    SetupMatrix();


    static uint32_t fps;
    static uint32_t nowframes;
    static uint32_t lasttimeframe;
    if( nowframes == 0 )
    {
        lasttimeframe = esp_timer_get_time();
        nowframes = 1;
    }
    if( ((uint32_t)esp_timer_get_time()) - lasttimeframe > 1000000 )
    {
        fps = nowframes;
        nowframes = 1;
        lasttimeframe+=1000000;
    }
    nowframes++;

#ifndef EMU
    uint32_t start = getCycleCount();
//  if( flight->mode == FLIGHT_PERFTEST )
//        portDISABLE_INTERRUPTS();
#ifndef EMU
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('1');
#endif

#else
    cndrawPerfcounter = 0;
    uint32_t start = cndrawPerfcounter;
#endif


    tdRotateEA( ProjectionMatrix, tflight->hpr[1]/11, tflight->hpr[0]/11, 0 );
    tdTranslate( ModelviewMatrix, -tflight->planeloc[0], -tflight->planeloc[1], -tflight->planeloc[2] );

    struct ModelRangePair mrp[tflight->enviromodels];
    int mdlct = 0;

/////////////////////////////////////////////////////////////////////////////////////////
////GAME LOGIC GOES HERE (FOR COLLISIONS/////////////////////////////////////////////////

    int i;
    for( i = 0; i < tflight->enviromodels;i++ )
    {
        const tdModel * m = tflight->environment[i];

        int label = m->label;
        int draw = 1;
        if( label )
        {
            draw = 0;
            if( label >= 100 && (label - 100) == tflight->ondonut )
            {
                draw = 1;
                if( tdDist( tflight->planeloc, m->center ) < 130 )
                {
                    flightLEDAnimate( FLIGHT_LED_DONUT );
                    tflight->ondonut++;
                }
            }
            //bean? 1000... groupings of 8.
            int beansec = ((label-1000)/10);

            if( label >= 1000 && ( beansec == tflight->ondonut || beansec == (tflight->ondonut-1) || beansec == (tflight->ondonut+1)) )
            {
                if( ! (tflight->beangotmask[beansec] & (1<<((label-1000)%10))) )
                {
                    draw = 1;

                    if( tdDist( tflight->planeloc, m->center ) < 100 )
                    {
                        tflight->beans++;
                        tflight->beangotmask[beansec] |= (1<<((label-1000)%10));
                        flightLEDAnimate( FLIGHT_LED_BEAN );
                    }
                }
            }
            if( label == 999 ) //gazebo
            {
                draw = 1;
                if( flight->mode != FLIGHT_GAME_OVER && tdDist( tflight->planeloc, m->center ) < 200 && tflight->ondonut == MAX_DONUTS)
                {
                    flightLEDAnimate( FLIGHT_LED_ENDING );
                    tflight->frames = 0;
                    tflight->wintime = ((uint32_t)esp_timer_get_time() - tflight->timeOfStart)/10000;
                    tflight->mode = FLIGHT_GAME_OVER;
                }
            }
        }

        if( draw == 0 ) continue;

        int r = tdModelVisibilitycheck( m );
        if( r < 0 ) continue;
        mrp[mdlct].model = m;
        mrp[mdlct].mrange = r;
        mdlct++;
    }

#ifndef EMU
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('2');
    uint32_t mid1 = getCycleCount();
#else
    uint32_t mid1 = cndrawPerfcounter;
#endif

    //Painter's algorithm
    qsort( mrp, mdlct, sizeof( struct ModelRangePair ), mdlctcmp );

#ifndef EMU
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('3');
    uint32_t mid2 = getCycleCount();
#else
    uint32_t mid2 = cndrawPerfcounter;
#endif

    for( i = 0; i < mdlct; i++ )
    {
        const tdModel * m = mrp[i].model;
        int label = m->label;
        int draw = 1;
        if( label )
        {
            draw = 0;
            if( label >= 100 && label < 999 )
            {
                draw = 2; //All donuts flash on.
            }
            if( label >= 1000 )
            {
                draw = 3; //All beans flash-invert
            }
            if( label == 999 ) //gazebo
            {
                draw = (tflight->ondonut==MAX_DONUTS)?2:1; //flash on last donut.
            }
        }

        //XXX TODO:
        // Flash light when you get a bean or a ring.
        // Do laptiming per ring for fastest time.
        // Fix time counting and presentation

        //draw = 0 = invisible
        //draw = 1 = regular
        //draw = 2 = flashing
        //draw = 3 = other flashing
        if( draw == 1 )
            tdDrawModel( disp, m );
        else if( draw == 2 || draw == 3 )
        {
            if( draw == 2 )
            {
                // Originally, (tflight->frames&1)?CNDRAW_WHITE:CNDRAW_BLACK;
                // Now, let's go buck wild.
                renderlinecolor = (tflight->frames*7)&127;
            }
            if( draw == 3 )
            {
                //renderlinecolor = (tflight->frames&1)?CNDRAW_BLACK:CNDRAW_WHITE;
                renderlinecolor = ((tflight->frames+i))&127;
            }
            tdDrawModel( disp, m );
            renderlinecolor = CNDRAW_WHITE;
        }
    }


#ifndef EMU
        //OVERCLOCK_SECTION_DISABLE();
        //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 );
#endif
#ifndef EMU
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('4');
    uint32_t stop = getCycleCount();
//    if( flight->mode == FLIGHT_PERFTEST )
//        portENABLE_INTERRUPTS();
#else
    uint32_t stop = cndrawPerfcounter;
#endif


    if( flight->mode == FLIGHT_GAME || tflight->mode == FLIGHT_PERFTEST )
    {
        char framesStr[32] = {0};

        fillDisplayArea(disp, 0, 0, TFT_WIDTH, flight->radiostars.h + 1, CNDRAW_BLACK);

        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        int elapsed = ((uint32_t)esp_timer_get_time()-tflight->timeOfStart)/10000;

        snprintf(framesStr, sizeof(framesStr), "%d/%d, %d", tflight->ondonut, MAX_DONUTS, tflight->beans );
        int16_t width = textWidth(&flight->radiostars, framesStr);
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 50, 0 );

        snprintf(framesStr, sizeof(framesStr), "%d.%02d", elapsed/100, elapsed%100 );
        width = textWidth(&flight->radiostars, framesStr);
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, 0 );
        
        if( flight->mode == FLIGHT_PERFTEST )
        {
            snprintf(framesStr, sizeof(framesStr), "%d", mid1 - start );
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h+1 );
            snprintf(framesStr, sizeof(framesStr), "%d", mid2 - mid1 );
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*2+2 );
            snprintf(framesStr, sizeof(framesStr), "%d", stop - mid2 );
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*3+3 );
            snprintf(framesStr, sizeof(framesStr), "%d", fps );
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*4+4 );
        }

        snprintf(framesStr, sizeof(framesStr), "%d", tflight->speed);
        width = textWidth(&flight->radiostars, framesStr);
        fillDisplayArea(disp, TFT_WIDTH - width-50, TFT_HEIGHT - flight->radiostars.h - 1, TFT_WIDTH, TFT_HEIGHT, CNDRAW_BLACK);
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - width + 1-50, TFT_HEIGHT - flight->radiostars.h );

        if(flight->oob)
        {
            width = textWidth(&flight->ibm, "TURN AROUND");
            drawText(disp, &flight->ibm, PROMPT_COLOR, "TURN AROUND", (TFT_WIDTH - width) / 2, (TFT_HEIGHT - flight->ibm.h) / 2);
        }

        // Draw crosshairs.
        int centerx = TFT_WIDTH/2;
        int centery = TFT_HEIGHT/2;
        speedyLine(disp, centerx-4, centery, centerx+4, centery, CROSSHAIR_COLOR );
        speedyLine(disp, centerx, centery-4, centerx, centery+4, CROSSHAIR_COLOR );
    }
    else
    {
        char framesStr[32] = {0};
        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        snprintf(framesStr, sizeof(framesStr), "YOU  WIN:" );
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 20+75, 50);
        snprintf(framesStr, sizeof(framesStr), "TIME:%d.%02d", tflight->wintime/100,tflight->wintime%100 );
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, ((tflight->wintime>10000)?14:20)+75, 18+50);
        snprintf(framesStr, sizeof(framesStr), "BEANS:%2d",tflight->beans );
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 20+75, 36+50);
    }

    if( tflight->beans >= MAX_BEANS )
    {
        if( tflight->timeGot100Percent == 0 )
            tflight->timeGot100Percent = ((uint32_t)esp_timer_get_time() - tflight->timeOfStart);

        int crazy = (((uint32_t)esp_timer_get_time() - tflight->timeOfStart)-tflight->timeGot100Percent) < 3000000;
        drawText( disp, &flight->ibm, crazy?( tflight->tframes * 9 ):PROMPT_COLOR, "100% 100% 100%", 10+75, 52+50 );
    }

    //If perf test, force full frame refresh
    //Otherwise, don't force full-screen refresh

#ifndef EMU    
    if( flight->mode == FLIGHT_PERFTEST ) uart_tx_one_char('5');
#endif
    return;
}

static void flightGameUpdate( flight_t * tflight )
{
    uint8_t bs = tflight->buttonState;

    int dpitch = 0;
    int dyaw = 0;

    const int flight_min_speed = (flight->mode==FLIGHT_PERFTEST)?0:FLIGHT_MIN_SPEED;

    //If we're at the ending screen and the user presses a button end game.
    if( tflight->mode == FLIGHT_GAME_OVER && ( bs & 16 ) && flight->frames > 199 )
    {
        flightEndGame();
    }

    if( tflight->mode == FLIGHT_GAME || tflight->mode == FLIGHT_PERFTEST )
    {
        if( bs & 4 ) dpitch += THRUSTER_ACCEL;
        if( bs & 8 ) dpitch -= THRUSTER_ACCEL;
        if( bs & 1 ) dyaw += THRUSTER_ACCEL;
        if( bs & 2 ) dyaw -= THRUSTER_ACCEL;

        if( tflight->inverty ) dyaw *= -1;

        if( dpitch )
        {
            tflight->pitchmoment += dpitch;
            if( tflight->pitchmoment > THRUSTER_MAX ) tflight->pitchmoment = THRUSTER_MAX;
            if( tflight->pitchmoment < -THRUSTER_MAX ) tflight->pitchmoment = -THRUSTER_MAX;
        }
        else
        {
            if( tflight->pitchmoment > 0 ) tflight->pitchmoment-=THRUSTER_DECAY;
            if( tflight->pitchmoment < 0 ) tflight->pitchmoment+=THRUSTER_DECAY;
        }

        if( dyaw )
        {
            tflight->yawmoment += dyaw;
            if( tflight->yawmoment > THRUSTER_MAX ) tflight->yawmoment = THRUSTER_MAX;
            if( tflight->yawmoment < -THRUSTER_MAX ) tflight->yawmoment = -THRUSTER_MAX;
        }
        else
        {
            if( tflight->yawmoment > 0 ) tflight->yawmoment-=THRUSTER_DECAY;
            if( tflight->yawmoment < 0 ) tflight->yawmoment+=THRUSTER_DECAY;
        }

        tflight->hpr[0] += tflight->pitchmoment;
        tflight->hpr[1] += tflight->yawmoment;
        
        if( tflight->hpr[0] > 3960 ) tflight->hpr[0] -= 3960;
        if( tflight->hpr[0] < 0 ) tflight->hpr[0] += 3960;
        if( tflight->hpr[1] > 3960 ) tflight->hpr[1] -= 3960;
        if( tflight->hpr[1] < 0 ) tflight->hpr[1] += 3960;

        if( bs & 16 ) tflight->speed++;
        else tflight->speed--;
        if( tflight->speed < flight_min_speed ) tflight->speed = flight_min_speed;
        if( tflight->speed > FLIGHT_MAX_SPEED ) tflight->speed = FLIGHT_MAX_SPEED;
    }

    //If game over, just keep status quo.

    flight->planeloc_fine[0] += (tflight->speed * getSin1024( tflight->hpr[0]/11 ) );
    flight->planeloc_fine[2] += (tflight->speed * getCos1024( tflight->hpr[0]/11 ) );
    flight->planeloc_fine[1] -= (tflight->speed * getSin1024( tflight->hpr[1]/11 ) );

    tflight->planeloc[0] = flight->planeloc_fine[0]>>FLIGHT_SPEED_DEC;
    tflight->planeloc[1] = flight->planeloc_fine[1]>>FLIGHT_SPEED_DEC;
    tflight->planeloc[2] = flight->planeloc_fine[2]>>FLIGHT_SPEED_DEC;

    // Bound the area
    tflight->oob = false;
    if(tflight->planeloc[0] < -1900)
    {
        tflight->planeloc[0] = -1900;
        tflight->oob = true;
    }
    else if(tflight->planeloc[0] > 1900)
    {
        tflight->planeloc[0] = 1900;
        tflight->oob = true;
    }

    if(tflight->planeloc[1] < -800)
    {
        tflight->planeloc[1] = -800;
        tflight->oob = true;
    }
    else if(tflight->planeloc[1] > 3500)
    {
        tflight->planeloc[1] = 3500;
        tflight->oob = true;
    }

    if(tflight->planeloc[2] < -1300)
    {
        tflight->planeloc[2] = -1300;
        tflight->oob = true;
    }
    else if(tflight->planeloc[2] > 3700)
    {
        tflight->planeloc[2] = 3700;
        tflight->oob = true;
    }
}

/**
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void flightButtonCallback( buttonEvt_t* evt )
{
    uint8_t state = evt->state;
    int button = evt->button;
    int down = evt->down;

    switch (flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
            if(down)
            {
                flightLEDAnimate( FLIGHT_LED_MENU_TICK );
                meleeMenuButton(flight->menu, evt->button);
                
                // If we are left-and-right on Y not inverted then we invert y or not.
                if( ( button & ( LEFT | RIGHT ) ) && flight->menu->selectedRow == flight->menuEntryForInvertY )
                {
                    flight->inverty ^= 1;
                    flight->menu->rows[flight->menuEntryForInvertY] = flight->inverty?fl_flight_invertY1_env:fl_flight_invertY0_env;

                    flightSimSaveData_t * sd = getFlightSaveData();
                    sd->flightInvertY = flight->inverty;
                    setFlightSaveData( sd );
                }
            }
            break;
        }
        case FLIGHT_SHOW_HIGH_SCORES:
        {
            if( down )
            {
                //Return to main mode.
                flight->mode = FLIGHT_MENU;
            }
            break;
        }
        case FLIGHT_HIGH_SCORE_ENTRY:
        {
            if( !textEntryInput( down, button ) )
            {
                //Actually insert high score.
                textEntryEnd();
                if( strlen( flight->highScoreNameBuffer ) )
                {
                    flightTimeHighScoreInsert(
                        flightTimeHighScorePlace( flight->wintime, flight->beans == MAX_BEANS ),
                        flight->beans == MAX_BEANS,
                        flight->highScoreNameBuffer,
                        flight->wintime );
                }
                flight->mode = FLIGHT_SHOW_HIGH_SCORES;
            }
            break;
        }
        case FLIGHT_GAME_OVER:
        case FLIGHT_PERFTEST:
        case FLIGHT_GAME:
        {
            flight->buttonState = state;
            break;
        }
    }
}




//Handling high scores.
/**
 *
 * @param wintime in centiseconds
 * @param whether this is a 100% run.
 * @return place in top score list.  If 4, means not in top score list.
 *
 */
static int flightTimeHighScorePlace( int wintime, bool is100percent )
{
    flightSimSaveData_t * sd = getFlightSaveData();
    int i;
    for( i = 0; i < NUM_FLIGHTSIM_TOP_SCORES; i++ )
    {
        int cs = sd->timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
        if( !cs || cs > wintime ) break;
    }
    return i;
}

/**
 *
 * @param which winning slot to place player into
 * @param whether this is a 100% run.
 * @param display name for player (truncated to
 * @param wintime in centiseconds
 *
 */
static void flightTimeHighScoreInsert( int insertplace, bool is100percent, char * name, int timeCentiseconds )
{
    if( insertplace >= NUM_FLIGHTSIM_TOP_SCORES || insertplace < 0 ) return;

    flightSimSaveData_t * sd = getFlightSaveData();
    int i;
    for( i = NUM_FLIGHTSIM_TOP_SCORES-1; i > insertplace; i-- )
    {
        memcpy( sd->displayName[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            sd->displayName[(i-1)+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            NUM_FLIGHTSIM_TOP_SCORES );
        sd->timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES] =
            sd->timeCentiseconds[i-1+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
    }
    int namelen = strlen( name );
    if( namelen > FLIGHT_HIGH_SCORE_NAME_LEN ) namelen = FLIGHT_HIGH_SCORE_NAME_LEN;
    memcpy( sd->displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
        name, namelen );

    //Zero pad if less than 4 chars.
    if( namelen < FLIGHT_HIGH_SCORE_NAME_LEN )
        sd->displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES][namelen] = 0;

    sd->timeCentiseconds[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES] = timeCentiseconds;
    setFlightSaveData( sd );
}

