//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "jsmn.h"

#include "spiffs_json.h"
#include "fighter_json.h"

//==============================================================================
// Prototypes
//==============================================================================

int32_t parseJsonFighter(          char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   fighter_t* ftr);
int32_t parseJsonAttack(           char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   attack_t* atk);
int32_t parseJsonAttackFrame(      char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   attackFrame_t* frm);
int32_t parseJsonAttackFrameHitbox(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   attackHitbox_t* hbx);
int32_t parseJsonOffsetSprite(     char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   offsetSprite_t* os);

//==============================================================================
// JSON Utility Functions
//==============================================================================

/**
 * @brief Compare a JSMN String to a given string value
 *
 * @param jsonStr The full JSON string
 * @param tok The token for the string to compare
 * @param s The string to compare to
 * @return 0 if the strings match, non-zero value if they do not (see strcmp())
 */
static int32_t jsoneq(const char* jsonStr, jsmntok_t* tok, const char* s)
{
    if (tok->type == JSMN_STRING && (int32_t)strlen(s) == tok->end - tok->start)
    {
        return strncmp(jsonStr + tok->start, s, tok->end - tok->start);
    }
    return -1;
}

/**
 * @brief Extract an integer value from a JSMN primitive
 *
 * @param jsonStr The full JSON string
 * @param tok The token for the integer to extract
 * @return An integer value
 */
static int32_t jsonInteger(const char* jsonStr, jsmntok_t tok)
{
    if(JSMN_PRIMITIVE == tok.type)
    {
        char jsonVal[(tok.end - tok.start) + 1];
        memcpy(jsonVal, &(jsonStr[tok.start]), (tok.end - tok.start));
        jsonVal[(tok.end - tok.start)] = 0;
        return atoi(jsonVal);
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Extract a string value from a JSMN string. This allocates memory which
 * must be free()'d later
 *
 * @param jsonStr The full JSON string
 * @param tok The token for the string to extract
 * @return A pointer to a freshly allocated string
 */
static char* jsonString(const char* jsonStr, jsmntok_t tok)
{
    char* copiedStr = calloc((tok.end - tok.start + 1), sizeof(char));
    memcpy(copiedStr, &(jsonStr[tok.start]), (tok.end - tok.start));
    copiedStr[tok.end - tok.start] = 0;
    return copiedStr;
}

/**
 * @brief Extract a boolean value from a JSMN primitive
 *
 * @param jsonStr The full JSON string
 * @param tok The token for the boolean to extract
 * @return true or false
 */
static bool jsonBoolean(const char* jsonStr, jsmntok_t tok)
{
    return (0 == memcmp("true", &(jsonStr[tok.start]), (tok.end - tok.start))) ? true : false;
}

//==============================================================================
// Parsing Functions
//==============================================================================

/**
 * @brief Parse fighter attributes from a JSON file into the given pointer
 *
 * @param fighter The fighter_t struct to load a fighter into
 * @param jsonFile The JSON file to load data from
 * @param loadedSprites A list of loaded sprites. Will be filled with sprites
 */
void loadJsonFighterData(fighter_t* fighter, const char* jsonFile, namedSprite_t* loadedSprites)
{
    // Load the json string
    char* jsonStr = loadJson(jsonFile);

    // Allocate a bunch of tokens
    jsmn_parser p;
    jsmntok_t* toks = calloc(1024, sizeof(jsmntok_t));
    int32_t tokIdx = 0;

    // Parse the JSON into tokens
    jsmn_init(&p);
    int32_t numToks = jsmn_parse(&p, jsonStr, strlen(jsonStr), toks, 1024);
    if (numToks < 0)
    {
        ESP_LOGE("FTR", "Failed to parse JSON: %d\n", numToks);
    }
    else
    {
        ESP_LOGI("FTR", "numToks = %d", numToks);
    }

    // Load the fighter
    parseJsonFighter(jsonStr, toks, tokIdx, loadedSprites, fighter);

    free(toks);
    free(jsonStr);
}

/**
 * @brief Parse a fighter object from a JSON string
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param ftr The fighter to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonFighter(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites, fighter_t* ftr)
{
    // Each fighter is an object
    if(JSMN_OBJECT != toks[tokIdx].type)
    {
        ESP_LOGE("JSON", "Non-object!!");
    }

    uint8_t numFieldsParsed = 0;
    uint8_t numFieldsToParse = toks[tokIdx].size;

    // Move to the first field
    tokIdx++;

    // Parse the tokens
    while(true)
    {
        if(JSMN_STRING == toks[tokIdx].type)
        {
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "name"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                ESP_LOGD("FTR", "Fighter name is %s", name);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "idle_spr_0"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->idleSprite0);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "idle_spr_1"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->idleSprite1);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "run_spr_0"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->runSprite0);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "run_spr_1"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->runSprite1);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "jump_spr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->jumpSprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "duck_spr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->duckSprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "land_lag_spr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->landingLagSprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hitstun_ground_sprite"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->hitstunGroundSprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hitstun_air_sprite"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &ftr->hitstunAirSprite);
                free(name);
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "gravity"))
            {
                tokIdx++;
                ftr->gravity = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "weight"))
            {
                tokIdx++;
                ftr->weight = jsonInteger(jsonStr, toks[tokIdx]);
                if(0 < ftr->weight && ftr->weight < 2048)
                {
                    ftr->weight = 2048 - ftr->weight;
                }
                else
                {
                    ftr->weight = 1024;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "jump_velo"))
            {
                tokIdx++;
                ftr->jump_velo = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_accel"))
            {
                tokIdx++;
                ftr->run_accel = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_decel"))
            {
                tokIdx++;
                ftr->run_decel = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_max_velo"))
            {
                tokIdx++;
                ftr->run_max_velo = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_x"))
            {
                tokIdx++;
                ftr->size.x = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                ftr->originalSize.x = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_y"))
            {
                tokIdx++;
                ftr->size.y = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                ftr->originalSize.y = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "nJumps"))
            {
                tokIdx++;
                ftr->numJumps = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "landing_lag"))
            {
                tokIdx++;
                // Convert ms to frames
                ftr->landingLag = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == ftr->landingLag)
                {
                    ftr->landingLag = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "stock_icn"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                ftr->stockIconIdx = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "attacks"))
            {
                // Move to the array, get the size
                tokIdx++;
                uint8_t numAttacks = toks[tokIdx].size;

                // Check length
                if(numAttacks != NUM_ATTACKS)
                {
                    ESP_LOGE("JSON", "Incorrect number of attacks");
                }

                tokIdx++;
                for(uint8_t atkIdx = 0; atkIdx < numAttacks; atkIdx++)
                {
                    tokIdx = parseJsonAttack(jsonStr, toks, tokIdx, loadedSprites, ftr->attacks);
                }
                numFieldsParsed++;
            }
        }
        else
        {
            ESP_LOGE("JSON", "Non-string key!!");
        }

        // Check to return
        if(numFieldsParsed == numFieldsToParse)
        {
            return tokIdx;
        }
    }
    return tokIdx;
}

/**
 * @brief Parse an attack object from a JSON string
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param atks A pointer to all the attacks
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttack(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites, attack_t* atks)
{
    // Each attack is an object
    if(JSMN_OBJECT != toks[tokIdx].type)
    {
        ESP_LOGE("JSON", "Non-object!!");
    }

    // Keep track of fields so we know when we're done
    uint8_t numFieldsParsed = 0;
    uint8_t numFieldsToParse = toks[tokIdx].size;

    // Move to the first field
    tokIdx++;

    // The attack to parse into
    attack_t* atk = NULL;

    // Parse the tokens
    while(true)
    {
        if(JSMN_STRING == toks[tokIdx].type)
        {
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "type"))
            {
                tokIdx++;
                char* type = jsonString(jsonStr, toks[tokIdx]);

                // Pick the attack slot based on type
                if(0 == strcmp(type, "up_gnd"))
                {
                    atk = &atks[UP_GROUND];
                }
                else if(0 == strcmp(type, "down_gnd"))
                {
                    atk = &atks[DOWN_GROUND];
                }
                else if(0 == strcmp(type, "dash_gnd"))
                {
                    atk = &atks[DASH_GROUND];
                }
                else if(0 == strcmp(type, "front_gnd"))
                {
                    atk = &atks[FRONT_GROUND];
                }
                else if(0 == strcmp(type, "neutral_gnd"))
                {
                    atk = &atks[NEUTRAL_GROUND];
                }
                else if(0 == strcmp(type, "neutral_air"))
                {
                    atk = &atks[NEUTRAL_AIR];
                }
                else if(0 == strcmp(type, "front_air"))
                {
                    atk = &atks[FRONT_AIR];
                }
                else if(0 == strcmp(type, "back_air"))
                {
                    atk = &atks[BACK_AIR];
                }
                else if(0 == strcmp(type, "up_air"))
                {
                    atk = &atks[UP_AIR];
                }
                else if(0 == strcmp(type, "down_air"))
                {
                    atk = &atks[DOWN_AIR];
                }

                free(type);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "startupLag"))
            {
                tokIdx++;
                // Convert ms to frames
                atk->startupLag = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == atk->startupLag)
                {
                    atk->startupLag = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "endLag"))
            {
                tokIdx++;
                // Convert ms to frames
                atk->endLag = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == atk->endLag)
                {
                    atk->endLag = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "landing_lag"))
            {
                tokIdx++;
                // Convert ms to frames
                atk->landingLag = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == atk->landingLag)
                {
                    atk->landingLag = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "startupLagSpr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &atk->startupLagSprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "endLagSpr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &atk->endLagSprite);
                free(name);
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "onlyFirstHit"))
            {
                tokIdx++;
                atk->onlyFirstHit = jsonBoolean(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "iframe_timer"))
            {
                tokIdx++;
                // Convert ms to frames
                atk->iFrames = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "attack_frames"))
            {
                // Move to the array, get the size
                tokIdx++;
                atk->numAttackFrames = toks[tokIdx].size;

                // Allocate memory
                atk->attackFrames = calloc(atk->numAttackFrames, sizeof(attackFrame_t));

                tokIdx++;
                for(uint8_t frmIdx = 0; frmIdx < atk->numAttackFrames; frmIdx++)
                {
                    tokIdx = parseJsonAttackFrame(jsonStr, toks, tokIdx, loadedSprites, &atk->attackFrames[frmIdx]);
                }
                numFieldsParsed++;
            }
        }
        else
        {
            ESP_LOGE("JSON", "Non-string key!!");
        }

        // Check to return
        if(numFieldsParsed == numFieldsToParse)
        {
            return tokIdx;
        }
    }
    return tokIdx;
}

/**
 * @brief Parse an attack frame object from a JSON string
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param ftr The attack frame to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttackFrame(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                             attackFrame_t* frm)
{
    // Each attack is an object
    if(JSMN_OBJECT != toks[tokIdx].type)
    {
        ESP_LOGE("JSON", "Non-object!!");
    }

    // Keep track of fields so we know when we're done
    uint8_t numFieldsParsed = 0;
    uint8_t numFieldsToParse = toks[tokIdx].size;

    // Move to the first field
    tokIdx++;

    // Parse the tokens
    while(true)
    {
        if(JSMN_STRING == toks[tokIdx].type)
        {
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "duration"))
            {
                tokIdx++;
                // Convert ms to frames
                frm->duration = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == frm->duration)
                {
                    frm->duration = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "atkSpr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &frm->sprite);
                free(name);
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hurtbox_offset_x"))
            {
                tokIdx++;
                frm->hurtbox_offset.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hurtbox_offset_y"))
            {
                tokIdx++;
                frm->hurtbox_offset.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hurtbox_size_x"))
            {
                tokIdx++;
                frm->hurtbox_size.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hurtbox_size_y"))
            {
                tokIdx++;
                frm->hurtbox_size.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "iframe_timer"))
            {
                tokIdx++;
                // Convert ms to frames
                frm->iFrames = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "velo_x"))
            {
                tokIdx++;
                frm->velocity.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "velo_y"))
            {
                tokIdx++;
                frm->velocity.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "hitboxes"))
            {
                // Move to the array, get the size
                tokIdx++;
                frm->numHitboxes = toks[tokIdx].size;

                // Allocate memory
                frm->hitboxes = calloc(frm->numHitboxes, sizeof(attackHitbox_t));

                // Move to the first object and start parsing
                tokIdx++;
                for(uint8_t hbxIdx = 0; hbxIdx < frm->numHitboxes; hbxIdx++)
                {
                    tokIdx = parseJsonAttackFrameHitbox(jsonStr, toks, tokIdx, loadedSprites, &frm->hitboxes[hbxIdx]);
                }
                numFieldsParsed++;
            }
        }
        else
        {
            ESP_LOGE("JSON", "Non-string key!!");
        }

        // Check to return
        if(numFieldsParsed == numFieldsToParse)
        {
            return tokIdx;
        }
    }
    return tokIdx;
}

/**
 * @brief Parse an attack hitbox object from a JSON string
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param ftr The attack hitbox to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttackFrameHitbox(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, namedSprite_t* loadedSprites,
                                   attackHitbox_t* hbx)
{
    // Each attack is an object
    if(JSMN_OBJECT != toks[tokIdx].type)
    {
        ESP_LOGE("JSON", "Non-object!!");
    }

    // Keep track of fields so we know when we're done
    uint8_t numFieldsParsed = 0;
    uint8_t numFieldsToParse = toks[tokIdx].size;

    // Move to the first field
    tokIdx++;

    // Parse the tokens
    while(true)
    {
        if(JSMN_STRING == toks[tokIdx].type)
        {
            if(0 == jsoneq(jsonStr, &toks[tokIdx], "relativePos_x"))
            {
                tokIdx++;
                hbx->hitboxPos.x = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "relativePos_y"))
            {
                tokIdx++;
                hbx->hitboxPos.y = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_x"))
            {
                tokIdx++;
                hbx->hitboxSize.x = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_y"))
            {
                tokIdx++;
                hbx->hitboxSize.y = jsonInteger(jsonStr, toks[tokIdx]) << SF;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "damage"))
            {
                tokIdx++;
                hbx->damage = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "knockback_x"))
            {
                tokIdx++;
                hbx->knockback.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "knockback_y"))
            {
                tokIdx++;
                hbx->knockback.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "hitstun"))
            {
                tokIdx++;
                // Convert ms to frames
                hbx->hitstun = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                // Allow 0 frames hitstun
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "isProjectile"))
            {
                tokIdx++;
                hbx->isProjectile = jsonBoolean(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileSprite"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                tokIdx = parseJsonOffsetSprite(jsonStr, toks, tokIdx, loadedSprites, &hbx->projSprite);
                free(name);
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileDuration"))
            {
                tokIdx++;
                // Convert ms to frames
                hbx->projDuration = jsonInteger(jsonStr, toks[tokIdx]) / FRAME_TIME_MS;
                if(0 == hbx->projDuration)
                {
                    hbx->projDuration = 1;
                }
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileVelo_x"))
            {
                tokIdx++;
                hbx->projVelo.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileVelo_y"))
            {
                tokIdx++;
                hbx->projVelo.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileAccel_x"))
            {
                tokIdx++;
                hbx->projAccel.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileAccel_y"))
            {
                tokIdx++;
                hbx->projAccel.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectilePassThru"))
            {
                tokIdx++;
                hbx->projPassThru = jsonBoolean(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
        }
        else
        {
            ESP_LOGE("JSON", "Non-string key!!");
        }

        // Check to return
        if(numFieldsParsed == numFieldsToParse)
        {
            return tokIdx;
        }
    }
    return tokIdx;
}

/**
 * @brief Parse an offset sprite JSON object
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param os The offset sprite to load data into
 * @return The index of the JSON token after parsing
 */
int32_t parseJsonOffsetSprite(char* jsonStr, jsmntok_t* toks, int32_t tokIdx,
                              namedSprite_t* loadedSprites, offsetSprite_t* os)
{
    // Each attack is an object
    if(JSMN_OBJECT != toks[tokIdx].type)
    {
        ESP_LOGE("JSON", "Non-object!!");
    }

    // Keep track of fields so we know when we're done
    uint8_t numFieldsParsed = 0;
    uint8_t numFieldsToParse = toks[tokIdx].size;

    // Move to the first field
    tokIdx++;

    // Parse the tokens
    while(true)
    {
        if(JSMN_STRING == toks[tokIdx].type)
        {
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "sprite"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                os->spriteIdx = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "off_x"))
            {
                tokIdx++;
                os->offset.x = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "off_y"))
            {
                tokIdx++;
                os->offset.y = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
        }
        else
        {
            ESP_LOGE("JSON", "Non-string key!!");
        }

        // Check to return
        if(numFieldsParsed == numFieldsToParse)
        {
            return tokIdx;
        }
    }
    return tokIdx;
}

/**
 * @brief Free all the data associated with an array of fighters
 *
 * @param fighters A pointer to fighter data to free
 * @param numFighters The number of fighters to free
 */
void freeFighterData(fighter_t* fighters, uint8_t numFighters)
{
    for(uint8_t ftrIdx = 0; ftrIdx < numFighters; ftrIdx++)
    {
        fighter_t* ftr = &fighters[ftrIdx];
        for(uint8_t atkIdx = 0; atkIdx < NUM_ATTACKS; atkIdx++)
        {
            attack_t* atk = &ftr->attacks[atkIdx];
            for(uint8_t atkFrameIdx = 0; atkFrameIdx < atk->numAttackFrames; atkFrameIdx++)
            {
                attackFrame_t* frm = &atk->attackFrames[atkFrameIdx];
                free(frm->hitboxes);
            }
            free(atk->attackFrames);
        }
        // attacks isn't dynamically allocated
    }
    // Fighters aren't dynamically allocated
}

/**
 * Load a sprite, or return a pointer to that sprite if it's already loaded.
 * When loading a sprite, add it to loadedSprites.
 *
 * @param name The name of the sprite to load
 * @param loadedSprites A linked list of loaded sprites
 * @return A sprite index
 */
uint8_t loadFighterSprite(char* name, namedSprite_t* loadedSprites)
{
    // Iterate over the loaded sprites
    for(int16_t idx = 0; idx < MAX_LOADED_SPRITES; idx++)
    {
        // Check if the name is NULL
        if(NULL == loadedSprites[idx].name)
        {
            // If so, we've reached the end and should load this sprite
            if(loadWsgSpiRam(name, &loadedSprites[idx].sprite, true))
            {
                // If the sprite loads, save the name
                loadedSprites[idx].name = calloc(1, strlen(name) + 1);
                memcpy(loadedSprites[idx].name, name, strlen(name) + 1);
            }
            // Return the index
            return idx;
        }
        else if(0 == strcmp(loadedSprites[idx].name, name))
        {
            // Name matches, so return this loaded sprite
            return idx;
        }
    }
    // Should be impossible to get here
    ESP_LOGE("JSON", "Couldn't load sprite");
    return 0;
}

/**
 * Get a sprite given the sprite's index
 *
 * @param spriteIdx The index to get a sprite for
 * @param loadedSprites A list of loaded sprites
 * @return wsg_t* The sprite that corresponds to this index
 */
wsg_t* getFighterSprite(uint8_t spriteIdx, namedSprite_t* loadedSprites)
{
    return &(loadedSprites[spriteIdx].sprite);
}

/**
 * Free all loaded sprites
 *
 * @param loadedSprites a list of sprites to free
 */
void freeFighterSprites(namedSprite_t* loadedSprites)
{
    // Free all sprites and names
    for(int16_t idx = 0; idx < MAX_LOADED_SPRITES; idx++)
    {
        if(NULL != loadedSprites[idx].name)
        {
            free(loadedSprites[idx].name);
            loadedSprites[idx].name = NULL;
            freeWsg(&loadedSprites[idx].sprite);
        }
    }
}
