#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "jsmn.h"

#include "spiffs_json.h"
#include "fighter_json.h"

typedef enum
{
    PARSING_ROOT,
    PARSING_FIGHTERS,
    PARSING_FIGHTER_NAME,

    PARSING_FIGHTER_ATTRS,
    PARSING_FIGHTER_ATTR_GRAVITY,
    PARSING_FIGHTER_ATTR_JUMP_VELO,
    PARSING_FIGHTER_ATTR_RUN_ACCEL,
    PARSING_FIGHTER_ATTR_RUN_DECEL,
    PARSING_FIGHTER_ATTR_RUN_MAX_VELO,
    PARSING_FIGHTER_ATTR_SIZE_X,
    PARSING_FIGHTER_ATTR_SIZE_Y,
    PARSING_FIGHTER_ATTR_NJUMPS,
    PARSING_FIGHTER_ATTR_IDLE_SPR_0,
    PARSING_FIGHTER_ATTR_IDLE_SPR_1,
    PARSING_FIGHTER_ATTR_JUMP_SPR,
    PARSING_FIGHTER_ATTR_DUCK_SPR,
    PARSING_FIGHTER_ATTR_MOVES,

    PARSING_FIGHTER_ATTACK_ATTRS,

    PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG,
    PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG_SPR,
    PARSING_FIGHTER_ATTR_ATTACK_END_LAG,
    PARSING_FIGHTER_ATTR_ATTACK_END_LAG_SPR,
    PARSING_FIGHTER_ATTR_ATTACK_ACTIVE_STATES,

    PARSING_FIGHTER_ATTACK_FRAME_ATTRS,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_DURATION,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_X,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_Y,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_X,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_Y,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_DAMAGE,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_X,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_Y,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_HITSTUN,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_SPRITE,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_ISPROJECTILE,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILESPRITE,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEDURATION,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_X,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_Y,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_X,
    PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_Y
} fgtParseState_t;

const char* attackOrder[] =
{
    "utilt",
    "dtilt",
    "dash",
    "neutral",
    "nair",
    "fair",
    "bair",
    "uair",
    "dair"
};

/**
 * @brief TODO
 *
 * @param jsonStr
 * @param tok
 * @param s
 * @return int
 */
static int jsoneq(const char* jsonStr, jsmntok_t* tok, const char* s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
            strncmp(jsonStr + tok->start, s, tok->end - tok->start) == 0)
    {
        return 0;
    }
    return -1;
}

/**
 * @brief TODO
 *
 * @param jsonStr
 * @param tok
 * @return int
 */
static int jsonInteger(const char* jsonStr, jsmntok_t tok)
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
 * @brief TODO
 *
 * @param jsonStr
 * @param tok
 * @return char*
 */
static char* jsonString(const char* jsonStr, jsmntok_t tok)
{
    char* copiedStr = malloc(sizeof(char) * (tok.end - tok.start + 1));
    memcpy(copiedStr, &(jsonStr[tok.start]), (tok.end - tok.start));
    copiedStr[tok.end - tok.start] = 0;
    return copiedStr;
}

/**
 * @brief TODO
 *
 * @param jsonStr
 * @param tok
 * @return char*
 */
static bool jsonBoolean(const char* jsonStr, jsmntok_t tok)
{
    return (0 == memcmp("true", &(jsonStr[tok.start]), (tok.end - tok.start))) ? true : false;
}

/**
 * @brief TODO
 *
 * @param numFighters
 * @return fighter_t*
 */
fighter_t* loadJsonFighterData(uint8_t* numFighters)
{
    char* jsonStr = loadJson("test.json");
    jsmn_parser p;
    jsmntok_t t[1024];

    jsmn_init(&p);
    int r = jsmn_parse(&p, jsonStr, strlen(jsonStr), t, sizeof(t) / sizeof(t[0]));
    if (r < 0)
    {
        ESP_LOGE("FGT", "Failed to parse JSON: %d\n", r);
        return NULL;
    }
    else
    {
        ESP_LOGI("FGT", "r = %d", r);
    }

    fgtParseState_t ps = PARSING_ROOT;
    fighter_t* fighters = NULL;
    fighter_t* cFighter = NULL;
    attack_t* cAttack = NULL;
    uint8_t cAttackIdx = -1;

    uint8_t fighterAttrsToParse = 0;
    uint8_t fighterAttrsParsed = 0;

    uint8_t numAttacksToParse = 0;
    uint8_t attacksParsed = 0;

    uint8_t attackAttrsToParse = 0;
    uint8_t attackAttrsParsed = 0;

    uint8_t activeStateAttrsToParse = 0;
    uint8_t activeStateAttrsParsed = 0;

    // t[i].size is >0 for keys, == 0 for vals
    // All keys are JSMN_STRING
    // JSMN_STRING can be either a key or value
    for (int i = 1; i < r; i++)
    {
        switch(ps)
        {
            default:
            {
                break;
            }
            case PARSING_ROOT:
            {
                if (jsoneq(jsonStr, &t[i], "fighters") == 0)
                {
                    ps = PARSING_FIGHTERS;
                }
                break;
            }
            case PARSING_FIGHTERS:
            {
                // Allocate space for fighter data
                *numFighters = t[i].size;
                fighters = malloc(sizeof(fighter_t) * t[i].size);
                memset(fighters, 0, sizeof(fighter_t) * t[i].size);
                // Set the current fighters
                cFighter = fighters;
                ps = PARSING_FIGHTER_NAME;
                break;
            }
            case PARSING_FIGHTER_NAME:
            {
                // TODO reset cFighter?
                ESP_LOGD("FGT", "Fighter name is %.*s\n", t[i].end - t[i].start, jsonStr + t[i].start);
                ps = PARSING_FIGHTER_ATTRS;
                break;
            }
            case PARSING_FIGHTER_ATTRS:
            {
                if(JSMN_OBJECT == t[i].type)
                {
                    fighterAttrsToParse = t[i].size;
                    fighterAttrsParsed = 0;
                }
                else if (jsoneq(jsonStr, &t[i], "gravity") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_GRAVITY;
                }
                else if (jsoneq(jsonStr, &t[i], "jump_velo") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_JUMP_VELO;
                }
                else if (jsoneq(jsonStr, &t[i], "run_accel") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_RUN_ACCEL;
                }
                else if (jsoneq(jsonStr, &t[i], "run_decel") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_RUN_DECEL;
                }
                else if (jsoneq(jsonStr, &t[i], "run_max_velo") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_RUN_MAX_VELO;
                }
                else if (jsoneq(jsonStr, &t[i], "size_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_SIZE_X;
                }
                else if (jsoneq(jsonStr, &t[i], "size_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_SIZE_Y;
                }
                else if (jsoneq(jsonStr, &t[i], "nJumps") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_NJUMPS;
                }
                else if (jsoneq(jsonStr, &t[i], "idle_spr_0") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_IDLE_SPR_0;
                }
                else if (jsoneq(jsonStr, &t[i], "idle_spr_1") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_IDLE_SPR_1;
                }
                else if (jsoneq(jsonStr, &t[i], "jump_spr") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_JUMP_SPR;
                }
                else if (jsoneq(jsonStr, &t[i], "duck_spr") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_DUCK_SPR;
                }
                else if (jsoneq(jsonStr, &t[i], "attacks") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_MOVES;
                }
                break;
            }
            case PARSING_FIGHTER_ATTR_GRAVITY:
            case PARSING_FIGHTER_ATTR_JUMP_VELO:
            case PARSING_FIGHTER_ATTR_RUN_ACCEL:
            case PARSING_FIGHTER_ATTR_RUN_DECEL:
            case PARSING_FIGHTER_ATTR_RUN_MAX_VELO:
            case PARSING_FIGHTER_ATTR_SIZE_X:
            case PARSING_FIGHTER_ATTR_SIZE_Y:
            case PARSING_FIGHTER_ATTR_NJUMPS:
            case PARSING_FIGHTER_ATTR_IDLE_SPR_0:
            case PARSING_FIGHTER_ATTR_IDLE_SPR_1:
            case PARSING_FIGHTER_ATTR_JUMP_SPR:
            case PARSING_FIGHTER_ATTR_DUCK_SPR:
            {
                switch(ps)
                {
                    default:
                    {
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_GRAVITY:
                    {
                        cFighter->gravity = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_JUMP_VELO:
                    {
                        cFighter->jump_velo = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_RUN_ACCEL:
                    {
                        cFighter->run_accel = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_RUN_DECEL:
                    {
                        cFighter->run_decel = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_RUN_MAX_VELO:
                    {
                        cFighter->run_max_velo = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_SIZE_X:
                    {
                        cFighter->size.x = SF * jsonInteger(jsonStr, t[i]);
                        cFighter->hurtbox.x0 = 0;
                        cFighter->hurtbox.x1 = cFighter->hurtbox.x0 + cFighter->size.x;
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_SIZE_Y:
                    {
                        cFighter->size.y = SF * jsonInteger(jsonStr, t[i]);
                        cFighter->hurtbox.y0 = 0;
                        cFighter->hurtbox.y1 = cFighter->hurtbox.y0 + cFighter->size.y;
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_NJUMPS:
                    {
                        cFighter->numJumps = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_IDLE_SPR_0:
                    {
                        cFighter->idleSprite0 = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_IDLE_SPR_1:
                    {
                        cFighter->idleSprite1 = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_JUMP_SPR:
                    {
                        cFighter->jumpSprite = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_DUCK_SPR:
                    {
                        cFighter->duckSprite = jsonString(jsonStr, t[i]);
                        break;
                    }
                }

                // Check to see if all attributes were parsed
                if((++fighterAttrsParsed) == fighterAttrsToParse)
                {
                    cFighter++;
                    ps = PARSING_FIGHTER_NAME;
                }
                else
                {
                    ps = PARSING_FIGHTER_ATTRS;
                }
                break;
            }
            case PARSING_FIGHTER_ATTR_MOVES:
            {
                if(JSMN_OBJECT == t[i].type)
                {
                    numAttacksToParse = t[i].size;
                    attacksParsed = 0;
                }
                else
                {
                    // Figure out which attack this is to store data
                    for(uint8_t atkIdx = 0; atkIdx < sizeof(attackOrder) / sizeof(attackOrder[0]); atkIdx++)
                    {
                        if (jsoneq(jsonStr, &t[i], attackOrder[atkIdx]) == 0)
                        {
                            cAttack = &(cFighter->attacks[atkIdx]);
                            ps = PARSING_FIGHTER_ATTACK_ATTRS;
                            break;
                        }
                    }
                }
                break;
            }
            case PARSING_FIGHTER_ATTACK_ATTRS:
            {
                if(JSMN_OBJECT == t[i].type)
                {
                    attackAttrsToParse = t[i].size;
                    attackAttrsParsed = 0;
                }
                else if (jsoneq(jsonStr, &t[i], "startupLag") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG;
                }
                else if (jsoneq(jsonStr, &t[i], "startupLagSpr") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG_SPR;
                }
                else if (jsoneq(jsonStr, &t[i], "endLag") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_ATTACK_END_LAG;
                }
                else if (jsoneq(jsonStr, &t[i], "endLagSpr") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_ATTACK_END_LAG_SPR;
                }
                else if (jsoneq(jsonStr, &t[i], "active_states") == 0)
                {
                    ps = PARSING_FIGHTER_ATTR_ATTACK_ACTIVE_STATES;
                }
                break;
            }
            case PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG:
            case PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG_SPR:
            case PARSING_FIGHTER_ATTR_ATTACK_ACTIVE_STATES:
            case PARSING_FIGHTER_ATTR_ATTACK_END_LAG:
            case PARSING_FIGHTER_ATTR_ATTACK_END_LAG_SPR:
            {
                switch(ps)
                {
                    default:
                    {
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG:
                    {
                        cAttack->startupLag = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_ATTACK_STARTUP_LAG_SPR:
                    {
                        cAttack->startupLagSpr = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_ATTACK_ACTIVE_STATES:
                    {
                        cAttack->numAttackFrames = t[i].size;
                        cAttack->attackFrames = malloc(sizeof(attackFrame_t) * t[i].size);
                        memset(cAttack->attackFrames, 0, sizeof(attackFrame_t) * t[i].size);
                        cAttackIdx = -1;
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_ATTACK_END_LAG:
                    {
                        cAttack->endLag = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTR_ATTACK_END_LAG_SPR:
                    {
                        cAttack->endLagSpr = jsonString(jsonStr, t[i]);
                        break;
                    }
                }

                // Handle state transition
                if(ps == PARSING_FIGHTER_ATTR_ATTACK_ACTIVE_STATES)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTRS;
                    attackAttrsParsed++;
                }
                else if((++attackAttrsParsed) == attackAttrsToParse)
                {
                    if((++attacksParsed) == numAttacksToParse)
                    {
                        if((++fighterAttrsParsed) == fighterAttrsToParse)
                        {
                            cFighter++;
                            ps = PARSING_FIGHTER_NAME;
                        }
                        else
                        {
                            ps = PARSING_FIGHTER_ATTRS;
                        }
                    }
                    else
                    {
                        ps = PARSING_FIGHTER_ATTR_MOVES;
                    }
                }
                else
                {
                    ps = PARSING_FIGHTER_ATTACK_ATTRS;
                }
                break;
            }
            case PARSING_FIGHTER_ATTACK_FRAME_ATTRS:
            {
                if(JSMN_OBJECT == t[i].type)
                {
                    cAttackIdx++;
                    activeStateAttrsToParse = t[i].size;
                    activeStateAttrsParsed = 0;
                }
                else if (jsoneq(jsonStr, &t[i], "duration") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_DURATION;
                }
                else if (jsoneq(jsonStr, &t[i], "relativePos_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_X;
                }
                else if (jsoneq(jsonStr, &t[i], "relativePos_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_Y;
                }
                else if (jsoneq(jsonStr, &t[i], "size_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_X;
                }
                else if (jsoneq(jsonStr, &t[i], "size_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_Y;
                }
                else if (jsoneq(jsonStr, &t[i], "damage") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_DAMAGE;
                }
                else if (jsoneq(jsonStr, &t[i], "knockback") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK;
                }
                else if (jsoneq(jsonStr, &t[i], "knockbackAng_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_X;
                }
                else if (jsoneq(jsonStr, &t[i], "knockbackAng_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_Y;
                }
                else if (jsoneq(jsonStr, &t[i], "hitstun") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_HITSTUN;
                }
                else if (jsoneq(jsonStr, &t[i], "sprite") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_SPRITE;
                }
                else if (jsoneq(jsonStr, &t[i], "isProjectile") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_ISPROJECTILE;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileSprite") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILESPRITE;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileDuration") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEDURATION;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileVelo_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_X;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileVelo_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_Y;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileAccel_x") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_X;
                }
                else if (jsoneq(jsonStr, &t[i], "projectileAccel_y") == 0)
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_Y;
                }
                break;
            }
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_DURATION:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_X:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_Y:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_X:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_Y:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_DAMAGE:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_X:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_Y:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_HITSTUN:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SPRITE:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_ISPROJECTILE:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILESPRITE:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEDURATION:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_X:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_Y:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_X:
            case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_Y:
            {
                switch(ps)
                {
                    default:
                    {
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_DURATION:
                    {
                        cAttack->attackFrames[cAttackIdx].duration = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_X:
                    {
                        cAttack->attackFrames[cAttackIdx].hitboxPos.x = SF * jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_RELATIVE_POS_Y:
                    {
                        cAttack->attackFrames[cAttackIdx].hitboxPos.y = SF * jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_X:
                    {
                        cAttack->attackFrames[cAttackIdx].hitboxSize.x = SF * jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SIZE_Y:
                    {
                        cAttack->attackFrames[cAttackIdx].hitboxSize.y = SF * jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_DAMAGE:
                    {
                        cAttack->attackFrames[cAttackIdx].damage = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK:
                    {
                        cAttack->attackFrames[cAttackIdx].knockback = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_X:
                    {
                        cAttack->attackFrames[cAttackIdx].knockbackAng.x = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_KNOCKBACK_ANG_Y:
                    {
                        cAttack->attackFrames[cAttackIdx].knockbackAng.y = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_HITSTUN:
                    {
                        cAttack->attackFrames[cAttackIdx].hitstun = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_SPRITE:
                    {
                        cAttack->attackFrames[cAttackIdx].sprite = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_ISPROJECTILE:
                    {
                        cAttack->attackFrames[cAttackIdx].isProjectile = jsonBoolean(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILESPRITE:
                    {
                        cAttack->attackFrames[cAttackIdx].projSprite = jsonString(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEDURATION:
                    {
                        cAttack->attackFrames[cAttackIdx].projDuration = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_X:
                    {
                        cAttack->attackFrames[cAttackIdx].projVelo.x = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEVELO_Y:
                    {
                        cAttack->attackFrames[cAttackIdx].projVelo.y = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_X:
                    {
                        cAttack->attackFrames[cAttackIdx].projAccel.x = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                    case PARSING_FIGHTER_ATTACK_FRAME_ATTR_PROJECTILEACCEL_Y:
                    {
                        cAttack->attackFrames[cAttackIdx].projAccel.y = jsonInteger(jsonStr, t[i]);
                        break;
                    }
                }

                // Handle state transitions
                if(((++activeStateAttrsParsed) == activeStateAttrsToParse) &&
                        (cAttackIdx == cAttack->numAttackFrames - 1))
                {
                    ps = PARSING_FIGHTER_ATTACK_ATTRS;
                }
                else
                {
                    ps = PARSING_FIGHTER_ATTACK_FRAME_ATTRS;
                }
                break;
            }
        }
    }

    freeJson(jsonStr);

    return fighters;
}

/**
 * @brief TODO
 *
 * @param fighter
 * @param numFighters
 */
void freeFighterData(fighter_t* fighter, uint8_t numFighters)
{
    for(uint8_t fgtIdx = 0; fgtIdx < numFighters; fgtIdx++)
    {
        for(uint8_t atkIdx = 0; atkIdx < NUM_ATTACKS; atkIdx++)
        {
            free(fighter[fgtIdx].attacks[atkIdx].startupLagSpr);
            free(fighter[fgtIdx].attacks[atkIdx].endLagSpr);
            for(uint8_t atkFrameIdx = 0; atkFrameIdx < fighter[fgtIdx].attacks[atkIdx].numAttackFrames; atkFrameIdx++)
            {
                free(fighter[fgtIdx].attacks[atkIdx].attackFrames[atkFrameIdx].sprite);
                if(NULL != fighter[fgtIdx].attacks[atkIdx].attackFrames[atkFrameIdx].projSprite)
                {
                    free(fighter[fgtIdx].attacks[atkIdx].attackFrames[atkFrameIdx].projSprite);
                }
            }
            free(fighter[fgtIdx].attacks[atkIdx].attackFrames);
        }
        free(fighter[fgtIdx].idleSprite0);
        free(fighter[fgtIdx].idleSprite1);
        free(fighter[fgtIdx].jumpSprite);
        free(fighter[fgtIdx].duckSprite);
    }
    free(fighter);
}
