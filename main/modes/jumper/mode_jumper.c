//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "linked_list.h"
#include "led_util.h"

#include "mode_jumper.h"
#include "jumper_menu.h"

//==============================================================================
// Constants
//==============================================================================

#define TO_SECONDS 1000000

static const uint8_t rowOffset[] = {5, 10, 15, 10, 5};

//===
// Structs
//===

typedef struct
{
    uint8_t numTiles;
    int32_t level;
    int32_t time;
    int32_t seconds;
    jumperBlockType_t blocks[30];
} jumperStage_t;

typedef struct
{
    wsg_t block[9];
    jumperGamePhase_t currentPhase;
    int64_t frameElapsed;
    display_t* d;
    font_t* mm_font;
    jumperStage_t* scene;
    bool controlsEnabled;

} jumperGame_t;




//==============================================================================
// Function Prototypes
//==============================================================================
void checkPlayerInput(void);
void jumperPlayerInput(void);
void CheckLevel(void);
void setupState(uint8_t stageIndex);

void drawJumperScene(display_t* d, font_t* font);
void drawJumperHud(display_t* d, font_t* font);

//==============================================================================
// Variables
//==============================================================================

uint64_t jumperJumpTime = 500000;

static jumperGame_t* j = NULL;

jumperCharacter_t* player = NULL;

//==============================================================================
// Functions
//==============================================================================
/**
 * Initialize all data needed for the jumper game
 * 
 * @param disp The display to draw to
 * @param mmFont The font used for teh HUD, already loaded
 * 
 */
void jumperStartGame(display_t* disp, font_t* mmFont)
{
    j = calloc(1, sizeof(jumperGame_t));

    j->d = disp;
    j->mm_font = mmFont;

    j->scene = calloc(1, sizeof(jumperStage_t));


    loadWsg("block_0a.wsg",&j->block[0]);
    loadWsg("block_0b.wsg",&j->block[1]);
    loadWsg("block_0c.wsg",&j->block[2]);
    loadWsg("block_0d.wsg",&j->block[3]);
    loadWsg("block_0a.wsg",&j->block[4]);
    loadWsg("block_0a.wsg",&j->block[5]);
    loadWsg("block_0a.wsg",&j->block[6]);
    loadWsg("block_0a.wsg",&j->block[7]);

    player = calloc(1, sizeof(jumperCharacter_t));
    loadWsg("ki0.wsg", &player->frames[0]);
    loadWsg("ki1.wsg", &player->frames[1]);
    loadWsg("kd0.wsg", &player->frames[2]);
    loadWsg("kj0.wsg", &player->frames[3]);
    loadWsg("ki0.wsg", &player->frames[4]);
    loadWsg("ki0.wsg", &player->frames[5]);
    loadWsg("ki0.wsg", &player->frames[6]);
    
    //Setup LEDS?

    //Setup stage
    setupState(0);
    // ESP_LOGI("FTR", "{[%d, %d], [%d, %d], %d}",
}

void setupState(uint8_t stageIndex)
{
    ESP_LOGI("FTR", "Stage %d %d", stageIndex, j->scene->level);
    j->currentPhase = JUMPER_COUNTDOWN;
    j->scene->time = 5 * TO_SECONDS;
    j->scene->seconds = 5000;
    j->scene->level = stageIndex + 1;

    player->state = CHARACTER_IDLE;
    player->x = 10;
    player->sx = 10;
    player->y = 64;
    player->sy = 64;
    player->frameIndex = 0;
    player->block = 0;
    player->dx = 10;
    player->dy = 64;

    for(uint8_t block = 0; block < 30; block++)
    {
        j->scene->blocks[block] = BLOCK_STANDARD;    
    }   

}

void checkPlayerInput(void)
{

}

void jumperGameLoop(int64_t elapsedUs)
{
    j->scene->time -= elapsedUs;
    switch(j->currentPhase)
    {
        case JUMPER_COUNTDOWN:
            
            if (j->scene->seconds < 0)
            {
                j->controlsEnabled = true;
                player->jumpReady = true;  
                j->currentPhase = JUMPER_GAMING;              

                
            }
            break;     
        case JUMPER_GAME_OVER:
            if (j->scene->seconds < 0)
            {
                ESP_LOGI("FTP", "BACK TO MENU");
            }

            break;   
        case JUMPER_WINSTAGE:
            if (j->scene->seconds < 0)
            {           
                setupState(j->scene->level);                
            }
            break;
        case JUMPER_GAMING:
        case JUMPER_DEATH:
        {
            // TODO something?
            break;
        }
    }

    jumperPlayerInput();

    player->frameTime += elapsedUs;
    switch (player->state)
    {
        case CHARACTER_IDLE:
            if (player->frameTime >= 150000)
            {
                player->frameTime -= 150000;

                player->frameIndex = (player->frameIndex + 1) % 2;
            }
            break;
        case CHARACTER_JUMPCROUCH:
            player->frameTime = 0;
            player->frameIndex = 2;
            break;
        case CHARACTER_JUMPING:
            player->frameTime = 0;
            player->frameIndex = 3;
            player->jumping = true;

            player->jumpTime += elapsedUs;
            if (player->jumpTime >= jumperJumpTime)
            {
                player->jumpTime = 0;
                player->state = CHARACTER_LANDING;
                ESP_LOGI("JUM", "Ooof");

                player->x = player->dx;
                player->y = player->dy;
            }
            else{
            // player->x = LERP based on x
                //(start_value + (end_value - start_value) * pct);
                float time = (player->jumpTime +.001)/(jumperJumpTime + .001);
                int per = time * 10;
                int offset[] = {0, 3, 5, 9, 11, 15, 11, 9, 5, 3, 0};
                player->x = player->sx + (player->dx - player->sx) * time;
                player->y = (player->sy + (player->dy - player->sy) * time) - offset[per];
                ESP_LOGI("JUM", "Distance %f", time);
            }
            // player->y = LERP based on y
            break;
        case CHARACTER_LANDING:
            player->jumping = false;
            player->frameIndex = 2;
            player->sx = player->x;
            player->sy = player->y;
            player->block = player->dBlock;

            if (j->scene->blocks[player->block] == BLOCK_STANDARD || j->scene->blocks[player->block] == BLOCK_COMPLETE)
            {
                j->scene->blocks[player->block] = BLOCK_PLAYERLANDED;
            }


            if (player->frameTime > 150000)
            {
                player->state = CHARACTER_IDLE;
                ESP_LOGI("JUM", "Ready");
                player->jumpReady = true;

                if (j->scene->blocks[player->block] == BLOCK_PLAYERLANDED)
                {
                    j->scene->blocks[player->block] = BLOCK_COMPLETE;
                }

            }
            CheckLevel();

            break;
        case CHARACTER_DYING:
        case CHARACTER_DEAD:
        {
            // TODO something?
            break;
        }
    }

    drawJumperScene(j->d, j->mm_font);

}

void CheckLevel()
{
    for(uint8_t block = 0; block < 30; block++)
    {
        if (j->scene->blocks[block] == BLOCK_STANDARD) return;    
    }   
    
    j->currentPhase = JUMPER_WINSTAGE;
    j->controlsEnabled = false;
    player->jumpReady = false;   
    j->scene->time = 5 * TO_SECONDS;

}

/**
 * @brief Process player controls
 * 
 * The player can only move in one direction at a time. DOWN takes priority, then UP, then LEFT
 * Once the player releases the button. Actually jump.
 * 
 */
void jumperPlayerInput(void)
{
    if (j->controlsEnabled == false) return;

    if (player->jumpReady && player->jumping == false)
    {
        player->sx = player->x;
        player->sy = player->y;
        if (player->btnState & DOWN)
        {
            player->state = CHARACTER_JUMPCROUCH;
            player->dBlock = player->block + 6;
        }
        else if (player->btnState & UP)
        {
            player->state = CHARACTER_JUMPCROUCH;
            player->dBlock = player->block - 6;
        }
        else if (player->btnState & LEFT)
        {
            player->state = CHARACTER_JUMPCROUCH;
            player->dBlock = player->block - 1;

        }
        else if (player->btnState & RIGHT)
        {
            player->state = CHARACTER_JUMPCROUCH;
            player->dBlock = player->block + 1;
        }
        else if (player->state == CHARACTER_JUMPCROUCH)
        {
            ESP_LOGI("JUM", "Jumping from %d block to block %d", player->block, player->dBlock);
            //Nothing is pressed. If something was pressed better jump
            player->state = CHARACTER_JUMPING;
            player->jumpReady = false;
            player->jumping = true;
            player->jumpTime = 0;
            
            ESP_LOGI("JUMP", "%d %d", player->sx, player->sy);
            uint8_t block = player->dBlock;
            uint8_t row = block / 6;    
            player->dx = 5 + ((block % 6)* 38) + rowOffset[row %5];
            player->dy = 64 + (row * 28);
            //drawWsg(d, &j->block[0],((block % 6)* 38) + rowOffset[row % 5], 84 +(row * 28), false, false, 0);
    
        }
    }
}

void drawJumperScene(display_t* d, font_t* font)
{
    
   d->clearPx();

   for(uint8_t block = 0; block < 30; block++)
   {
    uint8_t row = block / 6;    
    drawWsg(d, &j->block[j->scene->blocks[block]],((block % 6)* 38) + rowOffset[row % 5], 84 +(row * 28), false, false, 0);
          
   }   
    
   drawWsg(d, &player->frames[player->frameIndex], player->x, player->y, false, false, 0);
   
   drawJumperHud(d, font);
}

void drawJumperHud(display_t* d, font_t* font)
{
    char textBuffer[12];
    snprintf(textBuffer, sizeof(textBuffer) -1, "LEVEL %d", j->scene->level);
    drawText(d, font, c555, textBuffer, 20, 12);
           
    j->scene->seconds = (j->scene->time / TO_SECONDS);
    if (j->currentPhase == JUMPER_COUNTDOWN)
    {
        // char timeBuffer[3];
        if (j->scene->seconds <= 0)
        {
            drawText(d, font, c000, "JUMP!", 82, 129);
            drawText(d, font, c555, "JUMP!", 80, 128);
        }
        else if (j->scene->seconds <= 3 )
        {
            snprintf(textBuffer, sizeof(textBuffer) -1, "%d", j->scene->seconds);
            drawText(d, font, c555, textBuffer, 120, 128);
        }
        else
        {
            drawText(d, font, c555, "Ready?", 100, 128);
        }
    }
    if (j->currentPhase == JUMPER_GAMING)
    {
        if (j->scene->seconds <= 0 && j->scene->seconds > -2)
        {
            drawText(d, font, c000, "JUMP!", 82, 129);
            drawText(d, font, c555, "JUMP!", 80, 128);

        }
    }

    if (j->currentPhase == JUMPER_WINSTAGE)
    {
        drawText(d, font, c000, "JUMP COMPLETE!", 22, 129);
        drawText(d, font, c555, "JUMP COMPLETE!", 20, 128);
    }
}

void jumperGameButtonCb(buttonEvt_t* evt)
{
    player->btnState = evt->state;
}

void jumperExitGame(void)
{
    if (NULL != j)
    {
        //Clear all tiles
        //clear stage
        freeWsg(&j->block[0]);
        freeWsg(&j->block[1]);
        freeWsg(&j->block[2]);
        freeWsg(&j->block[3]);
        freeWsg(&j->block[4]);
        freeWsg(&j->block[5]);
        freeWsg(&j->block[6]);

        free(j);
        j = NULL;
    }
}