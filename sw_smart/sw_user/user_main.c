#include "esp_common.h"
#include "apps/sntp.h"

#include "sw_debug_log.h"
#include "sw_parameter.h"

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

static void printf_mac()
{
	int i = 0;
	char sta_mac[6] = {0};
	wifi_get_macaddr(STATION_IF, sta_mac);
	
	SW_LOG_DEBUG("ESP STATIN MAC:");
	for(i = 0; i < sizeof(sta_mac); i++)
		printf("%2X", sta_mac[i]);
	printf("\n");
}

static bool set_default_parameter(void)
{
	sw_parameter_clean();

	sw_parameter_set_int("boot_start", 1);

	sw_parameter_save();
	
}

static bool default_parameter()
{
	int buf;
	sw_parameter_init();
	
	if(!sw_parameter_get_int("boot_start", &buf))
		goto START;
	else if(buf == 0){
		goto START;
	}
	SW_LOG_DEBUG("system 1boot!\n");
	return true;

START:
	SW_LOG_DEBUG("system 0boot!,buf = %d\n", buf);
	return set_default_parameter();
}

static void start_sntp_server()
{

	uint32_t ip = 0;
	ip_addr_t *addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));
	
	//sntp_setservername(0, "us.pool.ntp.org");
	//sntp_setservername(1, "ntp.sjtu.edu.cn");
	
	ipaddr_aton("10.10.18.70", addr);
	sntp_setserver(0, addr);	

	ipaddr_aton("10.10.18.80", addr);
	sntp_setserver(1, addr);	
	
	sntp_set_timezone(8);

	sntp_init();
	os_free(addr);

	SW_LOG_DEBUG("sntp enabled!\n");
}

/**********************************************************************************/

bool sw_factory_settings()
{
	sw_parameter_set_int("boot_start", 0);
	return sw_parameter_save();
}

void sw_os_init()
{
	default_parameter();
	start_sntp_server();
	sw_getendtype();
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/

void user_init(void)
{
	int ret;

	SW_LOG_DEBUG("SDK version:%s\n", system_get_sdk_version());
	SW_LOG_DEBUG("ESP8266 chip ID:0x%x\n",	system_get_chip_id());
	printf_mac();

	sw_os_init();
	sw_factory_settings();
	sw_printf_param();
	
	sw_key_init();
	sw_set_led_twinkle(200);
}

