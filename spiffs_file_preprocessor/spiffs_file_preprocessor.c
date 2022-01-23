#include <ctype.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "spiffs_file_preprocessor.h"
#include "image_processor.h"
#include "font_processor.h"

const char * outDirName = NULL;

/**
 * @brief TODO
 *
 */
void print_usage(void)
{
    printf("Usage:\n  spiffs_file_preprocessor\n    -i INPUT_DIRECTORY\n    -o OUTPUT_DIRECTORY\n");
}

/**
 * @brief TODO
 * 
 * @param filename 
 * @param suffix 
 * @return true 
 * @return false 
 */
bool endsWith(const char *filename, const char *suffix)
{
    if(strlen(suffix) > strlen(filename))
    {
        return false;
    }
    return 0 == strcmp(&(filename[strlen(filename) - strlen(suffix)]), suffix);
}

/**
 * @brief Get the filename ext object
 *
 * @param filename
 * @return const char*
 */
const char *get_filename(const char *filename) {
    const char *slash = strrchr(filename, '/');
    if(!slash || slash == filename) {
        return "";
    }
    return slash + 1;
}

/**
 * @brief TODO
 *
 * @param fpath
 * @param st
 * @param tflag
 * @return int
 */
static int processFile(const char * fpath, const struct stat * st __attribute__((unused)), int tflag)
{
    switch(tflag) {
    case FTW_F: // file
        {
            if(endsWith(fpath, ".font.png"))
            {
                process_font(fpath, outDirName);
            }
            else if(endsWith(fpath, ".png"))
            {
                process_image(fpath, outDirName);
            }
            break;
        }
    case FTW_D: // directory
        {
            break;
        }
    default:
    // case FTW_SL: // symlink
    case FTW_NS: // failed
    case FTW_DNR: // failed
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief TODO
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char ** argv)
{
    int c;
    const char * inDirName = NULL;

    opterr = 0;
    while ((c = getopt (argc, argv, "i:o:")) != -1)
    {
        switch (c)
        {
        case 'i': {
                inDirName = optarg;
                break;
            }
        case 'o': {
                outDirName = optarg;
                break;
            }
        default: {
                fprintf(stderr, "Invalid argument %c\n", c);
                print_usage();
                return -1;
            }
        }
    }

    if(NULL == inDirName || NULL == outDirName)
    {
        fprintf(stderr, "Failed to provide all arguments\n");
        print_usage();
        return -1;
    }

    if(ftw(inDirName, processFile, 99) == -1) {
        fprintf(stderr, "Failed to walk file tree\n");
        return -1;
    }

    return 0;
}
