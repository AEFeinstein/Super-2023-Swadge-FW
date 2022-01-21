#ifndef _ESP_LOG_H_
#define _ESP_LOG_H_

#include <stdio.h>

#define ESP_LOGE(tag,fmt,...) fprintf(stderr, tag ": " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag,fmt,...) fprintf(stderr, tag ": " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGI(tag,fmt,...) fprintf(stdout, tag ": " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGD(tag,fmt,...) fprintf(stdout, tag ": " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGV(tag,fmt,...) fprintf(stdout, tag ": " fmt "\n", ##__VA_ARGS__)

#endif