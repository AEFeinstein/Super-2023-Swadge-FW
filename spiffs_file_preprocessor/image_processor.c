#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// #define TEST_OUTPUT
#ifdef TEST_OUTPUT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#include "spiffs_file_preprocessor.h"
#include "image_processor.h"

#define HI_BYTE(x) ((x >> 8) & 0xFF)
#define LO_BYTE(x) ((x) & 0xFF)

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    int eR;
    int eG;
    int eB;
    int eA;
    bool isDrawn;
} pixel_t;

/**
 * @brief TODO
 *
 * @param ar
 * @param len
 */
void shuffleArray(uint16_t * ar, uint32_t len) {
    srand(time(NULL));
    for (int i = len - 1; i > 0; i--) {
        int index = rand() % (i + 1);
        int a = ar[index];
        ar[index] = ar[i];
        ar[i] = a;
    }
}

/**
 * @brief TODO
 *
 * @param img
 * @param x
 * @param y
 * @param w
 * @param h
 * @return int
 */
int isNeighborNotDrawn(pixel_t ** img, int x, int y, int w, int h)
{
    if(0 <= x && x < w) {
        if(0 <= y && y < h) {
            return !(img[y][x].isDrawn) ? 1 : 0;
        }
    }
    return 0;
}

/**
 * @brief TODO
 *
 * @param image16b
 * @param x
 * @param y
 * @param w
 * @param h
 * @param teR
 * @param teG
 * @param teB
 * @param teA
 * @param diagScalar
 */
void spreadError(pixel_t ** img, int x, int y, int w, int h,
                 int teR, int teG, int teB, int teA, float diagScalar)
{
    if(0 <= x && x < w) {
        if(0 <= y && y < h) {
            if (!img[y][x].isDrawn) {
                img[y][x].eR = (int)(teR * diagScalar + 0.5);
                img[y][x].eG = (int)(teG * diagScalar + 0.5);
                img[y][x].eB = (int)(teB * diagScalar + 0.5);
                img[y][x].eA = (int)(teA * diagScalar + 0.5);
            }
        }
    }
}

/**
 * @brief TODO
 *
 * @param infile
 * @param outdir
 */
void process_image(const char * infile, const char * outdir)
{
    int w,h,n;
    unsigned char *data = stbi_load(infile, &w, &h, &n, 4);

    if(NULL != data)
    {
        /* Create an array for output */
        pixel_t ** image16b;
        image16b = (pixel_t ** )calloc(h, sizeof(pixel_t*));
        for(int y = 0; y < h; y++)
        {
            image16b[y] = (pixel_t *)calloc(w, sizeof(pixel_t));
        }

        /* Create an array of pixel indicies, then shuffle it */
        uint16_t indices[w * h];
        for(int i = 0; i < w * h; i++)
        {
            indices[i] = i;
        }
        shuffleArray(indices, w * h);

        /* For all pixels */
        for(int i = 0; i < w * h; i++)
        {
            /* Get the x, y coordinates for the random pixel */
            int x = indices[i] % w;
            int y = indices[i] / w;

            /* Get the source pixel, 8 bits per channel */
            unsigned char sourceR = data[(y * (w * 4)) + (x * 4) + 0];
            unsigned char sourceG = data[(y * (w * 4)) + (x * 4) + 1];
            unsigned char sourceB = data[(y * (w * 4)) + (x * 4) + 2];
            unsigned char sourceA = data[(y * (w * 4)) + (x * 4) + 3];

            /* Find the quantized value, use rounding, 5 bits per channel */
            image16b[y][x].r = (127 + ((sourceR + image16b[y][x].eR) * 31)) / 255;
            image16b[y][x].g = (127 + ((sourceG + image16b[y][x].eG) * 31)) / 255;
            image16b[y][x].b = (127 + ((sourceB + image16b[y][x].eB) * 31)) / 255;
            image16b[y][x].a = (127 + ((sourceA + image16b[y][x].eA) * 31)) / 255;

            /* Find the total error, 8 bits per channel */
            int teR = sourceR - ((image16b[y][x].r * 255) / 31);
            int teG = sourceG - ((image16b[y][x].g * 255) / 31);
            int teB = sourceB - ((image16b[y][x].b * 255) / 31);
            int teA = sourceA - ((image16b[y][x].a * 255) / 31);

            /* Count all the neighbors that haven't been drawn yet */
            int adjNeighbors = 0;
            adjNeighbors += isNeighborNotDrawn(image16b, x + 0, y + 1, w, h);
            adjNeighbors += isNeighborNotDrawn(image16b, x + 0, y - 1, w, h);
            adjNeighbors += isNeighborNotDrawn(image16b, x + 1, y + 0, w, h);
            adjNeighbors += isNeighborNotDrawn(image16b, x - 1, y + 0, w, h);
            int diagNeighbors = 0;
            diagNeighbors += isNeighborNotDrawn(image16b, x - 1, y - 1, w, h);
            diagNeighbors += isNeighborNotDrawn(image16b, x + 1, y - 1, w, h);
            diagNeighbors += isNeighborNotDrawn(image16b, x - 1, y + 1, w, h);
            diagNeighbors += isNeighborNotDrawn(image16b, x + 1, y + 1, w, h);

            /* Spread the error to all neighboring unquantized pixels, with
             * twice as much error to the adjacent pixels as the diagonal ones
             */
            float diagScalar = 1 / (float)((2 * adjNeighbors) + diagNeighbors);
            float adjScalar = 2 * diagScalar;

            /* Write the error */
            spreadError(image16b, x - 1, y - 1, w, h, teR, teG, teB, teA, diagScalar);
            spreadError(image16b, x - 1, y + 1, w, h, teR, teG, teB, teA, diagScalar);
            spreadError(image16b, x + 1, y - 1, w, h, teR, teG, teB, teA, diagScalar);
            spreadError(image16b, x + 1, y + 1, w, h, teR, teG, teB, teA, diagScalar);
            spreadError(image16b, x - 1, y + 0, w, h, teR, teG, teB, teA, adjScalar);
            spreadError(image16b, x + 1, y + 0, w, h, teR, teG, teB, teA, adjScalar);
            spreadError(image16b, x + 0, y - 1, w, h, teR, teG, teB, teA, adjScalar);
            spreadError(image16b, x + 0, y + 1, w, h, teR, teG, teB, teA, adjScalar);

            /* Mark the random pixel as drawn */
            image16b[y][x].isDrawn = true;
        }

        /* Open the output file */
        char outFilePath[128] = {0};
        strcat(outFilePath, outdir);
        strcat(outFilePath, "/");
        strcat(outFilePath, get_filename(infile));
        FILE * fp = fopen(outFilePath, "wb+");

#ifdef TEST_OUTPUT
        unsigned char testData[w * h * 4];
#endif

        /* Write the output dimensions */
        putc(HI_BYTE(w), fp);
        putc(LO_BYTE(w), fp);
        putc(HI_BYTE(h), fp);
        putc(LO_BYTE(h), fp);

        /* Write the output pixels */
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                uint16_t rgb555 = ((image16b[y][x].r & 0x1F) << 10) |
                                  ((image16b[y][x].g & 0x1F) <<  5) |
                                  ((image16b[y][x].b & 0x1F) <<  0);
                if(image16b[y][x].a < 0x0F) {
                    rgb555 |= 0x8000;
                }
                putc(HI_BYTE(rgb555), fp);
                putc(LO_BYTE(rgb555), fp);

#ifdef TEST_OUTPUT
                testData[(y * w * 4) + (x * 4) + 0] = (image16b[y][x].r * 255) / 31;
                testData[(y * w * 4) + (x * 4) + 1] = (image16b[y][x].g * 255) / 31;
                testData[(y * w * 4) + (x * 4) + 2] = (image16b[y][x].b * 255) / 31;
                testData[(y * w * 4) + (x * 4) + 3] = (image16b[y][x].a * 255) / 31;
#endif
            }
        }

        /* Close the file */
        fclose(fp);

#ifdef TEST_OUTPUT
        stbi_write_png_compression_level = 16;
        stbi_write_png("test.png", w, h, 4, testData, w * 4);
#endif

        /* Free memory */
        for(int y = 0; y < h; y++)
        {
            free(image16b[y]);
        }
        free(image16b);
        stbi_image_free(data);
    }
}