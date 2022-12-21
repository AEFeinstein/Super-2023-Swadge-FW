#include "crashwrap.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "esp_private/panic_internal.h"
#include "xtensa/xtensa_context.h"


void __real_esp_panic_handler(void*);
const char * panicreason = 0;

void IRAM_ATTR __wrap_esp_panic_handler (void* info)
{
    esp_rom_printf("Panic has been triggered\n");

    nvs_handle_t handle;
	if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK)
	{
		// Write and hope for the best.
		nvs_set_blob(handle, "crashwrap", info, 36);
		if( ((panic_info_t*)info)->description )
		{
			int len = strlen(((panic_info_t*)info)->description);
			nvs_set_blob(handle, "crashdesc", ((panic_info_t*)info)->description, len);
		}
		if( ((panic_info_t*)info)->reason )
		{
			int len = strlen(((panic_info_t*)info)->reason);
			nvs_set_blob(handle, "crashreason", ((panic_info_t*)info)->reason, len);
		}

		const XtExcFrame * fr = ((panic_info_t*)info)->frame;
		if( fr )
			nvs_set_blob(handle, "crashframe", fr, sizeof(XtExcFrame));

		nvs_set_blob(handle, "crashpanic", panicreason?panicreason:"", panicreason?strlen(panicreason):0 );

		nvs_close(handle);
	}

    __real_esp_panic_handler(info);
}




void IRAM_ATTR __attribute__((noreturn, no_sanitize_undefined)) __real_panic_abort(const char*);

void IRAM_ATTR __attribute__((noreturn, no_sanitize_undefined)) __wrap_panic_abort(const char *details)
{
	panicreason = details;
	__real_panic_abort(details);
}


void checkAndInstallCrashwrap()
{
	panic_info_t cd = { 0 };
	char buffer[768];
    nvs_handle_t handle;
	size_t length = sizeof(cd);

	ESP_LOGI("crashwrap", "Crashwrap Install" );

	if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK )
		return;

	if (nvs_get_blob(handle, "crashwrap", &cd, &length ) == ESP_OK && length >= sizeof( cd ) )
	{
		ESP_LOGW("crashwrap", "Crashwrap length: %d", length );
		ESP_LOGW("crashwrap", "Last Crash: ADDR: %08x FRAME: %08x EXCEPTION: %d", (uint32_t)cd.addr, (uint32_t)cd.frame, cd.exception);
	}

	length = sizeof(buffer)-1;
	if (nvs_get_blob(handle, "crashreason", buffer, &length ) == ESP_OK && length > 0 )
	{
		buffer[length] = 0;
		ESP_LOGW("crashwrap", "Reason: %s", buffer );
	}

	length = sizeof(buffer)-1;
	if (nvs_get_blob(handle, "crashdesc", buffer, &length ) == ESP_OK && length > 0 )
	{
		buffer[length] = 0;
		ESP_LOGW("crashwrap", "Description: %s", buffer );
	}

	length = sizeof(buffer)-1;
	if (nvs_get_blob(handle, "crashpanic", buffer, &length ) == ESP_OK && length > 0 )
	{
		buffer[length] = 0;
		ESP_LOGW("crashpanic", "Panic: %s", buffer );
	}


	XtExcFrame fr;
	length = sizeof( fr );
	if (nvs_get_blob(handle, "crashframe", &fr, &length ) == ESP_OK && length >= sizeof( fr ) )
	{
		ESP_LOGW("crashwrap", "EXIT: 0x%08x / PC: 0x%08x / PS: 0x%08x / A0: 0x%08x / A1: 0x%08x / SAR: 0x%08x / EXECCAUSE: 0x%08x / EXECVADDR: 0x%08x", 
			(uint32_t)fr.exit, (uint32_t)fr.pc, (uint32_t)fr.ps, (uint32_t)fr.a0, (uint32_t)fr.a1, (uint32_t)fr.sar, (uint32_t)fr.exccause, (uint32_t)fr.excvaddr );
	}

	nvs_close(handle);
}


#if 0
/*
	Neat stuff:
		* pm_noise_check_process
*/
int __real_ieee80211_ioctl(uint32_t a, uint32_t b, uint32_t c );
int __wrap_ieee80211_ioctl( uint32_t a, uint32_t b, uint32_t c )
{
	uint32_t * l = (uint32_t*)a;
	ESP_LOGE("crashwrap", "8ii: %08x { %08x %08x %08x %08x } %08x %08x", a, l[0], l[1], l[2], l[3], b, c );
	return __real_ieee80211_ioctl( a, b, c );
}


int __real_esp_wifi_internal_ioctl(uint32_t a, uint32_t b, uint32_t c );
int __wrap_esp_wifi_internal_ioctl( uint32_t a, uint32_t b, uint32_t c )
{
	ESP_LOGE("crashwrap", "wii: %08x %08x %08x", a, b, c );
	return __real_esp_wifi_internal_ioctl( a, b, c );
}



int __real_nvs_set_i8(uint32_t a, uint32_t b, uint32_t c );
int __wrap_nvs_set_i8(uint32_t a, uint32_t b, uint32_t c )
{
	ESP_LOGE("crashwrap", "nvs_set_i8: %08x %08x %08x", a, b, c );
	uint32_t * t = (uint32_t*)b;
	ESP_LOGE("crashwrap", "nvs_set_i8: %08x %08x (%08x %08x %08x)  %08x", a, b, t[0], t[1], t[2], c );
	
	return __real_nvs_set_i8( a, b, c );
}




int __real_nvs_set_u8(uint32_t a, uint32_t b, uint32_t c );
int __wrap_nvs_set_u8(uint32_t a, uint32_t b, uint32_t c )
{
	ESP_LOGE("crashwrap", "nvs_set_u8: %08x %08x %08x", a, b, c );
	uint32_t * t = (uint32_t*)b;
	ESP_LOGE("crashwrap", "nvs_set_u8: %08x %08x (%08x %08x %08x)  %08x", a, b, t[0], t[1], t[2], c );
	
	return __real_nvs_set_u8( a, b, c );
}


int __real_nvs_get_u8(uint32_t a, uint32_t b, uint32_t c );
int __wrap_nvs_get_u8(uint32_t a, uint32_t b, uint32_t c )
{
	ESP_LOGE("crashwrap", "nvs_get_u8: %08x %s %08x", a, (char*)b, c );
	return __real_nvs_get_u8( a, b, c );
}

esp_err_t __real_nvs_open(const char* name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t __wrap_nvs_open(const char* name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
	ESP_LOGE("crashwrap", "__real_nvs_open: %s %d %p", name, open_mode, out_handle );
	if( name == (char*)7 )
	{
		void esp_phy_erase_cal_data_in_nvs();
		esp_phy_erase_cal_data_in_nvs();
	}
	return __real_nvs_open(name, open_mode, out_handle);
}

#endif
