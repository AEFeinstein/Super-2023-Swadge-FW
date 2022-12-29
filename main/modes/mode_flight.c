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

/*============================================================================
 * Defines
 *==========================================================================*/

// Thruster speed, etc.
#define THRUSTER_ACCEL   2
#define THRUSTER_MAX     42 //NOTE: THRUSTER_MAX must be divisble by THRUSTER_ACCEL
#define THRUSTER_DECAY   2
#define FLIGHT_SPEED_DEC 12
#define FLIGHT_MAX_SPEED 96
#define FLIGHT_MAX_SPEED_FREE 50
#define FLIGHT_MIN_SPEED 10

#define VIEWPORT_PERSPECTIVE 600
#define VIEWPORT_DIV  4
#define OOBMUX 1.3333  //based on 48 bams/m

#define BOOLET_SPEED_DIVISOR 11

//XXX TODO: Refactor - these should probably be unified.
#define MAXRINGS 15
#define MAX_DONUTS 14
#define MAX_BEANS 69

// Target 40 FPS.
#define DEFAULT_FRAMETIME 25000
#define CROSSHAIR_COLOR 200
#define BOOLET_COLOR 198 //Very bright yellow-orange
#define CNDRAW_BLACK 0
#define CNDRAW_WHITE 18 // actually greenish
#define PROMPT_COLOR 92
#define OTHER_PLAYER_COLOR 5
#define MAX_COLOR cTransparent

#define BOOLET_MAX_LIFETIME 8000000
#define BOOLET_HIT_DIST_SQUARED 2400 // 60x60

#define flightGetCourseTimeUs() ( flight->paused ? (flight->timeOfPause - flight->timeOfStart) : ((uint32_t)esp_timer_get_time() - flight->timeOfStart) )

#define TFT_WIDTH 280
#define TFT_HEIGHT 240

/*============================================================================
 * Structs, Enums
 *==========================================================================*/


typedef enum
{
    FLIGHT_MENU,
    FLIGHT_GAME,
    FLIGHT_GAME_OVER,
    FLIGHT_HIGH_SCORE_ENTRY,
    FLIGHT_SHOW_HIGH_SCORES,
    FLIGHT_FREEFLIGHT,
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

typedef struct
{
    const tdModel * model;
    int       mrange;
} modelRangePair_t;

typedef enum
{
    FLIGHT_LED_NONE,
    FLIGHT_LED_MENU_TICK,
    FLIGHT_LED_GAME_START,
    FLIGHT_LED_BEAN,
    FLIGHT_LED_DONUT,
    FLIGHT_LED_ENDING,
    FLIGHT_LED_GOT_HIT,
    FLIGHT_LED_DIED,
    FLIGHT_GOT_KILL,
} flLEDAnimation;

//////////////////////////////////////////////////////////////////////////////
// Multiplayer

#define FSNET_CODE_SERVER 0x73534653
#define FSNET_CODE_PEER 0x66534653

#define MAX_PEERS 103 // Best if it's a prime number.
#define BOOLETSPERPLAYER 4
#define MAX_BOOLETS (MAX_PEERS*BOOLETSPERPLAYER)
#define MAX_NETWORK_MODELS 81

typedef struct  // 32 bytes.
{
    uint32_t timeOffsetOfPeerFromNow;
    uint32_t timeOfUpdate;  // In our timestamp
    uint8_t  mac[6];

    // Not valid for server.
    int16_t posAt[3];
    int8_t  velAt[3];
    int8_t  rotAt[3];    // Right now, only HPR where R = 0
    uint8_t  basePeerFlags; // If zero, don't render.  Note flags&1 has reserved meaning locally for presence..  if flags & 2, render as dead.
    uint16_t  auxPeerFlags; // If dead, is ID of boolet which killed "me"
    uint8_t  framesDead;
    uint8_t  reqColor;
} multiplayerpeer_t;

typedef struct  // Rounds up to 16 bytes.
{
    uint32_t timeOfLaunch; // In our timestamp.
    int16_t launchLocation[3];
    int16_t launchRotation[2]; // Pitch and Yaw
    uint16_t flags;  // If 0, disabled.  Otherwise, is a randomized ID.
} boolet_t;

typedef struct
{
    uint32_t timeOfUpdate;
    uint32_t binencprop;      //Encoding starts at lsb.  First: # of bones, then the way the bones are interconnected.
    int16_t root[3];
    uint8_t  radius;
    uint8_t  reqColor;
    int8_t  velocity[3];
    int8_t  bones[0*3];  //Does not need to be all that long.

} network_model_t;

//////////////////////////////////////////////////////////////////////////////

typedef struct
{
    flightModeScreen mode;
    display_t * disp;
    int frames, tframes;
    uint8_t buttonState;
    bool paused;

    int16_t planeloc[3];
    int32_t planeloc_fine[3];
    int16_t hpr[3];
    int16_t speed;
    int16_t pitchmoment;
    int16_t yawmoment;
    int8_t lastSpeed[3];
    bool perfMotion;
    bool oob;
    uint32_t lastFlightUpdate;


    int enviromodels;
    const tdModel ** environment;
    const tdModel * otherShip;

    meleeMenu_t * menu;
    font_t ibm;
    font_t radiostars;
    font_t meleeMenuFont;

    int beans;
    int ondonut;
    uint32_t timeOfStart;
    uint32_t timeGot100Percent;
    uint32_t timeAccumulatedAtPause;
    uint32_t timeOfPause;
    int wintime;
    uint16_t menuEntryForInvertY;

    flLEDAnimation ledAnimation;
    uint8_t        ledAnimationTime;

    char highScoreNameBuffer[FLIGHT_HIGH_SCORE_NAME_LEN];
    uint8_t beangotmask[MAXRINGS];

    flightSimSaveData_t savedata;
    int didFlightsimDataLoad;

    int16_t ModelviewMatrix[16];
    int16_t ProjectionMatrix[16];
    int renderlinecolor;

    // Boolets for multiplayer.
    multiplayerpeer_t allPeers[MAX_PEERS]; //32x103 = 3296 bytes.
    boolet_t allBoolets[MAX_BOOLETS];  // ~8kB
    network_model_t * networkModels[MAX_NETWORK_MODELS];
    int nNetworkServerLastSeen; // When seeing a server resets.
    int nNetworkMode;

    boolet_t myBoolets[BOOLETSPERPLAYER];
    uint16_t booletHitHistory[BOOLETSPERPLAYER];  // For boolets that collide with us.
    int booletHitHistoryHead;
    int myBooletHead;
    int timeOfLastShot;
    int myHealth;
    uint32_t timeOfDeath;
    uint32_t lastNetUpdate;
    uint16_t killedByBooletID;

    int kills;
    int deaths;

    modelRangePair_t * mrp;

    uint8_t bgcolor;
    uint8_t was_hit_by_boolet;
} flight_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

bool getFlightSaveData(flight_t* flightPtr);
bool setFlightSaveData( flightSimSaveData_t * sd );
static void flightRender(int64_t elapsedUs);
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
static void FlightNetworkFrameCall( flight_t * tflight, display_t* disp, uint32_t now, modelRangePair_t ** mrp );
static void FlightfnEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
static void FlightfnEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

static void TModOrDrawBoolet( flight_t * tflight, tdModel * tmod, int16_t * mat, boolet_t * b, uint32_t now );
static void TModOrDrawPlayer( flight_t * tflight, tdModel * tmod, int16_t * mat, multiplayerpeer_t * b, uint32_t now );
static void TModOrDrawCustomNetModel( flight_t * tflight, tdModel * tmod, int16_t * mat, network_model_t * b, uint32_t now );

static uint32_t ReadUQ( uint32_t * rin, uint32_t bits );
static uint32_t PeekUQ( uint32_t * rin, uint32_t bits );
static uint32_t ReadBitQ( uint32_t * rin );
static uint32_t ReadUEQ( uint32_t * rin );
static int WriteUQ( uint32_t * v, uint32_t number, int bits );
static int WriteUEQ( uint32_t * v, uint32_t number );


//Forward libc declarations.
#ifndef EMU
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *));
int abs(int j);
int uprintf( const char * fmt, ... );
#else
#define uprintf printf
#endif

/*============================================================================
 * Variables
 *==========================================================================*/

static const char fl_title[]  = "Flyin Donut";
static const char fl_flight_env[] = "Atrium Course";
static const char fl_flight_invertY0_env[] = "Y Invert: Off";
static const char fl_flight_invertY1_env[] = "Y Invert: On";
static const char fl_flight_perf[] = "Free Solo/VS";
static const char fl_100_percent[] = "100% 100% 100%";
static const char fl_turn_around[] = "TURN AROUND";
static const char fl_you_win[] = "YOU   WIN!";
static const char fl_paused[] = "PAUSED";
static const char str_quit[] = "Exit";
static const char str_high_scores[] = "High Scores";

swadgeMode modeFlight =
{
    .modeName = fl_title,
    .fnEnterMode = flightEnterMode,
    .fnExitMode = flightExitMode,
    .fnButtonCallback = flightButtonCallback,
    .fnBackgroundDrawCallback = flightBackground,
    .wifiMode = ESP_NOW_IMMEDIATE,
    .fnEspNowRecvCb = FlightfnEspNowRecvCb,
    .fnEspNowSendCb = FlightfnEspNowSendCb,
    .fnMainLoop = flightRender,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .overrideUsb = false
};
flight_t* flight;

/*============================================================================
 * Functions
 *==========================================================================*/

bool getFlightSaveData(flight_t* flightPtr)
{
    if( !flightPtr->didFlightsimDataLoad )
    {
        size_t size = sizeof(flightPtr->savedata);
        bool r = readNvsBlob( "flightsim", &flightPtr->savedata, &size );
        if( !r || size != sizeof( flightPtr->savedata ) )
        {
            memset( &flightPtr->savedata, 0, sizeof( flightPtr->savedata ) );

            // Set defaults here!
            flightPtr->savedata.flightInvertY = 0;
        }
        flightPtr->didFlightsimDataLoad = 1;
    }
    return flightPtr->didFlightsimDataLoad;
}

bool setFlightSaveData( flightSimSaveData_t * sd )
{
    return writeNvsBlob( "flightsim", sd, sizeof( flightSimSaveData_t ) );
}

static void flightBackground(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
    flight->bgcolor = CNDRAW_BLACK;

    if( flight->myHealth == 0 && flight->mode == FLIGHT_FREEFLIGHT )
    {
        flight->bgcolor = 72;
    }

    fillDisplayArea( disp, x, y, x+w, h+y, flight->bgcolor );
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
    setFrameRateUs( DEFAULT_FRAMETIME );

    flight->mode = FLIGHT_MENU;
    flight->disp = disp;
    flight->renderlinecolor = CNDRAW_WHITE;

    const uint16_t * data = model3d;//(uint16_t*)getAsset( "3denv.obj", &retlen );
    data+=2; //header
    flight->enviromodels = *(data++);
    flight->environment = malloc( sizeof(const tdModel *) * flight->enviromodels );

    flight->mrp = malloc( sizeof(modelRangePair_t) * ( flight->enviromodels+MAX_PEERS+MAX_BOOLETS+MAX_NETWORK_MODELS ) );

    int i;
    for( i = 0; i < flight->enviromodels; i++ )
    {
        const tdModel * m = flight->environment[i] = (const tdModel*)data;
        data += 8 + m->nrvertnums + m->nrfaces * m->indices_per_face;
    }

    flight->otherShip = (const tdModel * )(ship3d + 3);  //+ header(3)

    loadFont("ibm_vga8.font", &flight->ibm);
    loadFont("radiostars.font", &flight->radiostars);
    loadFont("mm.font", &flight->meleeMenuFont);

    flight->menu = initMeleeMenu(fl_title, &flight->meleeMenuFont, flightMenuCb);
    flight->menu->allowLEDControl = false; // we manage the LEDs

    getFlightSaveData(flight);

    addRowToMeleeMenu(flight->menu, fl_flight_env);
    addRowToMeleeMenu(flight->menu, fl_flight_perf);
    flight->menuEntryForInvertY = addRowToMeleeMenu( flight->menu, flight->savedata.flightInvertY?fl_flight_invertY1_env:fl_flight_invertY0_env );
    addRowToMeleeMenu(flight->menu, str_high_scores);
    addRowToMeleeMenu(flight->menu, str_quit);
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
    if( flight->mrp )
    {
        free( flight->mrp );
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
        flight->lastNetUpdate = esp_timer_get_time();

        flight->nNetworkMode = 1;

        flightStartGame(FLIGHT_FREEFLIGHT);
    }
    else if (fl_flight_env == menuItem)
    {
        flight->nNetworkMode = 0;
        flightStartGame(FLIGHT_GAME);
    }
    else if ( fl_flight_invertY0_env == menuItem )
    {
        flight->savedata.flightInvertY = 1;
        flight->menu->rows[flight->menuEntryForInvertY] = fl_flight_invertY1_env;

        setFlightSaveData( &flight->savedata );
    }
    else if ( fl_flight_invertY1_env == menuItem )
    {
        flight->savedata.flightInvertY = 0;
        flight->menu->rows[flight->menuEntryForInvertY] = fl_flight_invertY0_env;

        setFlightSaveData( &flight->savedata );
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
    if (FLIGHT_FREEFLIGHT == flight->mode)
    {
        //espNowDeinit();
    }

    if( flightTimeHighScorePlace( flight->wintime, flight->beans == MAX_BEANS ) < NUM_FLIGHTSIM_TOP_SCORES )
    {
        flight->mode = FLIGHT_HIGH_SCORE_ENTRY;
        textEntryStart( flight->disp, &flight->ibm, FLIGHT_HIGH_SCORE_NAME_LEN+1, flight->highScoreNameBuffer );
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
    case FLIGHT_LED_GOT_HIT:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(0, 255, 60 - 40*abs(ledAnimationTime-2), 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(0, 255, 60 - 40*abs(ledAnimationTime-6), 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(0, 255, 60 - 40*abs(ledAnimationTime-10), 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(0, 255, 60 - 40*abs(ledAnimationTime-14), 1 );
        if( ledAnimationTime == 40 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_DIED:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(0, 255, 200-5*ledAnimationTime, 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(0, 255, 200-5*ledAnimationTime, 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(0, 255, 200-5*ledAnimationTime, 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(0, 255, 200-5*ledAnimationTime, 1 );
        if( ledAnimationTime == 50 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_GOT_KILL:
        leds[0] = leds[7] = SafeEHSVtoHEXhelper(ledAnimationTime*8+0, 255, 200-5*ledAnimationTime, 1 );
        leds[1] = leds[6] = SafeEHSVtoHEXhelper(ledAnimationTime*8+60, 255, 200-5*ledAnimationTime, 1 );
        leds[2] = leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*8+120, 255, 200-5*ledAnimationTime, 1 );
        leds[3] = leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*8+180, 255, 200-5*ledAnimationTime, 1 );
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

    setLeds(leds, NUM_LEDS);
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

    flight->paused = false;

    if( mode == FLIGHT_FREEFLIGHT )
    {
        //setFrameRateUs( 0 ); // For profiling only.
        setFrameRateUs( DEFAULT_FRAMETIME );
    }
    else
    {
        setFrameRateUs( DEFAULT_FRAMETIME );
    }

    flight->ondonut = 0; //Set to 14 to b-line it to the end for testing.
    flight->beans = 0; //Set to MAX_BEANS for 100% instant.
    flight->timeOfStart = (uint32_t)esp_timer_get_time();//-1000000*190; // (Do this to force extra coursetime)
    flight->timeGot100Percent = 0;
    flight->timeOfPause = 0;
    flight->wintime = 0;
    flight->speed = 0;

    //Starting location/orientation
    if( mode == FLIGHT_FREEFLIGHT )
    {
        srand( flight->timeOfStart );
        flight->planeloc[0] = (int16_t)(((rand()%1500)-200)*OOBMUX);
        flight->planeloc[1] = (int16_t)(((rand()%500)+500)*OOBMUX);
        flight->planeloc[2] = (int16_t)(((rand()%900)+2000)*OOBMUX);
        flight->hpr[0] = 2061;
        flight->hpr[1] = 190;
        flight->hpr[2] = 0;
        flight->myHealth = 100;
        flight->killedByBooletID = 0;
    }
    else
    {
        flight->planeloc[0] = (int16_t)(24*48*OOBMUX);
        flight->planeloc[1] = (int16_t)(18*48*OOBMUX); //Start pos * 48 since 48 is the fixed scale.
        flight->planeloc[2] = (int16_t)(60*48*OOBMUX);    
        flight->hpr[0] = 2061;
        flight->hpr[1] = 190;
        flight->hpr[2] = 0;
        flight->myHealth = 0; // Not used in regular mode.
    }
    flight->planeloc_fine[0] = flight->planeloc[0] << FLIGHT_SPEED_DEC;
    flight->planeloc_fine[1] = flight->planeloc[1] << FLIGHT_SPEED_DEC;
    flight->planeloc_fine[2] = flight->planeloc[2] << FLIGHT_SPEED_DEC;

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
    static const char* const EnglishNumberSuffix[] = { "st", "nd", "rd", "th" };
    switch(flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
            drawMeleeMenu(flight->disp, flight->menu);
            break;
        }
        case FLIGHT_FREEFLIGHT:
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
                    int cs = flight->savedata.timeCentiseconds[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
                    char * name = flight->savedata.displayName[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
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
void tdRotateNoMulEA( int16_t * f, int16_t x, int16_t y, int16_t z );
void tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z );
// void tdScale( int16_t * f, int16_t x, int16_t y, int16_t z );
void td4Transform( int16_t * pout, const int16_t * restrict f, const int16_t * pin );
void tdPtTransform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin );
void tdPt3Transform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin );
void tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z );
// void Draw3DSegment( display_t * disp, const int16_t * c1, const int16_t * c2 );
uint16_t tdSQRT( uint32_t inval );
int16_t tdDist( const int16_t * a, const int16_t * b );


//From https://github.com/cnlohr/channel3/blob/master/user/3d.c

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

#define speedyHash( seed ) (( seed = (seed * 1103515245) + 12345 ), seed>>16 )

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
    tdIdentity( flight->ProjectionMatrix );
    tdIdentity( flight->ModelviewMatrix );

    Perspective( VIEWPORT_PERSPECTIVE, 256 /* 1.0 */, 50, 8192, flight->ProjectionMatrix );
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
//    int16_t ftmp[16];
//    tdIdentity(ftmp);
    f[m03] += x;
    f[m13] += y;
    f[m23] += z;
//    tdMultiply( f, ftmp, f );
}

void tdRotateNoMulEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
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
    f[m00] = (cy*cz)>>8;
    f[m10] = ((((sx*sy)>>8)*cz)-(cx*sz))>>8;
    f[m20] = ((((cx*sy)>>8)*cz)+(sx*sz))>>8;
    f[m30] = 0;

    f[m01] = (cy*sz)>>8;
    f[m11] = ((((sx*sy)>>8)*sz)+(cx*cz))>>8;
    f[m21] = ((((cx*sy)>>8)*sz)-(sx*cz))>>8;
    f[m31] = 0;

    f[m02] = -sy;
    f[m12] = (sx*cy)>>8;
    f[m22] = (cx*cy)>>8;
    f[m32] = 0;

    f[m03] = 0;
    f[m13] = 0;
    f[m23] = 0;
    f[m33] = 1;
}

void tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];
    tdRotateNoMulEA( ftmp, x, y, z );
    tdMultiply( f, ftmp, f );
}

// void tdScale( int16_t * f, int16_t x, int16_t y, int16_t z )
// {
//     f[m00] = (f[m00] * x)>>8;
//     f[m01] = (f[m01] * x)>>8;
//     f[m02] = (f[m02] * x)>>8;
// //    f[m03] = (f[m03] * x)>>8;

//     f[m10] = (f[m10] * y)>>8;
//     f[m11] = (f[m11] * y)>>8;
//     f[m12] = (f[m12] * y)>>8;
// //    f[m13] = (f[m13] * y)>>8;

//     f[m20] = (f[m20] * z)>>8;
//     f[m21] = (f[m21] * z)>>8;
//     f[m22] = (f[m22] * z)>>8;
// //    f[m23] = (f[m23] * z)>>8;
// }

void td4Transform( int16_t * pout, const int16_t * restrict f, const int16_t * pin )
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

void tdPtTransform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin )
{
    pout[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + 256 * f[m03])>>8;
    pout[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + 256 * f[m13])>>8;
    pout[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + 256 * f[m23])>>8;
    pout[3] = (pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + 256 * f[m33])>>8;
}

void tdPt3Transform( int16_t * pout, const int16_t * restrict f, const int16_t * restrict pin )
{
    pout[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + 256 * f[m03])>>8;
    pout[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + 256 * f[m13])>>8;
    pout[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + 256 * f[m23])>>8;
}

int LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 )
{
    int16_t tmppt[4];
    tdPtTransform( tmppt, flight->ModelviewMatrix, coords_3v );
    td4Transform( tmppt, flight->ProjectionMatrix, tmppt );
    if( tmppt[3] >= -4 ) { return -1; }
    int calcx = ((256 * tmppt[0] / tmppt[3])/VIEWPORT_DIV+(TFT_WIDTH/2));
    int calcy = ((256 * tmppt[1] / tmppt[3])/VIEWPORT_DIV+(TFT_HEIGHT/2));
    if( calcx < -16000 || calcx > 16000 || calcy < -16000 || calcy > 16000 ) return -2;
    *o1 = calcx;
    *o2 = calcy;
    return 0;
}

// Note: Function unused.  For illustration purposes.
// void Draw3DSegment( display_t * disp, const int16_t * c1, const int16_t * c2 )
// {
//     int16_t sx0, sy0, sx1, sy1;
//     if( LocalToScreenspace( c1, &sx0, &sy0 ) ||
//         LocalToScreenspace( c2, &sx1, &sy1 ) ) return;

//     //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
//     speedyLine( disp, sx0, sy0, sx1, sy1, CNDRAW_WHITE );
//     //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 );

//     //plotLine( sx0, sy0, sx1, sy1, CNDRAW_WHITE );
// }


// Only needs center and radius.
int tdModelVisibilitycheck( const tdModel * m )
{

    //For computing visibility check
    int16_t tmppt[4];
    tdPtTransform( tmppt, flight->ModelviewMatrix, m->center );
    td4Transform( tmppt, flight->ProjectionMatrix, tmppt );
    if( tmppt[3] < -2 )
    {
        int scx = ((256 * tmppt[0] / tmppt[3])/VIEWPORT_DIV+(TFT_WIDTH/2));
        int scy = ((256 * tmppt[1] / tmppt[3])/VIEWPORT_DIV+(TFT_HEIGHT/2));
       // int scz = ((65536 * tmppt[2] / tmppt[3]));
        int scd = ((-256 * 2 * m->radius / tmppt[3])/VIEWPORT_DIV);
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

#if 0
    // By the time we get here, we're sure we want to render.
    if( tdModelVisibilitycheck( m ) < 0 )
    {
        return;
    }
#endif

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
                speedyLine( disp, cv1[0], cv1[1], cv2[0], cv2[1], flight->renderlinecolor );
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
                    outlineTriangle( disp, cv1[0], cv1[1], cv2[0], cv2[1], cv3[0], cv3[1], flight->bgcolor, flight->renderlinecolor );
            }
        }
    }
}

int mdlctcmp( const void * va, const void * vb );
int mdlctcmp( const void * va, const void * vb )
{
    const modelRangePair_t * a = (const modelRangePair_t *)va;
    const modelRangePair_t * b = (const modelRangePair_t *)vb;
    return b->mrange - a->mrange;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void flightRender(int64_t elapsedUs __attribute__((unused)))
{

#ifndef EMU
    if( flight->mode == FLIGHT_FREEFLIGHT ) uart_tx_one_char('R');
#endif
    flightUpdate( 0 );

    flight_t * tflight = flight;
    display_t * disp = tflight->disp;
    tflight->tframes++;
    if( tflight->mode != FLIGHT_GAME && tflight->mode != FLIGHT_GAME_OVER && tflight->mode != FLIGHT_FREEFLIGHT ) return;

    SetupMatrix();

    uint32_t now = esp_timer_get_time();

    tdRotateEA( flight->ProjectionMatrix, tflight->hpr[1]/11, tflight->hpr[0]/11, 0 );
    tdTranslate( flight->ModelviewMatrix, -tflight->planeloc[0], -tflight->planeloc[1], -tflight->planeloc[2] );

    modelRangePair_t * mrp = tflight->mrp;
    modelRangePair_t * mrptr = mrp;

/////////////////////////////////////////////////////////////////////////////////////////
////GAME LOGIC GOES HERE (FOR COLLISIONS/////////////////////////////////////////////////

    int i;
    for( i = 0; i < tflight->enviromodels;i++ )
    {
        const tdModel * m = tflight->environment[i];

        int label = m->label;
        int draw = 1;
        if( (flight->mode != FLIGHT_FREEFLIGHT) && label )
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
                    tflight->wintime = flightGetCourseTimeUs() / 10000;
                    tflight->mode = FLIGHT_GAME_OVER;
                }
            }
        }

        if( draw == 0 ) continue;

        int r = tdModelVisibilitycheck( m );
        if( r < 0 ) continue;
        mrptr->model = m;
        mrptr->mrange = r;
        mrptr++;
    }

    if( tflight->nNetworkMode )
        FlightNetworkFrameCall( tflight, disp, now, &mrptr );

// #ifndef EMU
//     if( flight->mode == FLIGHT_FREEFLIGHT ) uart_tx_one_char('2');
//     uint32_t mid1 = getCycleCount();
// #else
//     uint32_t mid1 = cndrawPerfcounter;
// #endif

    int mdlct = mrptr - mrp;


    //Painter's algorithm
    qsort( mrp, mdlct, sizeof( modelRangePair_t ), mdlctcmp );

// #ifndef EMU
//     if( flight->mode == FLIGHT_FREEFLIGHT ) uart_tx_one_char('3');
//     uint32_t mid2 = getCycleCount();
// #else
//     uint32_t mid2 = cndrawPerfcounter;
// #endif

    for( i = 0; i < mdlct; i++ )
    {
        const tdModel * m = mrp[i].model;
        if( (intptr_t)m < 0x3000 )
        {
            // It's a special thing.  Don't draw the normal way.
            //0x0000 - 0x1000 = Boolets.
            if( (intptr_t)m < 0x800 ) TModOrDrawBoolet( tflight, 0, 0, &tflight->allBoolets[(intptr_t)m], now );
            else if( (intptr_t)m < 0x1000 ) TModOrDrawBoolet( tflight, 0, 0, &tflight->myBoolets[(intptr_t)m-0x800], now );
            else if( (intptr_t)m < 0x2000 ) TModOrDrawPlayer( tflight, 0, 0, &tflight->allPeers[((intptr_t)m)-0x1000], now );
            else TModOrDrawCustomNetModel( tflight, 0, 0, tflight->networkModels[((intptr_t)m) - 0x2000], now );
        }
        else
        {
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
            else if( (flight->mode != FLIGHT_FREEFLIGHT) && (draw == 2 || draw == 3) )
            {
                if( draw == 2 )
                {
                    // Originally, (tflight->frames&1)?CNDRAW_WHITE:CNDRAW_BLACK;
                    // Now, let's go buck wild.
                    flight->renderlinecolor = (tflight->frames*7)&127;
                }
                if( draw == 3 )
                {
                    //flight->renderlinecolor = (tflight->frames&1)?CNDRAW_BLACK:CNDRAW_WHITE;
                    flight->renderlinecolor = ((tflight->frames+i))&127;
                }
                tdDrawModel( disp, m );
                flight->renderlinecolor = CNDRAW_WHITE;
            }
        }
    }


// #ifndef EMU
//         //OVERCLOCK_SECTION_DISABLE();
//         //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 );
//     if( flight->mode == FLIGHT_FREEFLIGHT ) uart_tx_one_char('4');
//     uint32_t stop = getCycleCount();
// //    if( flight->mode == FLIGHT_FREEFLIGHT )
// //        portENABLE_INTERRUPTS();
// #else
//     uint32_t stop = cndrawPerfcounter;
// #endif


    if( flight->mode == FLIGHT_GAME || tflight->mode == FLIGHT_FREEFLIGHT )
    {
        char framesStr[32] = {0};
        int16_t width;

        if( flight->mode != FLIGHT_FREEFLIGHT )
        {
            fillDisplayArea(disp, 0, 0, TFT_WIDTH, flight->radiostars.h + 1, CNDRAW_BLACK);

            //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
            int elapsed = flightGetCourseTimeUs() / 10000;

            snprintf(framesStr, sizeof(framesStr), "%d/%d, %d", tflight->ondonut, MAX_DONUTS, tflight->beans );
            // width = textWidth(&flight->radiostars, framesStr);
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 50, 0 );

            snprintf(framesStr, sizeof(framesStr), "%d.%02d", elapsed/100, elapsed%100 );
            // width = textWidth(&flight->radiostars, framesStr);
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, 0 );
        }
        
        // if( flight->mode == FLIGHT_FREEFLIGHT )
        // {
        //     snprintf(framesStr, sizeof(framesStr), "%d", mid1 - start );
        //     drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h+1 );
        //     snprintf(framesStr, sizeof(framesStr), "%d", mid2 - mid1 );
        //     drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*2+2 );
        //     snprintf(framesStr, sizeof(framesStr), "%d", stop - mid2 );
        //     drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*3+3 );
        //     snprintf(framesStr, sizeof(framesStr), "%d FPS", fps );
        //     drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - 110, flight->radiostars.h*4+4 );
        // }


        if( flight->mode == FLIGHT_FREEFLIGHT )
        {
            fillDisplayArea(disp, 0, TFT_HEIGHT - flight->radiostars.h - 1, TFT_WIDTH, TFT_HEIGHT, CNDRAW_BLACK);

            // Display health in free-flight mode.
            snprintf(framesStr, sizeof(framesStr), "%d", tflight->myHealth);
            width = textWidth(&flight->radiostars, framesStr);
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 46, TFT_HEIGHT - flight->radiostars.h );

            snprintf(framesStr, sizeof(framesStr), "K:%d", tflight->kills);
            width = textWidth(&flight->radiostars, framesStr);
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 90, TFT_HEIGHT - flight->radiostars.h );

            snprintf(framesStr, sizeof(framesStr), "D:%d", tflight->deaths);
            width = textWidth(&flight->radiostars, framesStr);
            drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, 144, TFT_HEIGHT - flight->radiostars.h );
        }

        snprintf(framesStr, sizeof(framesStr), "%d", tflight->speed);
        width = textWidth(&flight->radiostars, framesStr);
        fillDisplayArea(disp, TFT_WIDTH - width-50, TFT_HEIGHT - flight->radiostars.h - 1, TFT_WIDTH, TFT_HEIGHT, CNDRAW_BLACK);
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, TFT_WIDTH - width + 1-50, TFT_HEIGHT - flight->radiostars.h );


        if(flight->paused)
        {
            width = textWidth(&flight->ibm, fl_paused);
            fillDisplayArea(disp, (TFT_WIDTH - width) / 2 - 2, TFT_HEIGHT / 4 - 2, (TFT_WIDTH + width) / 2 + 2, TFT_HEIGHT / 4 + flight->ibm.h + 2, CNDRAW_BLACK);
            drawText(disp, &flight->ibm, PROMPT_COLOR, fl_paused, (TFT_WIDTH - width) / 2, TFT_HEIGHT / 4);
        }

        if(flight->oob)
        {
            width = textWidth(&flight->ibm, fl_turn_around);
            drawText(disp, &flight->ibm, PROMPT_COLOR, fl_turn_around, (TFT_WIDTH - width) / 2, (TFT_HEIGHT - 6 * flight->ibm.h) / 2);
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
        drawText(disp, &flight->radiostars, PROMPT_COLOR, fl_you_win, (disp->w - textWidth(&flight->radiostars, fl_you_win)) / 2, 50);
        snprintf(framesStr, sizeof(framesStr), "TIME: %d.%02d", tflight->wintime/100,tflight->wintime%100 );
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, (disp->w - textWidth(&flight->radiostars, framesStr)) / 2/*((tflight->wintime>10000)?14:20)+75*/, 18+50);
        snprintf(framesStr, sizeof(framesStr), "BEANS: %2d",tflight->beans );
        drawText(disp, &flight->radiostars, PROMPT_COLOR, framesStr, (disp->w - textWidth(&flight->radiostars, framesStr)) / 2, 36+50);
    }

    if( tflight->beans >= MAX_BEANS )
    {
        if( tflight->timeGot100Percent == 0 )
            tflight->timeGot100Percent = flightGetCourseTimeUs();

        int crazy = (flightGetCourseTimeUs() - tflight->timeGot100Percent) < 3000000;
        drawText( disp, &flight->ibm, crazy?( tflight->tframes * 9 % MAX_COLOR ):PROMPT_COLOR, fl_100_percent, (TFT_WIDTH - textWidth(&flight->ibm, fl_100_percent)) / 2, (TFT_HEIGHT - 4 * flight->ibm.h) / 2 + 2);
    }

    //If perf test, force full frame refresh
    //Otherwise, don't force full-screen refresh

#ifndef EMU    
    if( flight->mode == FLIGHT_FREEFLIGHT ) uart_tx_one_char('5');
#endif
    return;
}

static void flightGameUpdate( flight_t * tflight )
{
    uint32_t now = esp_timer_get_time();
    uint32_t delta = now - tflight->lastFlightUpdate;
    tflight->lastFlightUpdate = now;
    uint8_t bs = tflight->buttonState;

    const int flight_min_speed = (flight->mode==FLIGHT_FREEFLIGHT)?0:FLIGHT_MIN_SPEED;

    //If we're at the ending screen and the user presses a button end game.
    if( tflight->mode == FLIGHT_GAME_OVER && ( bs & 16 ) && flight->frames > 199 )
    {
        flightEndGame();
    }

    if( flight->paused )
    {
        return;
    }

    int dead = 0;
    if( tflight->mode == FLIGHT_FREEFLIGHT && tflight->myHealth <= 0)
    {
        if( (uint32_t)(now - tflight->timeOfDeath) > 2000000 )
        {
            tflight->deaths++;
            flightStartGame(FLIGHT_FREEFLIGHT);
            return;
        }
        dead = 1;
    }

    if( ( tflight->mode == FLIGHT_GAME || tflight->mode == FLIGHT_FREEFLIGHT ) && !dead )
    {
        int dpitch = 0;
        int dyaw = 0;

        if( bs & 4 ) dpitch += THRUSTER_ACCEL;
        if( bs & 8 ) dpitch -= THRUSTER_ACCEL;
        if( bs & 1 ) dyaw += THRUSTER_ACCEL;
        if( bs & 2 ) dyaw -= THRUSTER_ACCEL;

        if( tflight->savedata.flightInvertY ) dyaw *= -1;

        // If flying upside down, invert left/right. (Optional see flip note below)
        if( tflight->hpr[1] >= 990 && tflight->hpr[1] < 2970 ) dpitch *= -1;

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
        
        if( tflight->hpr[0] >= 3960 ) tflight->hpr[0] -= 3960;
        if( tflight->hpr[0] < 0 ) tflight->hpr[0] += 3960;
        if( tflight->hpr[1] >= 3960 ) tflight->hpr[1] -= 3960;
        if( tflight->hpr[1] < 0 ) tflight->hpr[1] += 3960;

        // Optional: Prevent us from doing a flip.
        // if( tflight->hpr[1] > 1040 && tflight->hpr[1] < 1980 ) tflight->hpr[1] = 1040;
        // if( tflight->hpr[1] < 2990 && tflight->hpr[1] > 1980 ) tflight->hpr[1] = 2990;

        if( bs & 16 ) tflight->speed++;
        else tflight->speed--;
        if( tflight->speed < flight_min_speed ) tflight->speed = flight_min_speed;

        if( tflight->mode == FLIGHT_FREEFLIGHT )
        {
            if( tflight->speed > FLIGHT_MAX_SPEED_FREE ) tflight->speed = FLIGHT_MAX_SPEED_FREE;
        }
        else
        {
            if( tflight->speed > FLIGHT_MAX_SPEED ) tflight->speed = FLIGHT_MAX_SPEED;
        }
    }

    //If game over, just keep status quo.

    int yawDivisor = getCos1024( tflight->hpr[1]/11 );

    // game target is/was 25000 us per frame.
    int rspeed = ( tflight->speed);

    int32_t speedPU[3];
    speedPU[0] = (((rspeed * getSin1024( tflight->hpr[0]/11 ) * yawDivisor ))>>10)>>7;  // >>7 is "to taste"
    speedPU[2] = (((rspeed * getCos1024( tflight->hpr[0]/11 ) * yawDivisor ))>>10)>>7;
    speedPU[1] = (rspeed * -getSin1024( tflight->hpr[1]/11 ) )>>7;

    flight->planeloc_fine[0] += ((int32_t)delta * speedPU[0])>>8; // Also, to taste.
    flight->planeloc_fine[1] += ((int32_t)delta * speedPU[1])>>8;
    flight->planeloc_fine[2] += ((int32_t)delta * speedPU[2])>>8;

    flight->lastSpeed[0] = speedPU[0]>>4;  // This is crucial.  It's the difference between the divisor above and FLIGHT_SPEED_DEC!
    flight->lastSpeed[1] = speedPU[1]>>4;
    flight->lastSpeed[2] = speedPU[2]>>4;

    tflight->planeloc[0] = flight->planeloc_fine[0]>>FLIGHT_SPEED_DEC;
    tflight->planeloc[1] = flight->planeloc_fine[1]>>FLIGHT_SPEED_DEC;
    tflight->planeloc[2] = flight->planeloc_fine[2]>>FLIGHT_SPEED_DEC;

    // Bound the area
    tflight->oob = false;
    if(tflight->planeloc[0] < -(int)(1900*OOBMUX))
    {
        flight->planeloc_fine[0] = -((int)(1900*OOBMUX)<<FLIGHT_SPEED_DEC);
        tflight->oob = true;
    }
    else if(tflight->planeloc[0] > (int)(1900*OOBMUX))
    {
        flight->planeloc_fine[0] = ((int)(1900*OOBMUX))<<FLIGHT_SPEED_DEC;
        tflight->oob = true;
    }

    if(tflight->planeloc[1] < -((int)(800*OOBMUX)))
    {
        flight->planeloc_fine[1] = -(((int)(800*OOBMUX))<<FLIGHT_SPEED_DEC);
        tflight->oob = true;
    }
    else if(tflight->planeloc[1] > (int)(3500*OOBMUX))
    {
        flight->planeloc_fine[1] = ((int)(3500*OOBMUX))<<FLIGHT_SPEED_DEC;
        tflight->oob = true;
    }

    if(tflight->planeloc[2] < -((int)(1300*OOBMUX)))
    {
        flight->planeloc_fine[2] = -(((int)(1300*OOBMUX))<<FLIGHT_SPEED_DEC);
        tflight->oob = true;
    }
    else if(tflight->planeloc[2] > ((int)(3700*OOBMUX)))
    {
        flight->planeloc_fine[2] = ((int)(3700*OOBMUX))<<FLIGHT_SPEED_DEC;
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
                    flight->savedata.flightInvertY ^= 1;
                    flight->menu->rows[flight->menuEntryForInvertY] = flight->savedata.flightInvertY?fl_flight_invertY1_env:fl_flight_invertY0_env;

                    setFlightSaveData( &flight->savedata );
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
        case FLIGHT_FREEFLIGHT:
        case FLIGHT_GAME:
        {
            if(evt->down && evt->button == START && flight->mode != FLIGHT_FREEFLIGHT)
            {
                // If we're about to pause, save current course time
                if(!flight->paused)
                {
                    flight->timeOfPause = (uint32_t)esp_timer_get_time();
                }
                // If we're about to resume, subtract time spent paused from start time
                else
                {
                    flight->timeOfStart += ((uint32_t)esp_timer_get_time() - flight->timeOfPause);
                }

                flight->paused = !flight->paused;
            }

            if( evt->down && evt->button == BTN_B && flight->mode == FLIGHT_FREEFLIGHT )
            {
                uint32_t now = esp_timer_get_time();
                uint32_t timeSinceShot = now - flight->timeOfLastShot;
                if( timeSinceShot > 300000 ) // Limit fire rate.
                {  
                    // Fire a boolet.
                    boolet_t * tb = &flight->myBoolets[flight->myBooletHead];
                    flight->myBooletHead = ( flight->myBooletHead + 1 ) % BOOLETSPERPLAYER;
                    tb->flags = (rand()%65535)+1;
                    tb->timeOfLaunch = now;
                    flight->timeOfLastShot = now;

                    int eo_sign = (flight->myBooletHead&1)?1:-1;
                    int16_t rightleft[3];
                    rightleft[0] = -eo_sign*(getCos1024( flight->hpr[0]/11 ) ) >> 6;
                    rightleft[2] =  eo_sign*(getSin1024( flight->hpr[0]/11 ) ) >> 6;
                    rightleft[1] =  0;

                    tb->launchLocation[0] = flight->planeloc[0] + rightleft[0];
                    tb->launchLocation[1] = flight->planeloc[1] + rightleft[1];
                    tb->launchLocation[2] = flight->planeloc[2] + rightleft[2];
                    tb->launchRotation[0] = flight->hpr[0];
                    tb->launchRotation[1] = flight->hpr[1];
                }
            }

            flight->buttonState = state;
            break;
        }
        case FLIGHT_GAME_OVER:
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
    int i;
    for( i = 0; i < NUM_FLIGHTSIM_TOP_SCORES; i++ )
    {
        int cs = flight->savedata.timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
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

    int i;
    for( i = NUM_FLIGHTSIM_TOP_SCORES-1; i > insertplace; i-- )
    {
        memcpy( flight->savedata.displayName[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            flight->savedata.displayName[(i-1)+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            NUM_FLIGHTSIM_TOP_SCORES );
        flight->savedata.timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES] =
            flight->savedata.timeCentiseconds[i-1+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
    }
    int namelen = strlen( name );
    if( namelen > FLIGHT_HIGH_SCORE_NAME_LEN ) namelen = FLIGHT_HIGH_SCORE_NAME_LEN;
    memcpy( flight->savedata.displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
        name, namelen );

    //Zero pad if less than 4 chars.
    if( namelen < FLIGHT_HIGH_SCORE_NAME_LEN )
        flight->savedata.displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES][namelen] = 0;

    flight->savedata.timeCentiseconds[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES] = timeCentiseconds;
    setFlightSaveData( &flight->savedata );
}



//////////////////////////////////////////////////////////////////////////////
// Multiplayer



// From H.264, U, UE(ExpGolomb, k=0)
// Ways to store several values in a 32-bit number.

static uint32_t ReadUQ( uint32_t * rin, uint32_t bits )
{
    uint32_t ri = *rin;
    *rin = ri >> bits;
    return ri & ((1<<bits)-1);
}

static uint32_t PeekUQ( uint32_t * rin, uint32_t bits )
{
    uint32_t ri = *rin;
    return ri & ((1<<bits)-1);
}

static uint32_t ReadBitQ( uint32_t * rin )
{
    uint32_t ri = *rin;
    *rin = ri>>1;
    return ri & 1;
}

static uint32_t ReadUEQ( uint32_t * rin )
{
    if( !*rin ) return 0; //0 is invalid for reading Exp-Golomb Codes
    // Based on https://stackoverflow.com/a/11921312/2926815
    int32_t zeroes = 0;
    while( ReadBitQ( rin ) == 0 ) zeroes++;
    uint32_t ret = 1 << zeroes;
    for (int i = zeroes - 1; i >= 0; i--)
        ret |= ReadBitQ( rin ) << i;
    return ret - 1;
}

static int WriteUQ( uint32_t * v, uint32_t number, int bits )
{
    uint32_t newv = *v;
    newv <<= bits;
    *v = newv | number;
    return bits;
}

static int WriteUEQ( uint32_t * v, uint32_t number )
{
    int numvv = number+1;
    int gqbits = 0;
    while ( numvv >>= 1)
    {
        gqbits++;
    }
    *v <<= gqbits;
    return WriteUQ( v,number+1, gqbits+1 ) + gqbits;
}

static void FinalizeUEQ( uint32_t * v, int bits )
{
    uint32_t vv = *v;
    uint32_t temp = 0;
    for( ; bits != 0; bits-- )
    {
        int lb = vv & 1;
        vv>>=1;
        temp<<=1;
        temp |= lb;
    }
    *v = temp;
}



static void TModOrDrawBoolet( flight_t * tflight, tdModel * tmod, int16_t * mat, boolet_t * b, uint32_t now )
{
    int32_t delta = now - b->timeOfLaunch;
    int16_t * pa = b->launchLocation;
    int16_t * lr = b->launchRotation;
    int yawDivisor = getCos1024( lr[1]/11 );
    int deltaWS = delta>>BOOLET_SPEED_DIVISOR;

    int32_t direction[3];
    direction[0] = (( getSin1024( lr[0]/11 ) * yawDivisor ) >> 10);
    direction[2] = (( getCos1024( lr[0]/11 ) * yawDivisor ) >> 10);
    direction[1] = -getSin1024( lr[1]/11 );

    int32_t deltas[3];
    deltas[0] = (deltaWS * direction[0] ) >> 10;
    deltas[2] = (deltaWS * direction[2] ) >> 10;
    deltas[1] = (deltaWS * direction[1] ) >> 10;

    int16_t cstuff[3];

    int16_t * center = tmod?tmod->center:cstuff;
    center[0] = pa[0]+deltas[0];
    center[2] = pa[2]+deltas[2];
    center[1] = pa[1]+deltas[1];

    if( tmod )
    {
        tmod->radius = 50;
        return;
    }
    else
    {
        // Control length of boolet line.
        direction[0]>>=3;
        direction[1]>>=3;
        direction[2]>>=3;

        // Actually "Draw" the boolet.
        int16_t end[3];
        int16_t start[3];
        end[0] = center[0] + (direction[0]);
        end[1] = center[1] + (direction[1]);
        end[2] = center[2] + (direction[2]);

        start[0] = center[0];
        start[1] = center[1];
        start[2] = center[2];

        int16_t sx, sy, ex, ey;
        // We now have "start" and "end"

        if( LocalToScreenspace( start, &sx, &sy ) < 0 ) return;
        if( LocalToScreenspace( end, &ex, &ey ) < 0 ) return;

        speedyLine( tflight->disp, sx, sy+1, ex, ey-1, BOOLET_COLOR );
    }
}

static void TModOrDrawPlayer( flight_t * tflight, tdModel * tmod, int16_t * mat, multiplayerpeer_t * p, uint32_t now )
{
    int32_t deltaTime = (now - p->timeOfUpdate);
    // This isn't "exactly" right since we assume the 
    int16_t * pa = p->posAt;
    int8_t * va = p->velAt;
    if( tmod )
    {
        tmod->center[0] = pa[0] + ((va[0] * deltaTime)>>16);
        tmod->center[1] = pa[1] + ((va[1] * deltaTime)>>16);
        tmod->center[2] = pa[2] + ((va[2] * deltaTime)>>16);
        tmod->radius = tflight->otherShip->radius;
        return;
    }

    const tdModel * s = tflight->otherShip;
    int nri = s->nrfaces*s->indices_per_face;
    int size_of_header_and_indices = 16 + nri*2;

    tdModel * m = alloca( size_of_header_and_indices + s->nrvertnums*6 );

    memcpy( m, s, size_of_header_and_indices ); // Copy header + indices.
    int16_t * mverticesmark = (int16_t*)&m->indices_and_vertices[nri];
    const int16_t * sverticesmark = (const int16_t*)&s->indices_and_vertices[nri];

    int16_t LocalXForm[16];
    {
        uint8_t * ra = (uint8_t*)p->rotAt;  // Tricky: Math works easier on signed numbers.
        tdRotateNoMulEA( LocalXForm, 0, 359-((ra[0]*360)>>8), 0 );  // Perform pitchbefore yaw
        tdRotateEA( LocalXForm, 359-((ra[1]*360)>>8), 0, (ra[2]*360)>>8 );
    }

    LocalXForm[m03] = pa[0] + ((va[0] * deltaTime)>>16);
    LocalXForm[m13] = pa[1] + ((va[1] * deltaTime)>>16);
    LocalXForm[m23] = pa[2] + ((va[2] * deltaTime)>>16);

    tdPt3Transform( m->center, LocalXForm, s->center );    

    int i;
    int lv = s->nrvertnums;

    int fd = p->framesDead;
    int hashseed = p->auxPeerFlags;
    if( fd )
    {
        int nri = s->nrfaces*s->indices_per_face;

        // if dead, blow ship apart.

        // This is a custom function so that we can de-weld the vertices.
        // If they remained welded, then it could not blow apart.
        for( i = 0; i < nri; i+=2 )
        {
            display_t * disp = tflight->disp;
            int16_t npos[3];
            int16_t ntmp[3];
            int i1 = s->indices_and_vertices[i];
            int i2 = s->indices_and_vertices[i+1];

            const int16_t * origs = sverticesmark + i1;
            int16_t sx1, sy1, sx2, sy2;

            npos[0] = (origs)[0] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[1] = (origs)[1] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[2] = (origs)[2] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            tdPt3Transform( ntmp, LocalXForm, npos );
            int oktorender = !LocalToScreenspace( ntmp, &sx1, &sy1 );

            origs = sverticesmark + i2;
            npos[0] = (origs)[0] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[1] = (origs)[1] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            npos[2] = (origs)[2] + ((((int16_t)speedyHash( hashseed )) * fd)>>14);
            tdPt3Transform( ntmp, LocalXForm, npos );
            oktorender &= !LocalToScreenspace( ntmp, &sx2, &sy2 );

            if( oktorender )
                speedyLine( disp, sx1, sy1, sx2, sy2, 180 ); //Override color of explosions to red.
        }
        if( fd < 255 ) p->framesDead = fd + 1;
    }
    else
    {
        for( i = 0; i < lv; i+=3 )
        {
            tdPt3Transform( mverticesmark + i, LocalXForm, sverticesmark + i );
        }
        int backupColor = flight->renderlinecolor;
        flight->renderlinecolor = p->reqColor;
        tdDrawModel( tflight->disp, m );
        flight->renderlinecolor = backupColor;
    }
}

static void TModOrDrawCustomNetModel( flight_t * tflight, tdModel * tmod, int16_t * mat, network_model_t * m, uint32_t now )
{
    int32_t delta = now - m->timeOfUpdate;

    int16_t * pa = m->root;
    int8_t * va = m->velocity;

    if( tmod )
    {
        // This isn't "exactly" right since we assume the 
        tmod->center[0] = pa[0] + ((va[0] * delta)>>16);
        tmod->center[1] = pa[1] + ((va[1] * delta)>>16);
        tmod->center[2] = pa[2] + ((va[2] * delta)>>16);
        tmod->radius = m->radius;  // Arbitrary.
        return;
    }

    int16_t croot[3];
    croot[0] = pa[0] + ((va[0] * delta)>>16);
    croot[1] = pa[1] + ((va[1] * delta)>>16);
    croot[2] = pa[2] + ((va[2] * delta)>>16);
    int16_t rootcx, rootcy;
    int rootisvalid = !LocalToScreenspace( croot, &rootcx, &rootcy );
    int oldisvalid = rootisvalid;

    int16_t last[3];
    memcpy( last, croot, sizeof( croot ) );
    int lastcx = rootcx, lastcy = rootcy;

    //Otherwise, actually draw.
    uint32_t binencprop = m->binencprop;
    /* int mid = */ReadUQ( &binencprop, 8 );
    int numBones = ReadUQ( &binencprop, 4 ) + 1;
    int8_t * bpos = m->bones;
    int i;
    uint8_t rcolor = m->reqColor;
    for( i = 0; i < numBones; i++ )
    {
        int draw = ReadUQ( &binencprop, 1 );
        int next = PeekUQ( &binencprop, 1 );
        if( draw == 0 && next == 0 )
        {
            ReadUQ( &binencprop, 1 );
            memcpy( last, croot, sizeof( croot ) );
            lastcx = rootcx;
            lastcy = rootcy;
            draw = ReadUQ( &binencprop, 1 );
        }
        
        int16_t newcx, newcy;
        int16_t new[3];
        new[0] = last[0] + bpos[0];
        new[1] = last[1] + bpos[1];
        new[2] = last[2] + bpos[2];
        bpos+=3;
        int newisvalid = !LocalToScreenspace( new, &newcx, &newcy );

        if( draw && newisvalid && oldisvalid )
            speedyLine( tflight->disp, lastcx, lastcy, newcx, newcy, rcolor );        

        oldisvalid = newisvalid;
        lastcx = newcx;
        lastcy = newcy;
        memcpy( last, new, sizeof( last ) );
    }
}


static void FlightNetworkFrameCall( flight_t * tflight, display_t* disp, uint32_t now, modelRangePair_t ** mrp )
{
    modelRangePair_t * mrptr = *mrp;

    if( tflight->nNetworkServerLastSeen > 0 )
        tflight->nNetworkServerLastSeen--;

    {
        multiplayerpeer_t * ap = tflight->allPeers;
        multiplayerpeer_t * p = ap;
        multiplayerpeer_t * end = p + MAX_PEERS;
        for( ; p != end; p++ )
        {
            if( p->basePeerFlags )
            {
                int32_t delta = now - p->timeOfUpdate;
                if( delta > 10000000 )
                {
                    p->basePeerFlags = 0;
                    continue;
                }

                tdModel tmod;
                TModOrDrawPlayer( tflight, &tmod, 0, p, now );

                // Make sure ship will be visible, if so tack it on to the list of things to be rendered.
                int r = tdModelVisibilitycheck( &tmod );
                if( r >= 0 )
                {
                    mrptr->model = (const tdModel *)(intptr_t)(0x1000 + (p - ap));
                    mrptr->mrange = r;
                    mrptr++;
                }
            }
        }
    }

    {
        network_model_t ** nmbegin = &flight->networkModels[0];
        network_model_t ** nm = nmbegin;
        network_model_t ** end = nm + MAX_NETWORK_MODELS;
        for( ; nm != end; nm++ )
        {
            network_model_t * m = *nm;
            if( m )
            {
                int32_t delta = now - m->timeOfUpdate;
                if( delta > 10000000 )
                {
                    // Destroy model.
                    free( m );
                    *nm = 0;
                    continue;
                }

                tdModel tmod;
                TModOrDrawCustomNetModel( tflight, &tmod, 0, m, now );

                // Make sure ship will be visible, if so transform it.
                int r = tdModelVisibilitycheck( &tmod );
                if( r >= 0 )
                {
                    mrptr->model = (const tdModel *)(intptr_t)(0x2000 + (nm - nmbegin));
                    mrptr->mrange = r;
                    mrptr++;
                }
            }
        }
    }

    // Also render boolets.
    {
        boolet_t * allb = &tflight->allBoolets[0];
        boolet_t * b = allb;
        boolet_t * booletEnd = b + MAX_BOOLETS;
        int gen_ofs = 0;
        do
        {
            for( ; b != booletEnd; b++ )
            {
                if( b->flags == 0 ) continue;
                int32_t delta = now - b->timeOfLaunch;
                if( delta > BOOLET_MAX_LIFETIME )
                {
                    b->flags = 0;
                    continue;
                }

                tdModel tmod;

                TModOrDrawBoolet( tflight, &tmod, 0, b, now );

                // Make sure ship will be visible, if so transform it.
                int r = tdModelVisibilitycheck( &tmod );
                if( r >= 0 )
                {
                    mrptr->model = (const tdModel *)(intptr_t)(0x0000 + (b - allb + gen_ofs));
                    mrptr->mrange = r;
                    mrptr++;
                }

                // Handle collision logic.
                if( gen_ofs == 0 )
                {
                    int deltas[3];
                    deltas[0] = tmod.center[0] - flight->planeloc[0];
                    deltas[1] = tmod.center[1] - flight->planeloc[1];
                    deltas[2] = tmod.center[2] - flight->planeloc[2];
                    int dt = deltas[0]*deltas[0] + deltas[1]*deltas[1] + deltas[2]*deltas[2];
                    if( dt < BOOLET_HIT_DIST_SQUARED && tflight->myHealth > 0 )
                    {
                        // First check to make sure it's not a boolet we shot.
                        int i;
                        for( i = 0; i < BOOLETSPERPLAYER; i++ )
                        {
                            if( b->flags == tflight->myBoolets[i].flags ) break;
                        }
                        if( i == BOOLETSPERPLAYER )
                        {
                            for( i = 0; i < BOOLETSPERPLAYER; i++ )
                            {
                                if( b->flags == tflight->booletHitHistory[i] ) break;
                            }
                            if( i == BOOLETSPERPLAYER )
                            {
                                //We made contact.
                                // Record boolet.
                                tflight->booletHitHistory[tflight->booletHitHistoryHead] = b->flags;
                                tflight->booletHitHistoryHead = tflight->booletHitHistoryHead+1;
                                if( tflight->booletHitHistoryHead == BOOLETSPERPLAYER ) tflight->booletHitHistoryHead = 0;

                                flightLEDAnimate( FLIGHT_LED_GOT_HIT );
                                tflight->was_hit_by_boolet = 1;
                                tflight->myHealth-=10;
                                if( tflight->myHealth<= 0)
                                {
                                    flightLEDAnimate( FLIGHT_LED_DIED );
                                    tflight->killedByBooletID = b->flags;
                                    tflight->timeOfDeath = now;
                                }
                            }
                        }
                    }
                }
            }
            if( gen_ofs == 0 )
            {
                // Redo above loop, but with my boolets.
                allb = &tflight->myBoolets[0];
                b = allb;
                booletEnd = b + BOOLETSPERPLAYER;
                gen_ofs = 0x800;
                continue;
            }
            else
            {
                break;
            }
        }while(1);
    }

    // Only update at 10Hz.
    if( now > tflight->lastNetUpdate + 100000 )
    {
        tflight->lastNetUpdate = now;
        int NumActiveBoolets = 0;
        int i;
        for( i = 0; i < BOOLETSPERPLAYER; i++ )
        {
            if( tflight->myBoolets[i].flags != 0 ) NumActiveBoolets++; 
        }

        uint8_t espnow_buffer[256];
        uint8_t *pp = espnow_buffer;
        *((uint32_t*)pp) = FSNET_CODE_PEER; pp+=4;
        *((uint32_t*)pp) = now; pp+=4;

        uint32_t contents = 0;
        int bitct = 0;
        bitct += WriteUEQ( &contents, 1 );
        bitct += WriteUEQ( &contents, 0 );
        bitct += WriteUEQ( &contents, 1 );
        bitct += WriteUEQ( &contents, NumActiveBoolets );
        FinalizeUEQ( &contents, bitct );
        *((uint32_t*)pp) = contents; pp+=4;
        // XXX TODO: Here, we need to update peers with our position + boolets + status (alive/dead)

        // There are no models.

        // There is a ship - us.

        *(pp++) = 0; // "shipNo"
        memcpy( pp, tflight->planeloc, sizeof( tflight->planeloc ) ); pp += sizeof( tflight->planeloc );
        memcpy( pp, tflight->lastSpeed, sizeof( tflight->lastSpeed ) ); pp += sizeof( tflight->lastSpeed ); //mirrors velAt real speed = ( this * microsecond >> 16 )

        int8_t orot[3];
        orot[0] = tflight->hpr[0]>>4;
        orot[1] = tflight->hpr[1]>>4;
        orot[2] = tflight->hpr[2]>>4;
        memcpy( pp, orot, sizeof( orot ) ); pp += sizeof( orot );
        uint8_t flags = 1 | ((tflight->myHealth>0)?0:2);
        memcpy( pp, &flags, sizeof( flags ) ); pp += sizeof( flags );
        memcpy( pp, &tflight->killedByBooletID, sizeof( tflight->killedByBooletID ) ); pp += sizeof( tflight->killedByBooletID );
        *(pp++) = tflight->was_hit_by_boolet?92:5; // Purple:Blue
        tflight->was_hit_by_boolet = 0;

        // Now, need to send boolets.
        for( i = 0; i < BOOLETSPERPLAYER; i++ )
        {
            boolet_t * b = &tflight->myBoolets[i];
            if( b->flags == 0 ) continue; 
            *(pp++) = i; // Local "bulletID"
            memcpy( pp, &b->timeOfLaunch, sizeof(b->timeOfLaunch) ); pp += sizeof( b->timeOfLaunch );
            memcpy( pp, b->launchLocation, sizeof(b->launchLocation) ); pp += sizeof( b->launchLocation );
            memcpy( pp, b->launchRotation, sizeof(b->launchRotation) ); pp += sizeof( b->launchRotation );
            memcpy( pp, &b->flags, sizeof(b->flags) ); pp += sizeof( b->flags );
        }

        int len = pp - espnow_buffer;
        espNowSend((char*)espnow_buffer, len); //Don't enable yet.
        // uprintf( "ESPNow Send: %d\n", len );
    }

/*
    //XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);   // Disable Interrupts
    //XTOS_SET_INTLEVEL(0);  //re-enable interrupts.
*/
    *mrp = mrptr;
}


typedef struct
{
    uint32_t timeOnPeer;
    uint32_t assetCounts; // protVer, models, ships, boolets in UEQ.
} __attribute__((packed)) network_packet_t;


void FlightfnEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    //uprintf( "RX RSSI %d\n", rssi );
    int i;
    flight_t * flt = flight;

    if( flt->nNetworkMode == 0 ) return;

    uint32_t pcode = *((uint32_t*)data);  data+= 4;

    if( !( pcode == FSNET_CODE_SERVER || (!flt->nNetworkServerLastSeen && pcode == FSNET_CODE_PEER ) ) || len < 8 ) return;

    int isPeer = pcode == FSNET_CODE_PEER;
    // If we get a server packet, switch to server mode for a while.
    if( !isPeer )
    {
        if( !flt->nNetworkServerLastSeen )
        {
            // Switched to server mode.  Need to clear out all peers.
            memset( flt->allPeers, 0, sizeof( flt->allPeers ) );
        }
        flt->nNetworkServerLastSeen = 300;
    }

    const network_packet_t * np = (const network_packet_t*)(data); data += sizeof( network_packet_t );

    uint32_t now = esp_timer_get_time();

    // find peer and compute time delta..
    uint32_t hash;

    multiplayerpeer_t * thisPeer = 0;
    multiplayerpeer_t * allPeers = flt->allPeers;

    int peerId = 0;

    if( isPeer )
    {
        hash = (*(const uint32_t*)mac_addr) + (*(const uint16_t*)(mac_addr+4));
        hash ^= ( (hash>>11) * 51 ) ^ ( (hash>>22) * 37); // I just made this up.  Probably best to use something good.
        hash %= MAX_PEERS;
        const int maxPeersToSearch = 12;
        int foundfree = -1;
        for( i = 0; i < maxPeersToSearch; i++ ) // Only search 12 peers, if our hash map is this full, it's not worth it.
        {
            thisPeer = allPeers + hash;
            if( thisPeer->basePeerFlags == 0 && foundfree < 0 )
                foundfree = hash;
            if( memcmp( thisPeer->mac, mac_addr, 6 ) == 0 ) break;
            hash = hash + 1;
            if( hash >= MAX_PEERS ) hash = 0;
        }

        if( i == maxPeersToSearch && foundfree >= 0 )
        {
            thisPeer = allPeers + foundfree;

            thisPeer->timeOffsetOfPeerFromNow = np->timeOnPeer - now;
            memcpy( thisPeer->mac, mac_addr, 6 );
            thisPeer->basePeerFlags |= 1;
            peerId = hash;
        }
        else if( i == maxPeersToSearch )
        {
            return; // Haven't found our peer in time. Abort.
        }
        else
        {
            peerId = hash;
        }
    }
    else
    {
        // Server.  Force thisPeer = 0.
        thisPeer = allPeers;
    }

    // Refine time offset.  TODO: Use asymmetric filter.
    int32_t estTimeOffsetError = np->timeOnPeer - (thisPeer->timeOffsetOfPeerFromNow + now);
    thisPeer->timeOffsetOfPeerFromNow += estTimeOffsetError>>3;

    // Compute "now" in peer time.
    uint32_t peerSendInOurTime = np->timeOnPeer - thisPeer->timeOffsetOfPeerFromNow;

    uint32_t assetCounts = np->assetCounts;

    int protVer = ReadUEQ( &assetCounts );
    if( protVer != 1 ) return; // Try not to rev version.

    // assetCounts => models, ships, boolets, all in UEQ.

    {
        network_model_t ** netModels = &flight->networkModels[0];
        int modelCount = ReadUEQ( &assetCounts );
        for( i = 0; i < modelCount; i++ )
        {
            uint32_t codeword = (*(const uint32_t*)data);  data += 4;
            uint32_t res_codeword = codeword;
            uint32_t id = ReadUQ( &codeword, 8 );
            uint32_t bones = ReadUQ( &codeword, 4 ) + 1;

            // Just a quick and dirty way to prevent a buffer overflow.
            if( id >= MAX_NETWORK_MODELS ) id = 0;
            network_model_t * m;

            m = netModels[id];

            if( m && m->binencprop != codeword )
            {
                // Something has changed. Recreate.
                free( m );
                m = 0;
            }

            if( !m )
            {
                netModels[id] = m = malloc( sizeof( network_model_t ) + bones * 3 );
            }

            m->timeOfUpdate = peerSendInOurTime;
            m->binencprop = res_codeword;
            memcpy( m->root, data, sizeof(m->root) ); data += sizeof(m->root); 
            m->radius = (*data++);
            m->reqColor = (*data++);
            memcpy( m->velocity, data, sizeof(m->velocity) ); data += sizeof(m->velocity); 
            memcpy( m->bones, data, bones * 3 ); data += bones * 3;
        }
    }
    {
        int shipCount = ReadUEQ( &assetCounts );

        for( i = 0; i < shipCount; i++ )
        {
            int readID = *(data++);
            int shipNo = isPeer?peerId:readID;

            if( shipNo >= MAX_PEERS ) shipNo = 0;

            multiplayerpeer_t * tp = allPeers + shipNo;
            tp->timeOfUpdate = peerSendInOurTime;

            // Pos, Vel, Rot, RotVel + flags.
            memcpy( tp->posAt, data, sizeof( tp->posAt ) ); data+=sizeof( tp->posAt );
            memcpy( tp->velAt, data, sizeof( tp->velAt ) ); data+=sizeof( tp->velAt );
            memcpy( tp->rotAt, data, sizeof( tp->rotAt ) ); data+=sizeof( tp->rotAt );
            memcpy( &tp->basePeerFlags, data, sizeof( tp->basePeerFlags ) ); data+=sizeof( tp->basePeerFlags ); tp->basePeerFlags |= 1;
            memcpy( &tp->auxPeerFlags, data, sizeof( tp->auxPeerFlags ) ); data+=sizeof( tp->auxPeerFlags );
            tp->reqColor = *(data++);

            //uprintf( "%d %d %d - %d %d %d - %d %d %d %08x %08x\n", tp->posAt[0],tp->posAt[1],tp->posAt[2],tp->velAt[0], tp->velAt[1], tp->velAt[2], 
            //    tp->rotAt[0], tp->rotAt[1], tp->rotAt[2], tp->basePeerFlags, tp->auxPeerFlags );

            if( tp->basePeerFlags & 2 )
            {
                if( tp->framesDead == 0 )
                {
                    // Determine if it was one of our boolets that killed them.

                    boolet_t * b = flt->myBoolets;
                    boolet_t * be = b + BOOLETSPERPLAYER;
                    for( ; b != be ; b++ )
                    {
                        if( b->flags == tp->auxPeerFlags )
                        {
                            // It was one of our boolets!
                            flightLEDAnimate( FLIGHT_GOT_KILL );
                            flt->kills++;
                            break;
                        }
                    }

                    tp->framesDead = 1;
                }
            }
            else
            {
                tp->framesDead = 0;
            }
        }
    }
    {
        int booletCount = ReadUEQ( &assetCounts );

        boolet_t * allBoolets = &flt->allBoolets[0];
        for( i = 0; i < booletCount; i++ )
        {
            int booletID = *(data++);
            if( isPeer )
            {
                if( booletID >= BOOLETSPERPLAYER ) booletID = BOOLETSPERPLAYER;

                // Fixed locations.
                booletID += peerId*BOOLETSPERPLAYER;
            }
            else
            {
                if( booletID >= MAX_BOOLETS ) booletID = 0;
            }
            boolet_t * b = allBoolets + booletID;
            b->timeOfLaunch = *((const uint32_t*)data) - thisPeer->timeOffsetOfPeerFromNow;
            data += 4;
            memcpy( b->launchLocation, data, sizeof(b->launchLocation) );
            data += sizeof(b->launchLocation);
            memcpy( b->launchRotation, data, sizeof(b->launchRotation) );
            data += sizeof(b->launchRotation);
            memcpy( &b->flags, data, sizeof(b->flags) ); data+=sizeof(b->flags);
        }
    }
}

static void FlightfnEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    //uprintf( "SEND OK %d\n", status );
    // Do nothing.
}

