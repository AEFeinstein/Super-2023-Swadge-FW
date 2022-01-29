#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "list.h"

#include "emu_main.h"
#include "esp_emu.h"
#include "esp_log.h"
#include "swadge_esp32.h"
#include "sound.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#define MAX(x,y) ((x)>(y)?(x):(y))

// Display memory
uint32_t * bitmapDisplay = NULL; //0xRRGGBBAA
int bitmapWidth = 0;
int bitmapHeight = 0;
volatile bool shouldDrawTft = false;
pthread_mutex_t displayMutex = PTHREAD_MUTEX_INITIALIZER;

// Input queues for buttons
char inputKeys[32];
uint32_t buttonState = 0;
list_t * buttonQueue;
pthread_mutex_t buttonQueueMutex = PTHREAD_MUTEX_INITIALIZER;

// LED state
uint8_t rdNumLeds = 0;
led_t rdLeds[256] = {0};

/**
 * Set up keyboard keys to act as pushbuttons
 * 
 * @param numButtons The number of keyboard keys to set up, up to 32
 * @param keyOrder The keys which should be used, as lowercase letters
 */
void setInputKeys(uint8_t numButtons, char * keyOrder)
{
	memcpy(inputKeys, keyOrder, numButtons);
	buttonState = 0;
	buttonQueue = list_new();
}

/**
 * This function must be provided for rawdraw. Key events are received here
 * 
 * Note that this is called on the main thread and the swadge task is on a
 * separate thread, so queue accesses must be mutexed
 *
 * @param keycode The key code, a lowercase ascii char
 * @param bDown true if the key was pressed, false if it was released
 */
void HandleKey( int keycode, int bDown )
{
	// Check keycode against initialized keys
	for(uint8_t idx = 0; idx < ARRAY_SIZE(inputKeys); idx++)
	{
		// If this matches
		if(keycode == inputKeys[idx])
		{
			// Set or clear the button
			if(bDown)
			{
				// Check if button was already pressed
				if(buttonState & (1 << idx))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, set it!
					buttonState |= (1 << idx);
				}
			}
			else
			{
				// Check if button was already released
				if(0 == (buttonState & (1 << idx)))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, clear it!
					buttonState &= ~(1 << idx);
				}
			}

			// Create a new event
			buttonEvt_t * evt = malloc(sizeof(buttonEvt_t));
			evt->button = idx;
			evt->down = bDown;
			evt->state = buttonState;

			// Add the event to the list, guarded by a mutex
			pthread_mutex_lock(&buttonQueueMutex);
			list_node_t * buttonNode = list_node_new(evt);
			list_rpush(buttonQueue, buttonNode); 
			pthread_mutex_unlock(&buttonQueueMutex);
			
			break;
		}
	}
}

/**
 * Check if there were any queued input events and return them if there were
 * 
 * @param evt Return the event through this pointer argument
 * @return true if an event occurred and was returned, false otherwise
 */
bool checkInputKeys(buttonEvt_t * evt)
{
	// Check the queue, guarded by a mutex
	pthread_mutex_lock(&buttonQueueMutex);
	list_node_t * node = list_lpop(buttonQueue);
	pthread_mutex_unlock(&buttonQueueMutex);

	// No events
	if(NULL == node)
	{
		memset(evt, 0, sizeof(buttonEvt_t));
		return false;
	}
	else
	{
		// Copy the event to the arg
		memcpy(evt, node->val, sizeof(buttonEvt_t));
		// Free everything
		free(node->val);
		free(node);
		// Return that an event occurred
		return true;
	}
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param button
 * @param bDown
 */
void HandleButton( int x, int y, int button, int bDown )
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param mask
 */
void HandleMotion( int x, int y, int mask )
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
	// Upon exit, stop all tasks
	joinThreads();

	// Then free display memory
	pthread_mutex_lock(&displayMutex);
	if(NULL != bitmapDisplay)
	{
		free(bitmapDisplay);
	}
	pthread_mutex_unlock(&displayMutex);

	// Close sound
	// TODO proper
	CloseSound(NULL);

	// Free button queue
	free(buttonQueue);
}

/**
 * @brief TODO
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc UNUSED, char ** argv UNUSED)
{
	// First initialize rawdraw
	// Screen-specific configurations
#if defined(CONFIG_ST7735_160x80)
    #define TFT_WIDTH         160
    #define TFT_HEIGHT         80
#elif defined(CONFIG_ST7789_240x135)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        135
#elif defined(CONFIG_ST7789_240x240)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        240
#else
    #error "Please pick a screen size"
#endif
	CNFGSetup( "Swadge S2 Emulator", TFT_WIDTH, TFT_HEIGHT + MAX(TFT_WIDTH, TFT_HEIGHT) + 1);

	// ARGB pixels
	pthread_mutex_lock(&displayMutex);
	bitmapDisplay = malloc(sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	memset(bitmapDisplay, 0, sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	pthread_mutex_unlock(&displayMutex);
	bitmapWidth = TFT_WIDTH;
	bitmapHeight = TFT_HEIGHT;

	// This is the 'main' that gets called when the ESP boots
    app_main();

	// Spin around waiting for a program exit afterwards
    while(1)
    {
		// Always handle inputs
		CNFGHandleInput();
		
		// Only redraw the screen when requested by the thread
		if(shouldDrawTft)
		{
			// Lower flag
			shouldDrawTft = false;

			// Grey Background
			CNFGBGColor = 0x252525FF;
			CNFGClearFrame();

			// Draw bitmap to screen
			pthread_mutex_lock(&displayMutex);
			CNFGUpdateScreenWithBitmap(bitmapDisplay, bitmapWidth, bitmapHeight);
			pthread_mutex_unlock(&displayMutex);

			// Draw dividing line
			CNFGColor( 0x808080FF ); 
			CNFGTackSegment(0, TFT_HEIGHT, TFT_WIDTH, TFT_HEIGHT);

			// Draw simulated LEDs
			if (rdNumLeds > 0)
			{
				for(int i = 0; i < rdNumLeds; i++)
				{
					float angle1 = ( i      * 2 * M_PI) / rdNumLeds;
					float angle2 = ((i + 1) * 2 * M_PI) / rdNumLeds;
					RDPoint points[] = 
					{
						{
							.x = (TFT_WIDTH / 2) + (TFT_WIDTH/2) * sin(angle1),
							.y = 1 + (TFT_HEIGHT + (TFT_WIDTH/2)) + (TFT_WIDTH/2) * cos(angle1),
						},
						{
							.x = (TFT_WIDTH / 2) + (TFT_WIDTH/2) * sin(angle2),
							.y = 1 + (TFT_HEIGHT + (TFT_WIDTH/2)) + (TFT_WIDTH/2) * cos(angle2),
						},
						{
							.x = TFT_WIDTH / 2,
							.y = 1 + TFT_HEIGHT + (TFT_WIDTH/2)
						}
					};

					// Draw filled polygon
					pthread_mutex_lock(&displayMutex);
					CNFGColor( (rdLeds[i].r << 24) | (rdLeds[i].g << 16) | (rdLeds[i].b << 8) | 0xFF);
					pthread_mutex_unlock(&displayMutex);
					CNFGTackPoly(points, ARRAY_SIZE(points)); 

					// Draw outline
					CNFGColor( 0x808080FF );
					CNFGTackSegment(points[0].x, points[0].y, points[1].x, points[1].y);
				}
			}

			//Display the image and wait for time to display next frame.
			CNFGSwapBuffers();
		}

		// Sleep for a ms
        usleep(1000);
    }

	return 0;
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @param px 
 */
void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px)
{
	// Convert from 15 bit to 24 bit color
	if(PX_OPAQUE == px.a)
	{
		uint8_t r8 = ((px.r * 0xFF) / 0x1F) & 0xFF;
		uint8_t g8 = ((px.g * 0xFF) / 0x1F) & 0xFF;
		uint8_t b8 = ((px.b * 0xFF) / 0x1F) & 0xFF;
		pthread_mutex_lock(&displayMutex);
		bitmapDisplay[(bitmapWidth * y) + x] = (0xFF << 24) | (r8 << 16) | (g8 << 8) | (b8);
		pthread_mutex_unlock(&displayMutex);
	}
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @return rgba_pixel_t 
 */
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y)
{
	pthread_mutex_lock(&displayMutex);
	uint32_t argb = bitmapDisplay[(bitmapWidth * y) + x];
	pthread_mutex_unlock(&displayMutex);
	rgba_pixel_t px;
	px.r = (((argb & 0xFF0000) >> 16) * 0x1F) / 0xFF; // 5 bit
	px.g = (((argb & 0x00FF00) >>  8) * 0x1F) / 0xFF; // 5 bit
	px.b = (((argb & 0x0000FF) >>  0) * 0x1F) / 0xFF; // 5 bit
	return px;
}

/**
 * @brief TODO
 * 
 */
void emuClearPxTft(void)
{
	pthread_mutex_lock(&displayMutex);
	memset(bitmapDisplay, 0x00, sizeof(uint32_t) * bitmapWidth * bitmapHeight);
	pthread_mutex_unlock(&displayMutex);
}

/**
 * @brief Called when the Swadge wants to draw a new display. Note, this is
 * called from a pthread, so it raises a flag to draw on the main thread
 * 
 * @param drawDiff unused, the whole display is always drawn
 */
void emuDrawDisplayTft(bool drawDiff UNUSED)
{
	/* Rawdraw is initialized on the main thread, so the draw calls must come 
	 * from there too. Raise a flag to do so
	 */
	shouldDrawTft = true;
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @param px 
 */
void emuSetPxOled(int16_t x, int16_t y, rgba_pixel_t px)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @return rgba_pixel_t 
 */
rgba_pixel_t emuGetPxOled(int16_t x, int16_t y)
{
	WARN_UNIMPLEMENTED();
	rgba_pixel_t px = {.r=0x00, .g = 0x00, .b = 0x00, .a = PX_OPAQUE};
	return px;
}

/**
 * @brief TODO
 * 
 */
void emuClearPxOled(void)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param drawDiff 
 */
void emuDrawDisplayOled(bool drawDiff)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief When the main task yields, rest
 * 
 */
void onTaskYield(void)
{
	// Just sleep for a ms
	usleep(1000);
}

/**
 * @brief TODO
 * 
 * @param numLeds 
 */
void initRawdrawLeds(uint8_t numLeds)
{
	rdNumLeds = numLeds;
}

/**
 * @brief TODO
 * 
 * @param leds 
 * @param numLeds 
 */
void setRawdrawLeds(led_t * leds, uint8_t numLeds)
{
	pthread_mutex_lock(&displayMutex);
	memcpy(rdLeds, leds, sizeof(led_t) * numLeds);
	pthread_mutex_unlock(&displayMutex);
}
