//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "musical_buzzer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

#include "linked_list.h"
#include "led_util.h"
#include "aabb_utils.h"
#include "settingsManager.h"

#include "mode_jumper.h"
#include "jumper_menu.h"

//==============================================================================
// Constants
//==============================================================================

#define TO_SECONDS 1000000

static const uint8_t rowOffset[] = {5, 10, 15, 10, 5};
static const uint32_t aiResponseTime[] = { 2000000, 1000000, 900000, 800000, 700000, 600000, 500000, 400000, 300000, 200000 }; // level 1 will have a time of 3
//===
// Structs
//===


//==============================================================================
// Function Prototypes
//==============================================================================
void jumperPlayerInput(void);
void jumperRemoveEnemies(void);
void jumperResetPlayer(void);
void jumperCheckLevel(void);
void jumperKillPlayer(void);
void jumperDoEvilDonut(int64_t elapsedUs);
void jumperDoBlump(int64_t elapsedUs);
void jumperSetupState(uint8_t stageIndex);
bool jumperDoEnemyLand(uint8_t blockIndex);
void jumperClearBlock(uint8_t blockIndex);

void drawJumperScene(display_t* d);
void drawJumperEffects(display_t* d);
void drawJumperHud(display_t* d, font_t* prompt, font_t* font);

//==============================================================================
// Variables
//==============================================================================

static jumperGame_t* j = NULL;

static const song_t jumpDeathTune =
{
    .notes =
    {
        {G_SHARP_3, 250}, {SILENCE, 100}, {G_3, 200}, {SILENCE, 100},
        {F_3, 650}
    },
    .numNotes = 5,
    .shouldLoop = false
};

static const song_t jumpGameOverTune =
{
    .notes =
    {
        {C_4, 300}, {SILENCE, 50},
        {C_4, 300}, {SILENCE, 50},
        {C_4, 300}, {SILENCE, 50},
        {A_3, 150}, {SILENCE, 100},
        {A_SHARP_3, 150}, {SILENCE, 100},
        {C_4, 250}, {SILENCE, 100},
        {G_SHARP_3, 250}, {SILENCE, 100}, {G_3, 200}, {SILENCE, 100},
        {F_3, 650}
    },
    .numNotes = 17,
    .shouldLoop = false
};

static const song_t jumpWinTune =
{
    .notes =
    {
        {E_4, 250}, {SILENCE, 100}, {D_4, 200}, {SILENCE, 100},
        {E_4, 250}, {SILENCE, 100}, {G_3, 250}, {SILENCE, 100},
        {C_4, 350}, {SILENCE, 100}, {C_4, 200}, {SILENCE, 100}
    },
    .numNotes = 12,
    .shouldLoop = false
};

static const song_t jumpGameLoop = 
{
    .notes = {
        {F_3, 250}, {F_4, 250},
        {F_3, 250}, {F_4, 250},
        {F_3, 250}, {F_4, 250},
        {F_3, 250}, {F_4, 250},
        {G_SHARP_3, 250}, {G_SHARP_4, 250},
        {G_SHARP_3, 250}, {G_SHARP_4, 250},
        {A_SHARP_3, 250}, {A_SHARP_4, 250},
        {A_SHARP_3, 250}, {A_SHARP_4, 250},
    },
    .numNotes = 16,
    .shouldLoop = true
};

static const song_t jumpPerfectTune = 
{
    .notes =
    {
        {E_6, 250}, {SILENCE, 100}, {E_6, 200}, {SILENCE, 100},
        {C_6, 250}, {SILENCE, 100}, {A_5, 250}, {SILENCE, 100},
        {C_6, 650}, {SILENCE, 725}, {A_7, 50}, {B_7, 150}
    },
    .numNotes = 12,
    .shouldLoop = false
};

static const song_t jumpPlayerJump =
{
    .notes =
    {
        {G_5, 26}, {G_SHARP_5, 26},
        {A_5, 26}, {A_SHARP_5, 50}
    },
    .numNotes = 4,
    .shouldLoop = false
};

static const song_t jumpPlayerBrokeCombo =
{
    .notes =
    {
        {860, 26}, {830, 26},
        {800, 26}, {770, 26}
    },
    .numNotes = 4,
    .shouldLoop = false
};

static const song_t jumpCountdown = 
{
    .notes = 
    {
        {F_6, 26},
        {G_SHARP_6, 26},
        {F_6, 26},
        {D_SHARP_6, 26},
    },
    .numNotes = 4,
    .shouldLoop = false
};

static const song_t jumpEvilDonutJump =
{
    .notes =
    {
        {G_4, 26}, {G_SHARP_4, 26},
        {A_4, 26}, {A_SHARP_4, 50}
    },
    .numNotes = 4,
    .shouldLoop = false
};

static const song_t jumpBlumpJump =
{
    .notes =
    {
        {C_5, 26}, {E_5, 50},
        {C_5, 50}
    },
    .numNotes = 3,
    .shouldLoop = false
};

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
    j->promptFont = mmFont;

    loadFont("radiostars.font", &(j->game_font));

    j->multiplier = calloc(3, sizeof(jumperMultiplier_t));

    j->scene = calloc(1, sizeof(jumperStage_t));
    j->scene->combo = 1;
    j->scene->score = 0;
    j->scene->lives = 3;
    loadWsg("minidonut.wsg", &j->scene->livesIcon);


    loadWsg("block_0a.wsg", &j->block[0]);
    loadWsg("block_0b.wsg", &j->block[1]);
    loadWsg("block_0c.wsg", &j->block[2]);
    loadWsg("block_0d.wsg", &j->block[3]);
    loadWsg("block_0e.wsg", &j->block[4]);
    loadWsg("block_1a.wsg", &j->block[5]);
    loadWsg("block_1b.wsg", &j->block[6]);
    loadWsg("block_2.wsg", &j->block[7]);
    loadWsg("block_2b.wsg", &j->block[8]);
    loadWsg("block_2c.wsg", &j->block[9]);

    loadWsg("multiplier0.wsg", &j->digit[0]);
    loadWsg("multiplier1.wsg", &j->digit[1]);
    loadWsg("multiplier2.wsg", &j->digit[2]);
    loadWsg("multiplier3.wsg", &j->digit[3]);
    loadWsg("multiplier4.wsg", &j->digit[4]);
    loadWsg("multiplier5.wsg", &j->digit[5]);
    loadWsg("multiplier6.wsg", &j->digit[6]);
    loadWsg("multiplier7.wsg", &j->digit[7]);
    loadWsg("multiplier8.wsg", &j->digit[8]);
    loadWsg("multiplier9.wsg", &j->digit[9]);
    loadWsg("multiplierx.wsg", &j->digit[10]);
    loadWsg("perfect.wsg", &j->digit[11]);

    j->player = calloc(1, sizeof(jumperCharacter_t));

    loadWsg("ki0.wsg", &j->player->frames[0]);
    loadWsg("ki1.wsg", &j->player->frames[1]);
    loadWsg("kd0.wsg", &j->player->frames[2]);
    loadWsg("kj0.wsg", &j->player->frames[3]);
    loadWsg("kk0.wsg", &j->player->frames[4]);
    loadWsg("kk1.wsg", &j->player->frames[5]);
    loadWsg("kk2.wsg", &j->player->frames[6]);
    loadWsg("kk3.wsg", &j->player->frames[7]);

    j->evilDonut = calloc(1, sizeof(jumperCharacter_t));
    loadWsg("edi0.wsg", &j->evilDonut->frames[0]);
    loadWsg("edi1.wsg", &j->evilDonut->frames[1]);
    loadWsg("edd0.wsg", &j->evilDonut->frames[2]);
    loadWsg("edj0.wsg", &j->evilDonut->frames[3]);
    loadWsg("edi0.wsg", &j->evilDonut->frames[4]);


    j->blump = calloc(1, sizeof(jumperCharacter_t));
    loadWsg("blmpi0.wsg", &j->blump->frames[0]);
    loadWsg("blmpi1.wsg", &j->blump->frames[1]);
    loadWsg("blmpi2.wsg", &j->blump->frames[2]);
    loadWsg("blmpi3.wsg", &j->blump->frames[3]);
    loadWsg("blmpd0.wsg", &j->blump->frames[4]);
    loadWsg("blmpj0.wsg", &j->blump->frames[5]);

    j->jumperJumpTime = 500000;
    j->highScore = getQJumperHighScore();

    //Setup LEDS?

    //Setup stage
    jumperSetupState(0);
}

void jumperSetupState(uint8_t stageIndex)
{
    j->currentPhase = JUMPER_COUNTDOWN;
    j->scene->time = 5000000;
    j->scene->seconds = 5000;
    j->scene->level = stageIndex + 1;
    j->scene->blockOffset_x = 20;
    j->scene->blockOffset_y = 54;
    jumperResetPlayer();


    jumperRemoveEnemies();

    for(uint8_t block = 0; block < 30; block++)
    {
        j->scene->blocks[block] = BLOCK_STANDARD;
    }

    j->scene->perfect = 30;
    switch(j->scene->level % 10)
    {
        case 1:          
            break;
        case 2:
            j->scene->blocks[7] = BLOCK_EVILSTANDARD;
            j->scene->blocks[10] = BLOCK_EVILSTANDARD;
            j->scene->perfect = 28;            
            break;
            
        case 3:
            j->scene->blocks[13] = BLOCK_EVILSTANDARD;
            j->scene->blocks[14] = BLOCK_EVILSTANDARD;
            j->scene->blocks[15] = BLOCK_EVILSTANDARD;
            j->scene->blocks[16] = BLOCK_EVILSTANDARD;
            j->scene->perfect = 26;            
            break;
        case 4:            
            j->scene->blocks[14] = BLOCK_WARBLESTANDARD;
            j->scene->blocks[17] = BLOCK_WARBLESTANDARD;
            j->scene->perfect = 28;  
            break;
    }

}

void jumperResetPlayer()
{
    j->scene->combo = 1;
    j->player->state = CHARACTER_IDLE;
    j->player->x = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->player->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->player->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->player->y = j->scene->blockOffset_y;
    j->player->sy = j->scene->blockOffset_y;
    j->player->dy = j->scene->blockOffset_y;
    j->player->frameIndex = 0;
    j->player->block = 0;
    j->player->jumpReady = true;
    j->player->jumping  = false;
    j->player->flipped = false;


}

void jumperRemoveEnemies()
{
    j->evilDonut->state = CHARACTER_NONEXISTING;
    j->evilDonut->x = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->evilDonut->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->evilDonut->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->evilDonut->y = 0;
    j->evilDonut->sy = 0;
    j->evilDonut->dy = j->scene->blockOffset_y;
    j->evilDonut->frameIndex = 0;
    j->evilDonut->block = 0;
    j->evilDonut->dBlock = 0;
    j->evilDonut->respawnTime = 8000000;
    j->evilDonut->flipped = false;
    j->evilDonut->intelligence.decideTime = 0;
    j->evilDonut->jumpTime = 0;

    j->blump->state = CHARACTER_NONEXISTING;
    j->blump->x = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->blump->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->blump->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
    j->blump->y = 0;
    j->blump->sy = 0;
    j->blump->dy = j->scene->blockOffset_y;
    j->blump->frameIndex = 0;
    j->blump->block = 0;
    j->blump->dBlock = 0;
    j->blump->respawnTime = 4000000;
    j->blump->flipped = false;
    j->blump->intelligence.decideTime = 0;
    j->blump->jumpTime = 0;

    j->evilDonut->intelligence.resetTime = j->scene->level < 9 ? aiResponseTime[j->scene->level] :  aiResponseTime[9];
    j->blump->intelligence.resetTime = 700000; //Magic number
}


void jumperGameLoop(int64_t elapsedUs)
{
    jumperCharacter_t* player = j->player;
    jumperCharacter_t* evilDonut = j->evilDonut;
    jumperCharacter_t* blump = j->blump;

    j->scene->time -= elapsedUs;
    j->scene->seconds = (j->scene->time / TO_SECONDS);
    switch(j->currentPhase)
    {
        case JUMPER_COUNTDOWN:            


            if (j->scene->seconds < 1)
            {
                j->controlsEnabled = true;
                player->jumpReady = true;
                j->currentPhase = JUMPER_GAMING;

                buzzer_play_bgm(&jumpGameLoop);
            }
            else if (j->scene->seconds != j->scene->previousSecond && j->scene->seconds < 4)
            {
                j->scene->previousSecond = j->scene->seconds;
                buzzer_play_sfx(&jumpCountdown);
            }
            break;
        case JUMPER_WINSTAGE:
            if (j->scene->seconds < 0)
            {
                if (j->scene->level % 11 == 0 && j->scene->lives < 3)
                {
                    j->scene->lives++;
                }
                jumperSetupState(j->scene->level);
            }
            else if (j->scene->seconds < 2 && j->scene->combo >= j->scene->perfect)
            {
                //YEAH!
                for(uint8_t block = 0; block < 30; block++)
                {
                    if (j->scene->blocks[block] != BLOCK_COMPLETE)
                    {
                        continue;
                    }
                    j->scene->blocks[block] = BLOCK_WIN;
                }
            }
            break;
        case JUMPER_GAMING:
            if (player->state != CHARACTER_DYING)
            {
                jumperDoEvilDonut(elapsedUs);
                jumperDoBlump(elapsedUs);
            }
            break;
        case JUMPER_GAME_OVER:
        
            if (j->scene->seconds < 0)
            {
                
                setJumperMainMenu();
                jumperExitGame();
                return;
            }
            break;
        case JUMPER_DEATH:
        {
            if (j->scene->seconds < 0)
            {
                if (j->scene->lives == 0)
                {

                    buzzer_play_bgm(&jumpGameOverTune);

                    j->currentPhase = JUMPER_GAME_OVER;
                    j->scene->time = 3000000;

                    if(j->highScore > getQJumperHighScore())
                    {
                        setQJumperHighScore(j->highScore);
                    }
                }
                else
                {
                    j->currentPhase = JUMPER_GAMING;
                    buzzer_play_bgm(&jumpGameLoop);

                    // CLEAR ENEMIES
                    jumperRemoveEnemies();

                    // RESET PLAYER
                    jumperResetPlayer();

                    // Clean up blocks
                    for(uint8_t block = 0; block < 30; block++)
                    {                        
                        jumperClearBlock(block);
                    }
                }

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
            if (player->jumpTime >= j->jumperJumpTime)
            {
                player->jumpTime = 0;
                player->state = CHARACTER_LANDING;

                player->x = player->dx;
                player->y = player->dy;
            }
            else
            {
                //Player Lerp
                float time = (player->jumpTime + .001) / (j->jumperJumpTime + .001);
                int per = time * 10;
                int offset[] = {0, 3, 5, 9, 11, 15, 11, 9, 5, 3, 0};
                player->x = player->sx + (player->dx - player->sx) * time;
                player->y = (player->sy + (player->dy - player->sy) * time) - offset[per];
            }

            break;
        case CHARACTER_LANDING:
            player->jumping = false;
            player->frameIndex = 2;
            player->sx = player->x;
            player->sy = player->y;
            player->block = player->dBlock;

            switch(j->scene->blocks[player->block])
            {
                case BLOCK_STANDARD:
                    j->scene->blocks[player->block] = BLOCK_PLAYERLANDED;
                    j->scene->score = j->scene->score + (10 * j->scene->combo);

                    
                    if (j->scene->score > j->highScore)
                    {
                        j->highScore = j->scene->score;
                    }

                    if (j->scene->combo > 2)
                    {
                        for(int i = 0; i < 3; i++)
                        {
                            if (j->multiplier[i].digits[0] >= 11)
                            {
                                j->multiplier[i].x = player->x;
                                j->multiplier[i].y = player->y + 5;
                                j->multiplier[i].time = 0;
                                j->multiplier[i].digits[0] = 10;

                                if (j->scene->combo == j->scene->perfect)
                                {
                                    j->multiplier[i].digits[0] = 255;
                                    j->multiplier[i].x -= 24;
                                }
                                else if (j->scene->combo < 10)
                                {
                                    j->multiplier[i].digits[1] = j->scene->combo;
                                    j->multiplier[i].digits[2] = 11;
                                    j->multiplier[i].x += 4;
                                }
                                else
                                {
                                    j->multiplier[i].digits[1] = j->scene->combo / 10;
                                    j->multiplier[i].digits[2] = j->scene->combo - (j->multiplier[i].digits[1] * 10);
                                }
                                break;
                            }
                        }
                    }
                    
                    j->scene->combo++;
                    break;
                case BLOCK_WARBLESTANDARD:  
                    if (j->scene->combo > 5)
                    {                    
                        buzzer_play_sfx(&jumpPlayerBrokeCombo);
                    }

                    if ( esp_random() % 100 > 50)
                    {
                        j->scene->blocks[player->block] = BLOCK_WARBLELANDEDA;
                    }
                    else
                    {
                        j->scene->blocks[player->block] = BLOCK_WARBLELANDEDB;
                    }

                    j->scene->combo = 1;
                    break;

                case BLOCK_COMPLETE:                
                    if (j->scene->combo > 5)
                    {                    
                        buzzer_play_sfx(&jumpPlayerBrokeCombo);
                    }

                    j->scene->combo = 1;
                    j->scene->blocks[player->block] = BLOCK_PLAYERLANDED;
                    break;
                
                case BLOCK_EVILSTANDARD:
                    jumperKillPlayer();
                    j->scene->blocks[player->block] = BLOCK_EVILLANDED;
                    break;
                default:
                    break;
            }

            if (player->frameTime > 150000 && player->state != CHARACTER_DYING)
            {
                player->state = CHARACTER_IDLE;
                player->jumpReady = true;
                jumperClearBlock(player->block);

            }
            jumperCheckLevel();

            break;
        case CHARACTER_DYING:
            if (player->frameTime >= 200000)
            {
                player->frameTime -= 200000;

                player->frameIndex = player->frameIndex + 1;

                if (player->frameIndex > 7)
                {
                    player->frameIndex = 7;
                    player->state = CHARACTER_DEAD;
                    j->currentPhase = JUMPER_DEATH;
                    j->scene->time = 1000000;

                    //sub track a life and continue on
                    j->scene->lives--;


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

    //Check collision!
    if (player->state != CHARACTER_DEAD && player->state != CHARACTER_DYING)
    {

        box_t box1 =
        {
            .x0 = (player->x + 5) * 2,
            .y0 = (player->y + 16) * 2,
            .x1 = (player->x + 5 + 16) * 2,
            .y1 = (player->y + 8 + 8) * 2,
        };

        box_t box2 =
        {
            .x0 = (evilDonut->x + 5) * 2,
            .y0 = (evilDonut->y + 8) * 2,
            .x1 = (evilDonut->x + 5 + 16) * 2,
            .y1 = (evilDonut->y + 8 + 16) * 2,
        };

        if (evilDonut->state != CHARACTER_DYING && evilDonut->state != CHARACTER_DEAD && boxesCollide(box1, box2, 1))
        {
            jumperKillPlayer();
        }

        box_t box3 =
        {
            .x0 = (blump->x + 5) * 2,
            .y0 = (blump->y + 8) * 2,
            .x1 = (blump->x + 5 + 16) * 2,
            .y1 = (blump->y + 8 + 16) * 2,
        };

        if (blump->state != CHARACTER_DYING && blump->state != CHARACTER_DEAD && boxesCollide(box1, box3, 1))
        {
            jumperKillPlayer();
        }
    }

    //clean up special effects
    for (int i = 0; i < 3; i++)
    {
        //if statement to see if mutiplier is active
        if (j->multiplier[i].time > 2000000) continue;
        j->multiplier[i].time += elapsedUs;

        if (j->multiplier[i].time <= 250000) j->multiplier[i].y -= 1;

        if (j->multiplier[i].time >= 2000000)
        {
            j->multiplier[i].digits[0] = 11;
        }        

        if (j->multiplier[i].digits[0] >= 11 || j->multiplier[i].digits[0] == 0) continue;
        

    }

    drawJumperScene(j->d);

}

void jumperClearBlock(uint8_t blockIndex)
{
    switch(j->scene->blocks[blockIndex])
    {
        case BLOCK_EVILLANDED:
            j->scene->blocks[blockIndex] = BLOCK_EVILSTANDARD;
            break;
        case BLOCK_PLAYERLANDED:
            j->scene->blocks[blockIndex] = BLOCK_COMPLETE;
            break;
        case BLOCK_STANDARDLANDED:
            j->scene->blocks[blockIndex] = BLOCK_STANDARD;
            break;
        case BLOCK_WARBLELANDEDA:
        case BLOCK_WARBLELANDEDB:
            j->scene->blocks[blockIndex] = BLOCK_WARBLESTANDARD;
            break;
        default:
            break;
    }
}

bool jumperDoEnemyLand(uint8_t blockIndex)
{
    
    switch(j->scene->blocks[blockIndex])
    {
        case BLOCK_EVILSTANDARD:
            j->scene->blocks[blockIndex] = BLOCK_EVILLANDED;
            break;
        case BLOCK_COMPLETE:
            j->scene->blocks[blockIndex] = BLOCK_PLAYERLANDED;
            break;
        case BLOCK_STANDARD:
            j->scene->blocks[blockIndex] = BLOCK_STANDARDLANDED;
            break;
        case BLOCK_WARBLESTANDARD:
            if ( esp_random() % 100 > 50)
            {
                j->scene->blocks[blockIndex] = BLOCK_WARBLELANDEDA;
            }
            else
            {
                j->scene->blocks[blockIndex] = BLOCK_WARBLELANDEDB;
            }
            return true;
            break;
        default:
            break;
    }

    return false;
}

void jumperCheckLevel()
{
    for(uint8_t block = 0; block < 30; block++)
    {
        if (j->scene->blocks[block] == BLOCK_STANDARD)
        {
            return;
        }
    }

    buzzer_stop();
    if (j->scene->combo < j->scene->perfect)
    {
        buzzer_play_bgm(&jumpWinTune);        
    } 
    else
    {
        buzzer_play_bgm(&jumpPerfectTune);
    }
    j->currentPhase = JUMPER_WINSTAGE;
    j->controlsEnabled = false;
    j->player->jumpReady = false;
    j->scene->time = 5000000;

}

void jumperDoBlump(int64_t elapsedUs)
{
    jumperCharacter_t* blump = j->blump;

    blump->frameTime += elapsedUs;
    switch (blump->state)
    {
        case CHARACTER_IDLE:
            if (blump->frameTime >= 150000)
            {
                blump->frameTime -= 150000;

                blump->frameIndex = (blump->frameIndex + 1) % 4;
            }

            blump->intelligence.decideTime += elapsedUs;
            if (blump->intelligence.decideTime > blump->intelligence.resetTime)
            {
                // ESP_LOGI("JUM", "AI RESET");
                blump->intelligence.decideTime = 0;

                blump->dBlock = blump->block + 6;
                blump->state = CHARACTER_JUMPCROUCH;

            }
            break;
        case CHARACTER_JUMPCROUCH:
            blump->frameTime = 0;
            blump->frameIndex = 4;

            blump->intelligence.decideTime += elapsedUs;
            if (blump->intelligence.decideTime > 250000)
            {

                uint8_t block = blump->dBlock;
                uint8_t row = block / 6;

                blump->state = CHARACTER_JUMPING;
                blump->jumpReady = false;
                blump->jumping = true;
                blump->jumpTime = 0;

                buzzer_play_sfx(&jumpBlumpJump);
                if (blump->dBlock >= 30)
                {
                    blump->dx = blump->x + 10;
                    blump->dy = blump->y + 64;
                    blump->dBlock = 255;
                }
                else
                {
                    blump->dx = 5 + j->scene->blockOffset_x + ((block % 6) * 38) + rowOffset[row % 5];
                    blump->dy = j->scene->blockOffset_y + (row * 28);
                }


                blump->flipped = blump->sx > blump->dx;
            }
            break;
        case CHARACTER_JUMPING:
            blump->frameTime = 0;
            blump->frameIndex = 5;
            blump->jumping = true;

            blump->jumpTime += elapsedUs;
            if (blump->jumpTime >= j->jumperJumpTime)
            {
                blump->jumpTime = 0;
                blump->state = CHARACTER_LANDING;
                
                blump->x = blump->dx;
                blump->y = blump->dy;
            }
            else
            {
                //Blump Lerp
                float time = (blump->jumpTime + .001) / (j->jumperJumpTime + .001);
                int per = time * 10;
                int offset[] = {0, 3, 5, 9, 11, 15, 11, 9, 5, 3, 0};
                blump->x = blump->sx + (blump->dx - blump->sx) * time;
                blump->y = (blump->sy + (blump->dy - blump->sy) * time) - offset[per];
            }

            break;
        case CHARACTER_LANDING:
            blump->jumping = false;
            blump->frameIndex = 4;
            blump->sx = blump->x;
            blump->sy = blump->y;
            blump->block = blump->dBlock;

            if (blump->block == 255)
            {
                blump->state = CHARACTER_DEAD;
                blump->respawnTime = 6000000;
                return;
            }

            if(jumperDoEnemyLand(blump->block))
            {
                blump->state = CHARACTER_DYING;
            }

            if (blump->frameTime > 150000)
            {
                blump->state = CHARACTER_IDLE;
                // ESP_LOGI("JUM", "Evil Donut Landed");
                blump->jumpReady = true;
                jumperClearBlock(blump->block);

            }
            jumperCheckLevel();

            break;
        case CHARACTER_DYING:
            if (blump->frameTime >= 150000)
            {
                blump->frameTime -= 150000;

                blump->frameIndex = blump->frameIndex + 1;

                if (blump->frameIndex >= 7)
                {
                    blump->frameIndex = 7;
                    blump->state = CHARACTER_DEAD;
                }

                if (blump->block < 30 )
                {
                    jumperClearBlock(blump->block);
                }
            }
            break;
        case CHARACTER_DEAD:
        case CHARACTER_NONEXISTING:
        {

            blump->respawnTime -= elapsedUs;
            if (blump->respawnTime <= 0)
            {
                blump->respawnTime = 5000000;
                blump->state = CHARACTER_JUMPING;
                blump->sy = 0;
                blump->block = esp_random() % 6;
                blump->dBlock = blump->block;
                blump->sx = 5 + j->scene->blockOffset_x + ((blump->block % 6) * 38) + rowOffset[0];
                blump->x = blump->sx;
                blump->dx = blump->sx;
                blump->dy = j->scene->blockOffset_y;
                // ESP_LOGI("JUM", "{%d}", blump->block);
            }
            break;
        }
    }

}

void jumperDoEvilDonut(int64_t elapsedUs)
{
    jumperCharacter_t* player = j->player;
    jumperCharacter_t* evilDonut = j->evilDonut;

    evilDonut->frameTime += elapsedUs;
    switch (evilDonut->state)
    {
        case CHARACTER_IDLE:
            if (evilDonut->frameTime >= 150000)
            {
                evilDonut->frameTime -= 150000;

                evilDonut->frameIndex = (evilDonut->frameIndex + 1) % 2;
            }

            evilDonut->intelligence.decideTime += elapsedUs;
            if (evilDonut->intelligence.decideTime > evilDonut->intelligence.resetTime)
            {
                // ESP_LOGI("JUM", "AI RESET");
                evilDonut->intelligence.decideTime = 0;

                if (player->y > evilDonut->y)
                {
                    evilDonut->dBlock = evilDonut->block + 6;
                    evilDonut->state = CHARACTER_JUMPCROUCH;
                }
                else if (player->y < evilDonut->y)
                {
                    evilDonut->dBlock = evilDonut->block - 6;
                    evilDonut->state = CHARACTER_JUMPCROUCH;
                }
                else if (player->x > evilDonut->x)
                {
                    evilDonut->dBlock = evilDonut->block + 1;
                    evilDonut->state = CHARACTER_JUMPCROUCH;
                }
                else if (player->x < evilDonut->x)
                {
                    evilDonut->dBlock = evilDonut->block - 1;
                    evilDonut->state = CHARACTER_JUMPCROUCH;
                }
            }
            break;
        case CHARACTER_JUMPCROUCH:
            evilDonut->frameTime = 0;
            evilDonut->frameIndex = 2;

            evilDonut->intelligence.decideTime += elapsedUs;
            if (evilDonut->intelligence.decideTime > 50000)
            {
                bool legitJump = true;

                int col = evilDonut->block % 6;
                int dcol = evilDonut->dBlock % 6;
                uint8_t block = evilDonut->dBlock;
                uint8_t row = block / 6;

                if (col == 0 && dcol == 5)
                {
                    legitJump = false;
                }
                if (col == 5 && dcol == 0)
                {
                    legitJump = false;
                }
                if (evilDonut->dBlock >= 30)
                {
                    legitJump = false;
                }

                if (legitJump == false)
                {
                    // ESP_LOGI("JUM", "How'd we get to %d", evilDonut->dBlock);
                    evilDonut->intelligence.decideTime = 0;
                    evilDonut->state = CHARACTER_IDLE;
                    return;
                }
                evilDonut->state = CHARACTER_JUMPING;
                evilDonut->jumpReady = false;
                evilDonut->jumping = true;
                evilDonut->jumpTime = 0;

                buzzer_play_sfx(&jumpEvilDonutJump);

                evilDonut->dx = 5 + j->scene->blockOffset_x + ((block % 6) * 38) + rowOffset[row % 5];
                evilDonut->dy = j->scene->blockOffset_y + (row * 28);

                evilDonut->flipped = evilDonut->sx > evilDonut->dx;
            }
            break;
        case CHARACTER_JUMPING:
            evilDonut->frameTime = 0;
            evilDonut->frameIndex = 3;
            evilDonut->jumping = true;

            evilDonut->jumpTime += elapsedUs;
            if (evilDonut->jumpTime >= j->jumperJumpTime)
            {
                evilDonut->jumpTime = 0;
                evilDonut->state = CHARACTER_LANDING;

                evilDonut->x = evilDonut->dx;
                evilDonut->y = evilDonut->dy;
            }
            else
            {

                float time = (evilDonut->jumpTime + .001) / (j->jumperJumpTime + .001);
                int per = time * 10;
                int offset[] = {0, 3, 5, 9, 11, 15, 11, 9, 5, 3, 0};
                evilDonut->x = evilDonut->sx + (evilDonut->dx - evilDonut->sx) * time;
                evilDonut->y = (evilDonut->sy + (evilDonut->dy - evilDonut->sy) * time) - offset[per];
            }

            break;
        case CHARACTER_LANDING:
            evilDonut->jumping = false;
            evilDonut->frameIndex = 2;
            evilDonut->sx = evilDonut->x;
            evilDonut->sy = evilDonut->y;
            evilDonut->block = evilDonut->dBlock;

            if(jumperDoEnemyLand(evilDonut->block))
            {
                evilDonut->state = CHARACTER_DYING;
            }

            if (evilDonut->frameTime > 150000)
            {
                evilDonut->state = CHARACTER_IDLE;
                // ESP_LOGI("JUM", "Evil Donut Landed");
                evilDonut->jumpReady = true;

                
                jumperClearBlock(evilDonut->block);

            }
            jumperCheckLevel();

            break;
        case CHARACTER_DYING:

        
            if (evilDonut->frameTime >= 150000)
            {
                evilDonut->frameTime -= 150000;

                evilDonut->frameIndex = evilDonut->frameIndex + 1;

                if (evilDonut->frameIndex >= 7)
                {
                    evilDonut->frameIndex = 7;
                    evilDonut->state = CHARACTER_DEAD;
                }
                jumperClearBlock(evilDonut->block);
            }
            break;
        case CHARACTER_DEAD:
        case CHARACTER_NONEXISTING:
        {

            evilDonut->respawnTime -= elapsedUs;
            if (evilDonut->respawnTime <= 0)
            {
                evilDonut->respawnTime = 5000000;
                evilDonut->state = CHARACTER_JUMPING;
                evilDonut->x = j->scene->blockOffset_x + rowOffset[0] + 5;
                evilDonut->sx = j->scene->blockOffset_x + rowOffset[0] + 5;
                evilDonut->dx = j->scene->blockOffset_x + rowOffset[0] + 5;
                evilDonut->block = 0;
                evilDonut->dBlock = 0;
                evilDonut->y = 0;
                evilDonut->sy = 0;
                evilDonut->dy = j->scene->blockOffset_y;
            }
            break;
        }
    }

}

void jumperKillPlayer()
{
    buzzer_play_bgm(&jumpDeathTune);
    j->player->state = CHARACTER_DYING;
    j->player->frameIndex = 4;
    j->player->frameTime = 0;
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
    jumperCharacter_t* player = j->player;

    if (j->controlsEnabled == false || j->currentPhase != JUMPER_GAMING)
    {
        return;
    }

    if (player->btnState & BTN_A && player->state != CHARACTER_DEAD && player->state != CHARACTER_DYING)
    {
        //Button has no use here
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
            // ESP_LOGI("JUM", "Jumping from %d block to block %d", player->block, player->dBlock);

            bool legitJump = true;

            if (player->dBlock >= 30)
            {
                legitJump = false;
            }
            int col = player->block % 6;
            int dcol = player->dBlock % 6;

            if (col == 0 && dcol == 5)
            {
                legitJump = false;
            }
            if (col == 5 && dcol == 0)
            {
                legitJump = false;
            }

            if (legitJump)
            {
                player->state = CHARACTER_JUMPING;
                player->jumpReady = false;
                player->jumping = true;
                player->jumpTime = 0;
                buzzer_play_sfx(&jumpPlayerJump);

                uint8_t block = player->dBlock;
                uint8_t row = block / 6;
                player->dx = 5 + j->scene->blockOffset_x + ((block % 6) * 38) + rowOffset[row % 5];
                player->dy = j->scene->blockOffset_y + (row * 28);

                player->flipped = player->sx > player->dx;
            }
            else
            {
                player->state = CHARACTER_IDLE;
            }


        }
    }
}

void drawJumperScene(display_t* d)
{
    jumperCharacter_t* player = j->player;
    jumperCharacter_t* evilDonut = j->evilDonut;
    jumperCharacter_t* blump = j->blump;

    d->clearPx();

    for(uint8_t block = 0; block < 30; block++)
    {
        uint8_t row = block / 6;
        drawWsg(d, &j->block[j->scene->blocks[block]], j->scene->blockOffset_x + ((block % 6) * 38) + rowOffset[row % 5],
                20 + j->scene->blockOffset_y + (row * 28), false, false, 0);

    }

    if (evilDonut->state == CHARACTER_DYING)
    {
        drawWsg(d, &player->frames[7], evilDonut->x, evilDonut->y, false, false, 0);
    }
    else if (evilDonut->state != CHARACTER_DEAD && evilDonut->state != CHARACTER_NONEXISTING)
    {
        drawWsg(d, &evilDonut->frames[evilDonut->frameIndex], evilDonut->x, evilDonut->y, evilDonut->flipped, false, 0);
    }

    if (blump->state == CHARACTER_DYING)
    {
        drawWsg(d, &player->frames[7], blump->x, blump->y, false, false, 0);
    }
    else if (blump->state != CHARACTER_DEAD && blump->state != CHARACTER_NONEXISTING)
    {
        drawWsg(d, &blump->frames[blump->frameIndex], blump->x, blump->y, blump->flipped, false, 0);
    }

    if (player->state != CHARACTER_DEAD)
    {
        drawWsg(d, &player->frames[player->frameIndex], player->x, player->y, player->flipped, false, 0);
    }

    drawJumperEffects(d);
    drawJumperHud(d, j->promptFont, &j->game_font);

}

void drawJumperEffects(display_t* d)
{
    for (int i = 0; i < 3; i++)
    {
        //if statement to see if mutiplier is active
        if (j->multiplier[i].digits[0] == 255)
        {            
            drawWsg(d, &j->digit[11], j->multiplier[i].x, j->multiplier[i].y, false, false, 0);
            
        }

        if (j->multiplier[i].digits[0] >= 11 || j->multiplier[i].digits[0] == 0) continue;

        //if statement to parse numbers
        for(int digit = 0; digit < 3; digit++)
        {
            if (j->multiplier[i].digits[digit] >= 11) 
            {
                break;
            }

            drawWsg(d, &j->digit[j->multiplier[i].digits[digit]], j->multiplier[i].x + (digit * 8), j->multiplier[i].y, false, false, 0);
        }

    }
}

void drawJumperHud(display_t* d, font_t* prompt, font_t* font)
{
    char textBuffer[12];
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", j->scene->level);

    if (j->scene->level < 10)
    {
        drawText(d, font, c555, textBuffer, 230, 220);
    }
    else
    {
        drawText(d, font, c555, textBuffer, 214, 220);
    }

    drawText(d, font, c555, "SCORE", 28, 10);
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", j->scene->score);
    drawText(d, font, c555, textBuffer, 28, 26);

    drawText(d, font, c555, "HI SCORE", 178, 10);
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", j->highScore);
    drawText(d, font, c555, textBuffer, 204, 26);

    for (int i = 0; i < j->scene->lives; i++)
    {
        drawWsg(d, &j->scene->livesIcon, 25 + (i * 11), 220, false, false, 0);
    }


    if (j->currentPhase == JUMPER_COUNTDOWN)
    {
        if (j->scene->seconds <= 0)
        {
        }
        else if (j->scene->seconds <= 3 )
        {
            snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", j->scene->seconds);
            drawText(d, prompt, c000, textBuffer, 142, 128);
            drawText(d, prompt, c555, textBuffer, 140, 128);
        }
        else
        {
            drawText(d, prompt, c000, "Ready?", 102, 128);
            drawText(d, prompt, c555, "Ready?", 100, 128);
        }
    }
    if (j->currentPhase == JUMPER_GAMING)
    {
        if (j->scene->seconds <= 0 && j->scene->seconds > -2)
        {
            drawText(d, prompt, c000, "JUMP!", 122, 129);
            drawText(d, prompt, c555, "JUMP!", 120, 128);
        }
    }
    if (j->currentPhase == JUMPER_GAME_OVER)
    {
        drawText(d, prompt, c000, "GAME OVER", 77, 129);
        drawText(d, prompt, c555, "GAME OVER", 75, 128);
    }
    if (j->currentPhase == JUMPER_WINSTAGE)
    {
        drawText(d, prompt, c000, "JUMP COMPLETE!", 22, 129);
        drawText(d, prompt, c555, "JUMP COMPLETE!", 20, 128);
    }
    

}

void jumperGameButtonCb(buttonEvt_t* evt)
{
    j->player->btnState = evt->state;
}

void jumperExitGame(void)
{
    if (NULL != j)
    {
        //Clear all tiles
        //clear stage

        freeWsg(&j->scene->livesIcon);
        freeFont(&(j->game_font));

        freeWsg(&j->block[0]);
        freeWsg(&j->block[1]);
        freeWsg(&j->block[2]);
        freeWsg(&j->block[3]);
        freeWsg(&j->block[4]);
        freeWsg(&j->block[5]);
        freeWsg(&j->block[6]);
        freeWsg(&j->block[7]);
        freeWsg(&j->block[8]);
        freeWsg(&j->block[9]);

        freeWsg(&j->digit[0]);
        freeWsg(&j->digit[1]);
        freeWsg(&j->digit[2]);
        freeWsg(&j->digit[3]);
        freeWsg(&j->digit[4]);
        freeWsg(&j->digit[5]);
        freeWsg(&j->digit[6]);
        freeWsg(&j->digit[7]);
        freeWsg(&j->digit[8]);
        freeWsg(&j->digit[9]);
        freeWsg(&j->digit[10]);
        freeWsg(&j->digit[11]);

        freeWsg(&j->player->frames[0]);
        freeWsg(&j->player->frames[1]);
        freeWsg(&j->player->frames[2]);
        freeWsg(&j->player->frames[3]);
        freeWsg(&j->player->frames[4]);
        freeWsg(&j->player->frames[5]);
        freeWsg(&j->player->frames[6]);
        freeWsg(&j->player->frames[7]);

        freeWsg( &j->evilDonut->frames[0]);
        freeWsg( &j->evilDonut->frames[1]);
        freeWsg( &j->evilDonut->frames[2]);
        freeWsg( &j->evilDonut->frames[3]);
        freeWsg( &j->evilDonut->frames[4]);

        freeWsg( &j->blump->frames[0]);
        freeWsg( &j->blump->frames[1]);
        freeWsg( &j->blump->frames[2]);
        freeWsg( &j->blump->frames[3]);
        freeWsg( &j->blump->frames[4]);
        freeWsg( &j->blump->frames[5]);

        free(j->scene);

        free(j->multiplier);

        free(j->evilDonut);
        free(j->player);
        free(j->blump);
        free(j);
        j = NULL;
    }
}