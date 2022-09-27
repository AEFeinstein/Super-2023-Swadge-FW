#ifndef PLATFORMER_COMMON_TYPEDEF_INCLUDED
#define PLATFORMER_COMMON_TYPEDEF_INCLUDED

typedef struct platformer_t platformer_t;
typedef struct entityManager_t entityManager_t;
typedef struct tilemap_t tilemap_t;
typedef struct entity_t entity_t;

typedef enum {
    ST_NULL,
    ST_TITLE_SCREEN,
    ST_READY_SCREEN,
    ST_GAME,
    ST_DEAD,
    ST_LEVEL_CLEAR,
    ST_WORLD_CLEAR,
    ST_GAME_CLEAR,
    ST_GAME_OVER,
    ST_HIGH_SCORE_ENTRY,
    ST_HIGH_SCORE_TABLE
} gameStateEnum_t;

typedef enum {
    SP_PLAYER_IDLE,
    SP_PLAYER_WALK1,
    SP_PLAYER_WALK2,
    SP_PLAYER_WALK3,
    SP_PLAYER_JUMP,
    SP_PLAYER_SLIDE,
    SP_PLAYER_HURT,
    SP_PLAYER_CLIMB,
    SP_PLAYER_WIN,
    SP_ENEMY_BASIC,
    SP_HITBLOCK_CONTAINER,
    SP_HITBLOCK_BRICKS,
    SP_DUSTBUNNY_IDLE,
    SP_DUSTBUNNY_CHARGE,
    SP_DUSTBUNNY_JUMP,
    SP_GAMING_1,
    SP_GAMING_2,
    SP_GAMING_3,
    SP_MUSIC_1,
    SP_MUSIC_2,
    SP_MUSIC_3,
    SP_WARP_1,
    SP_WARP_2,
    SP_WARP_3,
    SP_WASP_1,
    SP_WASP_2,
    SP_WASP_DIVE
} spriteDef_t;

#endif