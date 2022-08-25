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
    int8_t blockOffset_x;
    int8_t blockOffset_y;
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
void jumperPlayerInput(void);
void jumperRemoveEnemies(void);
void jumperResetPlayer(void);
void CheckLevel(void);
void jumperSetupState(uint8_t stageIndex);

void drawJumperScene(display_t* d, font_t* font);
void drawJumperHud(display_t* d, font_t* font);

//==============================================================================
// Variables
//==============================================================================

uint64_t jumperJumpTime = 500000;

static jumperGame_t* j = NULL;

jumperCharacter_t* player = NULL;
jumperCharacter_t* evilDonut = NULL;

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
    loadWsg("block_0e.wsg",&j->block[4]);
    loadWsg("block_0a.wsg",&j->block[5]);
    loadWsg("block_0a.wsg",&j->block[6]);
    loadWsg("block_0a.wsg",&j->block[7]);

    player = calloc(1, sizeof(jumperCharacter_t));
    loadWsg("ki0.wsg", &player->frames[0]);
    loadWsg("ki1.wsg", &player->frames[1]);
    loadWsg("kd0.wsg", &player->frames[2]);
    loadWsg("kj0.wsg", &player->frames[3]);
    loadWsg("kk0.wsg", &player->frames[4]);
    loadWsg("kk1.wsg", &player->frames[5]);
    loadWsg("kk2.wsg", &player->frames[6]);
    loadWsg("kk3.wsg", &player->frames[7]);

    evilDonut = calloc(1, sizeof(jumperCharacter_t));
    loadWsg("edi0.wsg", &evilDonut->frames[0]);
    loadWsg("edi1.wsg", &evilDonut->frames[1]);
    loadWsg("edd0.wsg", &evilDonut->frames[2]);
    loadWsg("edj0.wsg", &evilDonut->frames[3]);
    loadWsg("edi0.wsg", &evilDonut->frames[4]);
    
    //Setup LEDS?

    //Setup stage
    jumperSetupState(0);
    // ESP_LOGI("FTR", "{[%d, %d], [%d, %d], %d}",
}

void jumperSetupState(uint8_t stageIndex)
{
    ESP_LOGI("FTR", "Stage %d %d", stageIndex, j->scene->level);
    j->currentPhase = JUMPER_COUNTDOWN;
    j->scene->time = 5 * TO_SECONDS;
    j->scene->seconds = 5000;
    j->scene->level = stageIndex + 1;
    j->scene->blockOffset_x = 20;
    j->scene->blockOffset_y = 64;
    jumperResetPlayer();

    
    jumperRemoveEnemies();

    for(uint8_t block = 0; block < 30; block++)
    {
        j->scene->blocks[block] = BLOCK_STANDARD;    
    }   

}

void jumperResetPlayer()
{
    player->state = CHARACTER_IDLE;
    player->x = j->scene->blockOffset_x + rowOffset[0] + 5;
    player->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
    player->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
    player->y = j->scene->blockOffset_y;
    player->sy = j->scene->blockOffset_y;
    player->dy = j->scene->blockOffset_y;
    player->frameIndex = 0;
    player->block = 0;
    player->jumpReady = true;  
    player->jumping  = false;
}

void jumperRemoveEnemies()
{
    evilDonut->state = CHARACTER_NONEXISTING;
    evilDonut->x = j->scene->blockOffset_x + rowOffset[0] + 5;
    evilDonut->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
    evilDonut->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
    evilDonut->y = j->scene->blockOffset_y;
    evilDonut->sy = j->scene->blockOffset_y;
    evilDonut->dy = j->scene->blockOffset_y;
    evilDonut->frameIndex = 0;
    evilDonut->block = 0;
    evilDonut->respawnTime = 6 * TO_SECONDS;
}


void jumperGameLoop(int64_t elapsedUs)
{
    j->scene->time -= elapsedUs;
    j->scene->seconds = (j->scene->time / TO_SECONDS);
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
                jumperSetupState(j->scene->level);                
            }
            else if (j->scene->seconds < 2)
            {                
                //YEAH!
                for(uint8_t block = 0; block < 30; block++)
                {
                    if (j->scene->blocks[block] != BLOCK_COMPLETE) continue;
                    j->scene->blocks[block] = BLOCK_WIN;    
                } 
            }
            break;
        case JUMPER_GAMING:
            if (evilDonut->state == CHARACTER_NONEXISTING || evilDonut->state == CHARACTER_DEAD)
            {
                evilDonut->respawnTime -= elapsedUs;
                if (evilDonut->respawnTime <= 0)
                {
                    evilDonut->respawnTime = 5 * TO_SECONDS;
                    evilDonut->state = CHARACTER_LANDING;

                }
            }
            break;
        case JUMPER_DEATH:
        {   

            if (j->scene->seconds < 0)
            {
                j->currentPhase = JUMPER_GAMING;

                // CLEAR ENEMIES
                jumperRemoveEnemies();

                // RESET PLAYER
                jumperResetPlayer();
            }

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
            if (player->frameTime >= 150000)
            {
                player->frameTime -= 150000;

                player->frameIndex = player->frameIndex + 1;

                if (player->frameIndex >= 7)
                {
                    player->frameIndex = 7;
                    player->state = CHARACTER_DEAD;
                    j->currentPhase = JUMPER_DEATH;
                    j->scene->time = 2 * TO_SECONDS;

                    //sub track a life and continue on
                }
            }
            break;
        case CHARACTER_DEAD:
        case CHARACTER_NONEXISTING:
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
    if (j->controlsEnabled == false || j->currentPhase != JUMPER_GAMING) return;

    if (player->btnState & BTN_A && player->state != CHARACTER_DEAD && player->state != CHARACTER_DYING)
    {
        player->state = CHARACTER_DYING;
        player->frameIndex = 4;
        player->frameTime = 0;
    }


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
           
           bool legitJump = true;

           //if (player->dBlock < 0) legitJump = false;
           if (player->dBlock >= 30) legitJump = false;
            int col = player->block % 6;
            int dcol = player->dBlock % 6;

            if (col == 0 && dcol == 5) legitJump = false;
            if (col == 5 && dcol == 0) legitJump = false;

            if (legitJump)
            {
                //Nothing is pressed. If something was pressed better jump
                player->state = CHARACTER_JUMPING;
                player->jumpReady = false;
                player->jumping = true;
                player->jumpTime = 0;
                
                ESP_LOGI("JUM", "%d %d", player->sx, player->sy);
                ESP_LOGI("JUM", "FROM ROW %d TO %d", col, dcol);
                uint8_t block = player->dBlock;
                uint8_t row = block / 6;    
                player->dx = 5 + j->scene->blockOffset_x + ((block % 6)* 38) + rowOffset[row %5];
                player->dy = j->scene->blockOffset_y + (row * 28);
                //drawWsg(d, &j->block[0],((block % 6)* 38) + rowOffset[row % 5], 84 +(row * 28), false, false, 0);
    
            }
            else
            {
                player->state = CHARACTER_IDLE;
            }

            
        }
    }
}

void drawJumperScene(display_t* d, font_t* font)
{
    
   d->clearPx();

   for(uint8_t block = 0; block < 30; block++)
   {
    uint8_t row = block / 6;    
    drawWsg(d, &j->block[j->scene->blocks[block]],j->scene->blockOffset_x + ((block % 6)* 38) + rowOffset[row % 5], 84 +(row * 28), false, false, 0);
          
   }   
    
    if (player->state != CHARACTER_DEAD)
    {
        drawWsg(d, &player->frames[player->frameIndex], player->x, player->y, false, false, 0);
    }
   
   drawJumperHud(d, font);
}

void drawJumperHud(display_t* d, font_t* font)
{
    char textBuffer[12];
    snprintf(textBuffer, sizeof(textBuffer) -1, "LEVEL %d", j->scene->level);
    drawText(d, font, c555, textBuffer, 20, 12);
           
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