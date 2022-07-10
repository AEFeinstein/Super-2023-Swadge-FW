//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "jsmn.h"

#include "spiffs_json.h"
#include "fighter_json.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    char* name;
    wsg_t sprite;
} namedSprite_t;

//==============================================================================
// Prototypes
//==============================================================================

int32_t parseJsonFighter(          char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites,
                                   fighter_t* fgt);
int32_t parseJsonAttack(           char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites,
                                   attack_t* atk);
int32_t parseJsonAttackFrame(      char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites,
                                   attackFrame_t* frm);
int32_t parseJsonAttackFrameHitbox(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites,
                                   attackHitbox_t* hbx);

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
 * @brief Parse fighter attributes from a JSON file and return them in struct form
 *
 * @param numFighters The number of fighters will be written to this pointer
 * @param loadedSprites A list of loaded sprites. Will be filled with sprites
 * @return A pointer to the allocated fighter attribute data
 */
fighter_t* loadJsonFighterData(uint8_t* numFighters, list_t* loadedSprites)
{
    // Load the json string
    char* jsonStr = loadJson("test.json");

    // Allocate a bunch of tokens
    jsmn_parser p;
    jsmntok_t* toks = calloc(1024, sizeof(jsmntok_t));
    int32_t tokIdx = 0;

    // Parse the JSON into tokens
    jsmn_init(&p);
    int32_t numToks = jsmn_parse(&p, jsonStr, strlen(jsonStr), toks, 1024);
    if (numToks < 0)
    {
        ESP_LOGE("FGT", "Failed to parse JSON: %d\n", numToks);
        free(toks);
        return NULL;
    }
    else
    {
        ESP_LOGI("FGT", "numToks = %d", numToks);
    }

    // Pointer to allocate memory for later
    fighter_t* fighters;

    ////////////////////////////////////////////////////////////////////////////

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
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "fighters"))
            {
                tokIdx++;

                // Allocate space for the fighters
                *numFighters = toks[tokIdx].size;
                fighters = calloc(sizeof(fighter_t), *numFighters);

                // Move to the next token
                tokIdx++;

                // Parse each fighter
                for(int32_t fighterIdx = 0; fighterIdx < *numFighters; fighterIdx++)
                {
                    // Sum up the number of tokens this fighter parsed
                    tokIdx = parseJsonFighter(jsonStr, toks, tokIdx, loadedSprites, &fighters[fighterIdx]);
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
            return fighters;
        }
    }
    return fighters;
}

/**
 * @brief Parse a fighter object from a JSON string
 *
 * @param jsonStr The whole JSON string
 * @param toks An array of JSON tokens
 * @param tokIdx The index of the current JSON token
 * @param loadedSprites A list of sprites, used for loading
 * @param fgt The fighter to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonFighter(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites, fighter_t* fgt)
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
                ESP_LOGD("FGT", "Fighter name is %s", name);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "idle_spr_0"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->idleSprite0 = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "idle_spr_1"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->idleSprite1 = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "run_spr_0"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->runSprite0 = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "run_spr_1"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->runSprite1 = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "jump_spr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->jumpSprite = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "duck_spr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                fgt->duckSprite = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "gravity"))
            {
                tokIdx++;
                fgt->gravity = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "jump_velo"))
            {
                tokIdx++;
                fgt->jump_velo = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_accel"))
            {
                tokIdx++;
                fgt->run_accel = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_decel"))
            {
                tokIdx++;
                fgt->run_decel = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "run_max_velo"))
            {
                tokIdx++;
                fgt->run_max_velo = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_x"))
            {
                tokIdx++;
                fgt->size.x = SF * jsonInteger(jsonStr, toks[tokIdx]);
                fgt->hurtbox.x0 = 0;
                fgt->hurtbox.x1 = fgt->hurtbox.x0 + fgt->size.x;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_y"))
            {
                tokIdx++;
                fgt->size.y = SF * jsonInteger(jsonStr, toks[tokIdx]);
                fgt->hurtbox.y0 = 0;
                fgt->hurtbox.y1 = fgt->hurtbox.y0 + fgt->size.y;
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "nJumps"))
            {
                tokIdx++;
                fgt->numJumps = jsonInteger(jsonStr, toks[tokIdx]);
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
                    tokIdx = parseJsonAttack(jsonStr, toks, tokIdx, loadedSprites, &fgt->attacks[atkIdx]);
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
 * @param atk The attack to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttack(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites, attack_t* atk)
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
            if (0 == jsoneq(jsonStr, &toks[tokIdx], "type"))
            {
                tokIdx++;
                char* type = jsonString(jsonStr, toks[tokIdx]);
                ESP_LOGD("FGT", "Parsing %s", type);
                free(type);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "startupLag"))
            {
                tokIdx++;
                atk->startupLag = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "endLag"))
            {
                tokIdx++;
                atk->endLag = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "startupLagSpr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                atk->startupLagSpr = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "endLagSpr"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                atk->endLagSpr = loadFighterSprite(name, loadedSprites);
                free(name);
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
 * @param fgt The attack frame to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttackFrame(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites, attackFrame_t* frm)
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
                frm->duration = jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if (0 == jsoneq(jsonStr, &toks[tokIdx], "sprite"))
            {
                tokIdx++;
                char* name = jsonString(jsonStr, toks[tokIdx]);
                frm->sprite = loadFighterSprite(name, loadedSprites);
                free(name);
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
 * @param fgt The attack hitbox to parse data into
 * @return int32_t The index of the JSON token after parsing
 */
int32_t parseJsonAttackFrameHitbox(char* jsonStr, jsmntok_t* toks, int32_t tokIdx, list_t* loadedSprites,
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
                hbx->hitboxPos.x = SF * jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "relativePos_y"))
            {
                tokIdx++;
                hbx->hitboxPos.y = SF * jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_x"))
            {
                tokIdx++;
                hbx->hitboxSize.x = SF * jsonInteger(jsonStr, toks[tokIdx]);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "size_y"))
            {
                tokIdx++;
                hbx->hitboxSize.y = SF * jsonInteger(jsonStr, toks[tokIdx]);
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
                hbx->hitstun = jsonInteger(jsonStr, toks[tokIdx]);
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
                hbx->projSprite = loadFighterSprite(name, loadedSprites);
                free(name);
                tokIdx++;
                numFieldsParsed++;
            }
            else if(0 == jsoneq(jsonStr, &toks[tokIdx], "projectileDuration"))
            {
                tokIdx++;
                hbx->projDuration = jsonInteger(jsonStr, toks[tokIdx]);
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
    for(uint8_t fgtIdx = 0; fgtIdx < numFighters; fgtIdx++)
    {
        fighter_t* fgt = &fighters[fgtIdx];
        for(uint8_t atkIdx = 0; atkIdx < NUM_ATTACKS; atkIdx++)
        {
            attack_t* atk = &fgt->attacks[atkIdx];
            for(uint8_t atkFrameIdx = 0; atkFrameIdx < atk->numAttackFrames; atkFrameIdx++)
            {
                attackFrame_t* frm = &atk->attackFrames[atkFrameIdx];
                // for(uint8_t hbIdx = 0; hbIdx < frm->numHitboxes; hbIdx++)
                // {
                //     attackHitbox_t * hbx = &frm->hitboxes[hbIdx];
                // }
                free(frm->hitboxes);
            }
            free(atk->attackFrames);
        }
        // attacks isn't dynamically allocated
    }
    free(fighters);
}

/**
 * Load a sprite, or return a pointer to that sprite if it's already loaded.
 * When loading a sprite, add it to loadedSprites.
 *
 * @param name The name of the sprite to load
 * @param loadedSprites A linked list of loaded sprites
 * @return A pointer to a wsg_t sprite
 */
wsg_t* loadFighterSprite(char* name, list_t* loadedSprites)
{
    // Iterate through the list of loaded sprites and look to see if this
    // has been loaded already. If so, return it
    node_t* currentNode = loadedSprites->first;
    while (currentNode != NULL)
    {
        if(0 == strcmp(((namedSprite_t*)currentNode->val)->name, name))
        {
            // Name matches, so return this loaded sprite
            return &((namedSprite_t*)currentNode->val)->sprite;
        }
        // Name didn't match, so iterate
        currentNode = currentNode->next;
    }

    // Made it this far, which means it isn't loaded yet.
    // Allocate a new sprite
    namedSprite_t* newSprite = calloc(1, sizeof(namedSprite_t));

    // Load the sprite
    loadWsg(name, &(newSprite->sprite));
    // Copy the name
    newSprite->name = calloc(1, strlen(name) + 1);
    memcpy(newSprite->name, name, strlen(name) + 1);

    // Add the loaded sprite to the list
    push(loadedSprites, newSprite);

    // Return the loaded sprite
    return &(newSprite->sprite);
}

/**
 * Free all loaded sprites
 *
 * @param loadedSprites a list of sprites to free
 */
void freeFighterSprites(list_t* loadedSprites)
{
    // Pop and free all sprites
    namedSprite_t* toFree;
    while (NULL != (toFree = pop(loadedSprites)))
    {
        // Free the fields
        freeWsg(&(toFree->sprite));
        free(toFree->name);
        // Free the named sprite
        free(toFree);
    }
}
