#ifndef _NVS_MANAGER_H_
#define _NVS_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "nvs.h"

//==============================================================================
// Defines
//==============================================================================

#define NVS_NAMESPACE_NAME "storage"

/// Helper macro to return the lowest of two integers
#define MIN(X, Y) ( ((X) < (Y)) ? (X) : (Y) )

// This comes from partitions.csv, and must be changed in both places simultaneously
#define NVS_PARTITION_SIZE              0x6000

// These are described in the ESP-IDF documentation
#define NVS_ENTRY_BYTES                     32
#define NVS_MAX_BLOB_BYTES MIN(508000, (uint32_t)(NVS_PARTITION_SIZE * 0.976) - 4000)

#ifndef NVS_PART_NAME_MAX_SIZE
#define NVS_PART_NAME_MAX_SIZE              16   /*!< maximum length of partition name (excluding null terminator) */
#endif
#ifndef NVS_KEY_NAME_MAX_SIZE
#define NVS_KEY_NAME_MAX_SIZE               16   /*!< Maximal length of NVS key name (including null terminator) */
#endif

//==============================================================================
// Enums, Structs
//==============================================================================

//==============================================================================
// Const Variables
//==============================================================================

//const char NVS_NAMESPACE_NAME[] = "storage";

//==============================================================================
// Function Prototypes
//==============================================================================

bool initNvs(bool firstTry);
bool eraseNvs(void);
bool readNvs32(const char* key, int32_t* outVal);
bool writeNvs32(const char* key, int32_t val);
bool readNvsBlob(const char* key, void* out_value, size_t* length);
bool writeNvsBlob(const char* key, const void* value, size_t length);
bool eraseNvsKey(const char* key);
bool readNvsStats(nvs_stats_t* outStats);
bool readAllNvsEntryInfos(nvs_stats_t* outStats, nvs_entry_info_t** outEntryInfos, uint32_t* numEntryInfos);

#endif
