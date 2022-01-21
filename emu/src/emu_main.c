#include <stdbool.h>
#include "esp_emu.h"

extern void app_main(void);

/**
 * @brief TODO
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char ** argv)
{
    app_main();

    while(true)
    {
        runAllTasks();
    }
    return 0;
}