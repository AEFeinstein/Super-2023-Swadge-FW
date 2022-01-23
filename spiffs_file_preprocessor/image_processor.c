#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

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
 * @param diagScalar
 */
void spreadError(pixel_t **img, int x, int y, int w, int h,
				 int teR, int teG, int teB, float diagScalar)
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
 * @param fname
 * @return true
 * @return false
 */
bool doesFileExist(const char *fname)
{
	int fd = open(fname, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		/* failure */
		if (errno == EEXIST)
		{
			/* the file already existed */
			close(fd);
			return true;
		}
	}

	/* File does not exist */
	close(fd);
	return false;
}

/**
 * @brief TODO
 *
 * @param infile
 * @param outdir
 */
void process_image(const char *infile, const char *outdir)
{
	/* Determine if the output file already exists */
	char outFilePath[128] = {0};
	strcat(outFilePath, outdir);
	strcat(outFilePath, "/");
	strcat(outFilePath, get_filename(infile));

	/* Change the file extension */
	char * dotptr = strrchr(outFilePath, '.');
	dotptr[1] = 'q';
	dotptr[2] = 'o';
	dotptr[3] = 'i';

	if(doesFileExist(outFilePath))
	{
		printf("Output for %s already exists\n", infile);
		return;
	}

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

			/* Find the bit-reduced value, use rounding, 5551 for RGBA */
			image16b[y][x].r = (127 + ((sourceR + image16b[y][x].eR) * 31)) / 255;
			image16b[y][x].g = (127 + ((sourceG + image16b[y][x].eG) * 31)) / 255;
			image16b[y][x].b = (127 + ((sourceB + image16b[y][x].eB) * 31)) / 255;
			image16b[y][x].a = sourceA ? 0xFF : 0x00;

			/* Find the total error, 8 bits per channel */
			int teR = sourceR - ((image16b[y][x].r * 255) / 31);
			int teG = sourceG - ((image16b[y][x].g * 255) / 31);
			int teB = sourceB - ((image16b[y][x].b * 255) / 31);

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
			spreadError(image16b, x - 1, y - 1, w, h, teR, teG, teB, diagScalar);
			spreadError(image16b, x - 1, y + 1, w, h, teR, teG, teB, diagScalar);
			spreadError(image16b, x + 1, y - 1, w, h, teR, teG, teB, diagScalar);
			spreadError(image16b, x + 1, y + 1, w, h, teR, teG, teB, diagScalar);
			spreadError(image16b, x - 1, y + 0, w, h, teR, teG, teB, adjScalar);
			spreadError(image16b, x + 1, y + 0, w, h, teR, teG, teB, adjScalar);
			spreadError(image16b, x + 0, y - 1, w, h, teR, teG, teB, adjScalar);
			spreadError(image16b, x + 0, y + 1, w, h, teR, teG, teB, adjScalar);

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
				pixBuf[pixBufIdx++] = (image16b[y][x].g * 255) / 31;
				pixBuf[pixBufIdx++] = (image16b[y][x].b * 255) / 31;
				pixBuf[pixBufIdx++] = image16b[y][x].a;
			}
		}

		/* Free dithering memory */
		for (int y = 0; y < h; y++)
		{
			free(image16b[y]);
		}
		free(image16b);

		/* Write a QOI image */
		const qoi_desc qd = {
			.width = (unsigned int)w,
			.height = (unsigned int)h,
			.channels = 4,
			.colorspace = QOI_SRGB
		};
		qoi_write(outFilePath, pixBuf, &qd);

		/* Print results */
		printf("%s:\n  Source file size: %ld\n  QOI   file size: %ld\n",
			   infile,
			   getFileSize(infile),
			   getFileSize(outFilePath));
	}
}
