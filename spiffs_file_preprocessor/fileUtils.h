#ifndef _FILE_UTILS_H_
#define _FILE_UTILS_H_

#include <stdbool.h>

#define HI_BYTE(x) ((x >> 8) & 0xFF)
#define LO_BYTE(x) ((x) & 0xFF)

long getFileSize(const char *fname);
bool doesFileExist(const char *fname);
const char *get_filename(const char *filename);

#endif