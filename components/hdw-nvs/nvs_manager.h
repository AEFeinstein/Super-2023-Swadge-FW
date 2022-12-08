#ifndef _NVS_MANAGER_H_
#define _NVS_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifndef EMU
#include "nvs.h"
#endif

//==============================================================================
// Defines
//==============================================================================

#define NVS_NAMESPACE_NAME "storage"
#define NVS_ENTRY_BYTES                     32

#define NVS_PART_NAME_MAX_SIZE              16   /*!< maximum length of partition name (excluding null terminator) */
#define NVS_KEY_NAME_MAX_SIZE               16   /*!< Maximal length of NVS key name (including null terminator) */

//==============================================================================
// Enums, Structs
//==============================================================================

#ifdef EMU
/**
 * @brief Types of variables
 *
 */
typedef enum {
    NVS_TYPE_U8    = 0x01,  /*!< Type uint8_t */
    NVS_TYPE_I8    = 0x11,  /*!< Type int8_t */
    NVS_TYPE_U16   = 0x02,  /*!< Type uint16_t */
    NVS_TYPE_I16   = 0x12,  /*!< Type int16_t */
    NVS_TYPE_U32   = 0x04,  /*!< Type uint32_t */
    NVS_TYPE_I32   = 0x14,  /*!< Type int32_t */
    NVS_TYPE_U64   = 0x08,  /*!< Type uint64_t */
    NVS_TYPE_I64   = 0x18,  /*!< Type int64_t */
    NVS_TYPE_STR   = 0x21,  /*!< Type string */
    NVS_TYPE_BLOB  = 0x42,  /*!< Type blob */
    NVS_TYPE_ANY   = 0xff   /*!< Must be last */
} nvs_type_t;

/**
 * @note Info about storage space NVS.
 */
typedef struct {
    size_t used_entries;      /**< Amount of used entries. */
    size_t free_entries;      /**< Amount of free entries. */
    size_t total_entries;     /**< Amount all available entries. */
    size_t namespace_count;   /**< Amount name space. */
} nvs_stats_t;

/**
 * @brief information about entry obtained from nvs_entry_info function
 */
typedef struct {
    char namespace_name[16];    /*!< Namespace to which key-value belong */
    char key[NVS_KEY_NAME_MAX_SIZE];               /*!< Key of stored key-value pair */
    nvs_type_t type;            /*!< Type of stored key-value pair */
} nvs_entry_info_t;
#endif

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
bool readAllNvsEntryInfos(nvs_stats_t* outStats, nvs_entry_info_t** outEntries);

#endif
