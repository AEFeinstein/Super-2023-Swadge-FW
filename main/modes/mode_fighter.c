//==============================================================================
// Notes
//==============================================================================

/* Addition, subtraction, and multiplication of 8 and 16 bit ints is 5 instructions
 * Addition, subtraction, and multiplication of 32 bit ints is 4 instructions
 *
 * Division of 8 and 16 bit ints is 6 instructions
 * Division of 32 bit ints is 4 instructions
 */

//==============================================================================
// Includes
//==============================================================================

#include "esp_log.h"

#include "swadgeMode.h"
#include "mode_fighter.h"
#include "aabb_utils.h"
#include "bresenham.h"

//==============================================================================
// Constants
//==============================================================================

// Division by a power of 2 has slightly more instructions than rshift, but handles negative numbers properly!
#define SF (1 << 8) // Scaling factor, a nice power of 2
#define FIGHTER_SIZE (16 * SF)

#define FRAME_TIME_MS 50 // 20fps

#define ABS(x) (((x) < 0) ? -(x) : (x))

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
	box_t hurtbox;
	vector_t velocity;
	vector_t acceleration;
	bool isOnGround;
} fighter_t;

typedef struct
{
	box_t area;
	bool canFallThrough;
} platform_t;

//==============================================================================
// Functoon Prototypes
//==============================================================================

void fighterEnterMode(display_t *disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);

void updatePosition(fighter_t *f, const platform_t *platforms, uint8_t numPlatforms);

// void fighterAccelerometerCb(accel_t* accel);
// void fighterAudioCb(uint16_t * samples, uint32_t sampleCnt);
// void fighterTemperatureCb(float tmp_c);
// void fighterButtonCb(buttonEvt_t* evt);
// void fighterTouchCb(touch_event_t* evt);
// void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
// void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

// void fighterConCbFn(p2pInfo* p2p, connectionEvt_t evt);
// void fighterMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
// void fighterMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

//==============================================================================
// Variables
//==============================================================================

swadgeMode modeFighter =
{
	.modeName = "Fighter",
	.fnEnterMode = fighterEnterMode,
	.fnExitMode = fighterExitMode,
	.fnMainLoop = fighterMainLoop,
	.fnButtonCallback = NULL, // fighterButtonCb,
	.fnTouchCallback = NULL, // fighterTouchCb,
	.wifiMode = NO_WIFI, // ESP_NOW,
	.fnEspNowRecvCb = NULL, // fighterEspNowRecvCb,
	.fnEspNowSendCb = NULL, // fighterEspNowSendCb,
	.fnAccelerometerCallback = NULL, // fighterAccelerometerCb,
	.fnAudioCallback = NULL, // fighterAudioCb,
	.fnTemperatureCallback = NULL, // fighterTemperatureCb
};

fighter_t fighters[2];
display_t *d;

static const platform_t finalDest[] =
{
	{
		.area =
		{
			.x0 = (32) * SF,
			.y0 = (115) * SF,
			.x1 = (240-32) * SF,
			.y1 = (120) * SF,
		},
		.canFallThrough = false
	},
    {
		.area =
		{
			.x0 = (200) * SF,
			.y0 = (20) * SF,
			.x1 = (200) * SF,
			.y1 = (130) * SF,
		},
		.canFallThrough = false
	}
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param disp
 */
void fighterEnterMode(display_t *disp)
{
	d = disp;

	fighters[0].hurtbox.x0 = 0 * SF;
	fighters[0].hurtbox.y0 = 0 * SF;
	fighters[0].hurtbox.x1 = fighters[0].hurtbox.x0 + FIGHTER_SIZE;
	fighters[0].hurtbox.y1 = fighters[0].hurtbox.y0 + FIGHTER_SIZE;
	fighters[0].velocity.x = 4 * SF;
	fighters[0].velocity.y = 0 * SF;
	fighters[0].acceleration.x = 0 * SF;
	fighters[0].acceleration.y = 1 * SF;
	fighters[0].isOnGround = false;

	fighters[1].hurtbox.x0 = (((disp->w - 1 - 100) * SF) - FIGHTER_SIZE);
	fighters[1].hurtbox.y0 = 8 * SF;
	fighters[1].hurtbox.x1 = fighters[1].hurtbox.x0 + FIGHTER_SIZE;
	fighters[1].hurtbox.y1 = fighters[1].hurtbox.y0 + FIGHTER_SIZE;
	fighters[1].velocity.x = -4 * SF;
	fighters[1].velocity.y = 0 * SF;
	fighters[1].acceleration.x = 0 * SF;
	fighters[1].acceleration.y = 1 * SF;
	fighters[1].isOnGround = false;
}

/**
 * @brief TODO
 *
 */
void fighterExitMode(void)
{

}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void fighterMainLoop(int64_t elapsedUs)
{
	static int64_t frameElapsed = 0;
	frameElapsed += elapsedUs;
	if (frameElapsed > (FRAME_TIME_MS * 1000))
	{
		frameElapsed -= (FRAME_TIME_MS * 1000);

		updatePosition(&fighters[0], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));
        // TODO something gets screwy when this is offscreen
        // TODO cap velocity?
		updatePosition(&fighters[1], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // ESP_LOGI("FGT", "{[%d, %d], [%d, %d]}",
        //     fighters[0].hurtbox.x0,
        //     fighters[0].hurtbox.y0,
        //     fighters[0].velocity.x,
        //     fighters[0].velocity.y);
	}

	d->clearPx();

	rgba_pixel_t px =
	{
		.r = 0x1F,
		.g = 0x00,
		.b = 0x00,
		.a = PX_OPAQUE
	};
	drawBox(d, fighters[0].hurtbox, px, SF);

	px.r = 0;
	px.b = 0x1F;
	drawBox(d, fighters[1].hurtbox, px, SF);

	if (boxesCollide(fighters[0].hurtbox, fighters[1].hurtbox))
	{
		px.g = 0x1F;
		px.b = 0;
		fillDisplayArea(d, 0, 0, 10, 10, px);
	}

	px.r = 0x1F;
	px.g = 0x1F;
	px.b = 0x1F;
	for (uint8_t idx = 0; idx < sizeof(finalDest) / sizeof(finalDest[0]); idx++)
	{
		drawBox(d, finalDest[idx].area, px, SF);
	}
}

/**
 * @brief TODO
 * 
 * @param f 
 * @param platforms 
 * @param numPlatforms 
 */
void updatePosition(fighter_t *f, const platform_t *platforms, uint8_t numPlatforms)
{
	// Update velocity
	f->velocity.x += ((f->acceleration.x * FRAME_TIME_MS) / SF);
	if (!f->isOnGround)
	{
		f->velocity.y += ((f->acceleration.y * FRAME_TIME_MS) / SF);
	}

	// Find the target position
	int32_t x1 = f->hurtbox.x0 + (((f->velocity.x * FRAME_TIME_MS) / SF) + ((f->acceleration.x * FRAME_TIME_MS * FRAME_TIME_MS) / (SF * 4)));
	int32_t y1;
	if (!f->isOnGround)
	{
		y1 = f->hurtbox.y0 + (((f->velocity.y * FRAME_TIME_MS) / SF) + ((f->acceleration.y * FRAME_TIME_MS * FRAME_TIME_MS) / (SF * 4)));
	}
	else
	{
		y1 = f->hurtbox.y0;
	}

	// Move one pixel at a time towards the target, using Bresenham
	int32_t dx = ABS(x1 - f->hurtbox.x0);
	int32_t sx = f->hurtbox.x0 < x1 ? 1 : -1;
	int32_t dy = -ABS(y1 - f->hurtbox.y0);
	int32_t sy = f->hurtbox.y0 < y1 ? 1 : -1;
	int32_t err = dx + dy;
	int32_t e2; /* error value e_xy */

	while (true)
	{
		// Check for collisions between platforms and hurtbox
		// This comes before moving in case the initial state was a collision
		bool collisionDetected = false;
		bool isOnGround = false;
		for (uint8_t idx = 0; idx < numPlatforms; idx++)
		{
			// Check with a boundary
			if (boxesCollideBoundary(f->hurtbox, platforms[idx].area, SF))
			{
                // A fighter only has to be on one platform to be on the ground. Velocity doesn't matter
				if (((f->hurtbox.y1 + SF) == (platforms[idx].area.y0 + 1)))
				{
					isOnGround = true;
				}

                // If a fighter is moving vertically
				if (f->velocity.y)
				{
                    // If the fighter is moving downward and hit a platform
					if ((f->velocity.y > 0) && ((f->hurtbox.y1 + SF) == (platforms[idx].area.y0 + 1)))
					{
						// Fighter above platform
						f->velocity.y = 0;
						collisionDetected = true;
					}
                    // If the fighter is moving upward and hit a platform
					else if ((f->velocity.y < 0) && ((f->hurtbox.y0 + 1) == (platforms[idx].area.y1 + SF)))
					{
						// Fighter below platform
						f->velocity.y = 0;
						collisionDetected = true;
					}
				}

                // If a fighter is moving horizontally
				if (f->velocity.x)
				{
                    // If the fighter is moving rightward and hit a wall
					if ((f->velocity.x > 0) && ((f->hurtbox.x1 + SF) == (platforms[idx].area.x0 + 1)))
					{
						// Fighter to left of platform
						f->velocity.x = 0;
						collisionDetected = true;
					}
                    // If the fighter is moving leftward and hit a wall
					else if ((f->velocity.x < 0) && ((f->hurtbox.x0 + 1) == (platforms[idx].area.x1 + SF)))
					{
						// Fighter to right of platform
						f->velocity.x = 0;
						collisionDetected = true;
					}
				}

				if (collisionDetected)
				{
					// Break out of loop checking platforms
					break;
				}
			}
		}

        // Save if the fighter is on the ground
		f->isOnGround = isOnGround;

		if (collisionDetected)
		{
			// Break out of Bresenham loop
			break;
		}

		// Move either the x or y start of the hurtbox
        // This is Bresenham in action!
		e2 = 2 * err;
		if (e2 >= dy)
		{
			if (f->hurtbox.x0 == x1)
			{
				break;
			}
			err += dy;
			f->hurtbox.x0 += sx;
		}
		if (e2 <= dx)
		{
			if (f->hurtbox.y0 == y1)
			{
				break;
			}
			err += dx;
			f->hurtbox.y0 += sy;
		}

		// Adjust the end of the hurtbox
		f->hurtbox.x1 = f->hurtbox.x0 + FIGHTER_SIZE;
		f->hurtbox.y1 = f->hurtbox.y0 + FIGHTER_SIZE;
	}
}
