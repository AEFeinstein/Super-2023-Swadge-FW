#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "zopflipng_lib.h"

#include "spiffs_file_preprocessor.h"
#include "image_processor.h"

#define HI_BYTE(x) ((x >> 8) & 0xFF)
#define LO_BYTE(x) ((x) & 0xFF)

typedef struct
{
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
void shuffleArray(uint16_t *ar, uint32_t len)
{
	srand(time(NULL));
	for (int i = len - 1; i > 0; i--)
	{
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
int isNeighborNotDrawn(pixel_t **img, int x, int y, int w, int h)
{
	if (0 <= x && x < w)
	{
		if (0 <= y && y < h)
		{
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
void spreadError(pixel_t **img, int x, int y, int w, int h,
				 int teR, int teG, int teB, int teA, float diagScalar)
{
	if (0 <= x && x < w)
	{
		if (0 <= y && y < h)
		{
			if (!img[y][x].isDrawn)
			{
				img[y][x].eR = (int)(teR * diagScalar + 0.5);
				img[y][x].eG = (int)(teG * diagScalar + 0.5);
				img[y][x].eB = (int)(teB * diagScalar + 0.5);
				img[y][x].eA = (int)(teA * diagScalar + 0.5);
			}
		}
	}
}

/**
 * TODO
 *
 * @param fname
 * @return long
 */
long getFileSize(const char *fname)
{
	FILE *fp = fopen(fname, "rb");
	fseek(fp, 0L, SEEK_END);
	long sz = ftell(fp);
	fclose(fp);
	return sz;
}

/**
 * @brief TODO
 *
 * @param infile
 * @param outdir
 */
void process_image(const char *infile, const char *outdir)
{
	/* Load the source PNG */
	int w,h,n;
	unsigned char *data = stbi_load(infile, &w, &h, &n, 4);

	if (NULL != data)
	{
		/* Create an array for output */
		pixel_t **image16b;
		image16b = (pixel_t **)calloc(h, sizeof(pixel_t *));
		for (int y = 0; y < h; y++)
		{
			image16b[y] = (pixel_t *)calloc(w, sizeof(pixel_t));
		}

		/* Create an array of pixel indicies, then shuffle it */
		uint16_t indices[w * h];
		for (int i = 0; i < w * h; i++)
		{
			indices[i] = i;
		}
		shuffleArray(indices, w * h);

		/* For all pixels */
		for (int i = 0; i < w * h; i++)
		{
			/* Get the x, y coordinates for the random pixel */
			int x = indices[i] % w;
			int y = indices[i] / w;

			/* Get the source pixel, 8 bits per channel */
			unsigned char sourceR = data[(y * (w * 4)) + (x * 4) + 0];
			unsigned char sourceG = data[(y * (w * 4)) + (x * 4) + 1];
			unsigned char sourceB = data[(y * (w * 4)) + (x * 4) + 2];
			unsigned char sourceA = data[(y * (w * 4)) + (x * 4) + 3];

			/* Find the bit-reduced value, use rounding, 5655 for RGBA */
			image16b[y][x].r = (127 + ((sourceR + image16b[y][x].eR) * 31)) / 255;
			image16b[y][x].g = (127 + ((sourceG + image16b[y][x].eG) * 63)) / 255;
			image16b[y][x].b = (127 + ((sourceB + image16b[y][x].eB) * 31)) / 255;
			image16b[y][x].a = (127 + ((sourceA + image16b[y][x].eA) * 31)) / 255;

			/* Find the total error, 8 bits per channel */
			int teR = sourceR - ((image16b[y][x].r * 255) / 31);
			int teG = sourceG - ((image16b[y][x].g * 255) / 63);
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

		/* Free stbi memory */
		stbi_image_free(data);

		/* Convert to a pixel buffer */
		unsigned char pixBuf[w*h*4];
		int pixBufIdx = 0;
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				pixBuf[pixBufIdx++] = (image16b[y][x].r * 255) / 31;
				pixBuf[pixBufIdx++] = (image16b[y][x].g * 255) / 63;
				pixBuf[pixBufIdx++] = (image16b[y][x].b * 255) / 31;
				pixBuf[pixBufIdx++] = (image16b[y][x].a * 255) / 31;
			}
		}

		/* Free dithering memory */
		for (int y = 0; y < h; y++)
		{
			free(image16b[y]);
		}
		free(image16b);

		/* Write PNG to RAM */
		stbi_write_png_compression_level = INT_MAX;
		int pngOutLen = 0;
		const unsigned char *pngBytes = stbi_write_png_to_mem(pixBuf, w * 4, w, h, 4, &pngOutLen);

		/* Set up zopfli defaults */
		// --iterations=500 --filters=01234mepb --lossy_8bit --lossy_transparent
		enum ZopfliPNGFilterStrategy filters[] =
		{
			kStrategyZero,
			kStrategyOne,
			kStrategyTwo,
			kStrategyThree,
			kStrategyFour,
			kStrategyMinSum,
			kStrategyEntropy,
			kStrategyPredefined,
			kStrategyBruteForce
		};
		CZopfliPNGOptions zopfliOpts =
		{
			.lossy_transparent = true,
			.lossy_8bit = true,
			.filter_strategies = filters,
			.num_filter_strategies = sizeof(filters) / sizeof(filters[0]),
			.auto_filter_strategy = false,
			.keepchunks = NULL,
			.num_keepchunks = 0,
			.use_zopfli = true,
			.num_iterations = 500,
			.num_iterations_large = 500,
			.block_split_strategy = 0,
		};

		/* Compress as best as we can */
		unsigned char *resultpng;
		size_t resultpng_size;
		CZopfliPNGOptimize(pngBytes,
						   pngOutLen,
						   &zopfliOpts,
						   1, // int verbose,
						   &resultpng,
						   &resultpng_size);

		/* Write zopfli compressed png to a file */
		char outFilePath[128] = {0};
		strcat(outFilePath, outdir);
		strcat(outFilePath, "/");
		strcat(outFilePath, get_filename(infile));
		FILE *zopfliOut = fopen(outFilePath, "wb+");
		fwrite(resultpng, resultpng_size, 1, zopfliOut);
		fclose(zopfliOut);

		/* free zopfli memory */
		free(resultpng);

		/* Print results */
		printf("%s:\n  Source file size: %ld\n  STBI   file size: %d\n  Zopfli file size: %ld\n",
			   infile,
			   getFileSize(infile),
			   pngOutLen,
			   getFileSize(outFilePath));
	}
}
