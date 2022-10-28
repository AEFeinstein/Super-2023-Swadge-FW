#ifndef _PLATFORMER_SOUNDS_H_
#define _PLATFORMER_SOUNDS_H_

//==============================================================================
// Includes
//==============================================================================
#include <stdint.h>
#include "musical_buzzer.h"

//==============================================================================
// Constants
//==============================================================================
static const song_t sndHit =
    {
        .notes =
            {
                {C_4, 25}, {C_5, 25}},
        .numNotes = 2,
        .shouldLoop = false
    };

static const song_t sndSquish =
    {
        .notes =
            {
                {740, 10}, {840, 10}, {940, 10}},
        .numNotes = 3,
        .shouldLoop = false
    };

static const song_t sndBreak =
    {
        .notes =
            {
                {C_5, 25}, {C_4, 25}, {A_4, 25}, {A_SHARP_3, 25}, {A_3, 25}},
        .numNotes = 5,
        .shouldLoop = false
    };

static const song_t sndCoin =
    {
        .notes =
            {
                {1000, 50}, {1200, 100}},
        .numNotes = 2,
        .shouldLoop = false
    };

static const song_t sndPowerUp =
    {
        .notes =
            { 
                {C_4, 100}, {G_4, 100}, {E_4, 100}, {C_5, 100}, {G_4, 100}, {D_5, 100}, {C_5, 100}},
        .numNotes = 7,
        .shouldLoop = false
    };

static const song_t sndJump1 =
    {
        .notes =
            { 
                {C_5, 50}, {E_5, 50}, {C_5, 50}},
        .numNotes = 2,
        .shouldLoop = false
    };         

static const song_t sndJump2 =
    {
        .notes =
            { 
                {E_5, 50}, {G_5, 50}, {E_5, 50}},
        .numNotes = 2,
        .shouldLoop = false
    };         

static const song_t sndJump3 =
    {
        .notes =
            { 
                {G_5, 50}, {C_6, 50}, {G_5, 50}},
        .numNotes = 2,
        .shouldLoop = false
    };

static const song_t sndWarp =
    {
        .notes =
            { 
                {D_5, 50}, {A_4, 50}, {E_4, 50},{D_5, 50}, {A_4, 50}, {E_4, 50},{D_5, 50}, {A_4, 50}, {E_4, 50}
            },
        .numNotes = 9,
        .shouldLoop = false
    };

static const song_t sndHurt =
    {
        .notes =
            { 
                {E_4, 50}, {D_SHARP_4, 50}, {D_4, 50},{C_SHARP_4, 50}, {C_4, 50}, {C_5, 50}, {C_4, 50}
            },
        .numNotes = 6,
        .shouldLoop = false
    };   

static const song_t sndWaveBall =
{
    .notes =
    {
        {D_4, 50},{D_5, 50},{A_6, 50},{A_5, 50}
    },
    .numNotes = 4,
    .shouldLoop = false
};    

static const song_t snd1up =
{
    .notes = 
    {
        {G_7, 40},{D_6, 40},{B_5, 80}
    },
    .numNotes = 3,
    .shouldLoop = false
};

#endif