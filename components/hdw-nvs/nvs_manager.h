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

#ifndef NVS_NAMESPACE_NAME
#define NVS_NAMESPACE_NAME nvs_namespace_name
#endif
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

#ifndef nvs_namespace_name
const char nvs_namespace_name[] = "storage";
#endif

/**
 * @brief Read an unsigned 32 bit value from NVS with a given string key
 *
 * @param key The key for the value to read
 * @param outVal The value that was read
 * @return true if the value was read, false if it was not
 */
#define readNvsU8(key,outVal) readNamespacedNvsU8(NVS_NAMESPACE_NAME, key, outVal)

/**
 * @brief Write an unsigned 8 bit value to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param val The value to write
 * @return true if the value was written, false if it was not
 */
#define writeNvsU8(key,val) writeNamespacedNvsU8(NVS_NAMESPACE_NAME, key, val)

/**
 * @brief Read an unsigned 32 bit value from NVS with a given string key
 *
 * @param key The key for the value to read
 * @param outVal The value that was read
 * @return true if the value was read, false if it was not
 */
#define readNvsU32(key,outVal) readNamespacedNvsU32(NVS_NAMESPACE_NAME, key, outVal)

/**
 * @brief Write an unsigned 32 bit value to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param val The value to write
 * @return true if the value was written, false if it was not
 */
#define writeNvsU32(key,val) writeNamespacedNvsU32(NVS_NAMESPACE_NAME, key, val)

/**
 * @brief Read a 32 bit value from NVS with a given string key
 *
 * @param key The key for the value to read
 * @param outVal The value that was read
 * @return true if the value was read, false if it was not
 */
#define readNvs32(key,outVal) readNamespacedNvs32(NVS_NAMESPACE_NAME, key, outVal)

/**
 * @brief Write a 32 bit value to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param val The value to write
 * @return true if the value was written, false if it was not
 */
#define writeNvs32(key,val) writeNamespacedNvs32(NVS_NAMESPACE_NAME, key, val)

/**
 * @brief Read a blob from NVS with a given string key
 * 
 * @param key The key for the value to read
 * @param out_value The value will be written to this memory. It must be allocated before calling readNvsBlob()
 * @param length The length of the value that was read
 * @return true if the value was read, false if it was not
 */
#define readNvsBlob(key,out_value,length) readNamespacedNvsBlob(NVS_NAMESPACE_NAME, key, out_value, length)

/**
 * @brief Write a blob to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param value The blob value to write
 * @param length The length of the blob
 * @return true if the value was written, false if it was not
 */
#define writeNvsBlob(key,value,length) writeNamespacedNvsBlob(NVS_NAMESPACE_NAME, key, value, length)

/**
 * @brief Delete the value with the given key from NVS
 *
 * @param key The NVS key to be deleted
 * @return true if the value was deleted, false if it was not
 */
#define eraseNvsKey(key) eraseNamespacedNvsKey(NVS_NAMESPACE_NAME, key)

/**
 * @brief Read info about each used entry in NVS
 *
 * @param outStats If non-NULL, the NVS stats struct will be written to this memory. It must be allocated before calling readAllNvsEntryInfos()
 * @param outEntries A pointer to an array of NVS entry info structs will be written to this memory. If there is already an allocated array, it will be freed and reallocated.
 * @return true if the entry infos were read, false if they were not
 */
#define readAllNvsEntryInfos(outStats,outEntries) readAllNamespacedNvsEntryInfos(NULL, outStats, outEntries)

//==============================================================================
// Function Prototypes
//==============================================================================

bool initNvs(bool firstTry);
bool eraseNvs(void);
bool readNamespacedNvsU8(const char* namespace, const char* key, uint8_t* outVal);
bool writeNamespacedNvsU8(const char* namespace, const char* key, uint8_t val);
bool readNamespacedNvsU32(const char* namespace, const char* key, uint32_t* outVal);
bool writeNamespacedNvsU32(const char* namespace, const char* key, uint32_t val);
bool readNamespacedNvs32(const char* namespace, const char* key, int32_t* outVal);
bool writeNamespacedNvs32(const char* namespace, const char* key, int32_t val);
bool readNamespacedNvsBlob(const char* namespace, const char* key, void* out_value, size_t* length);
bool writeNamespacedNvsBlob(const char* namespace, const char* key, const void* value, size_t length);
bool eraseNamespacedNvsKey(const char* namespace, const char* key);
bool readNvsStats(nvs_stats_t* outStats);
bool readAllNamespacedNvsEntryInfos(const char* namespace, nvs_stats_t* outStats, nvs_entry_info_t** outEntries);

#endif
