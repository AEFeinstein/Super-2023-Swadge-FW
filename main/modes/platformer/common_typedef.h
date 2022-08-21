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
    ST_GAME_OVER,
    ST_HIGH_SCORE_ENTRY,
    ST_HIGH_SCORE_TABLE,
    ST_ENDING
} gameStateEnum_t;

#endif