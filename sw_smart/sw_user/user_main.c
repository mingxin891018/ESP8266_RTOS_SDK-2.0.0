#include "esp_common.h"
#include "apps/sntp.h"

#include "user_config.h"
#include "sw_debug_log.h"
#include "sw_parameter.h"
#include "sw_os.h"

static smart_dev_t* m_dev = NULL;

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

static bool set_default_parameter(void)
{
	char buf[PARAMETER_MAX_LEN];

	sw_parameter_clean();

	sw_parameter_set_int("boot_start", 1);

	memset(buf, 0, PARAMETER_MAX_LEN);
	strcpy(buf, "SW_PLUG_2.0.6");
	sw_parameter_set("software_version", buf, strlen(buf));

	memset(buf, 0, PARAMETER_MAX_LEN);
	strcpy(buf, "HYS-01-066-V1.0");
	sw_parameter_set("hardware_version", buf, strlen(buf));

	memset(buf, 0, PARAMETER_MAX_LEN);
	strcpy(buf, "S-IoTZ501");
	sw_parameter_set("dev_mode", buf, strlen(buf));

	memset(buf, 0, PARAMETER_MAX_LEN);
	strcpy(buf, "SmartSocket");
	sw_parameter_set("dev_name", buf, strlen(buf));

	memset(buf, 0, PARAMETER_MAX_LEN);
	strcpy(buf, "Sunniwell");
	sw_parameter_set("manufacturer", buf, strlen(buf));

	sw_parameter_save();

}

static bool load_parameter()
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

bool sw_os_init()
{
	smart_dev_t *dev;
	char sta_mac[6] = {0};

	load_parameter();
	start_sntp_server();
	sw_getendtype();
	dev = (smart_dev_t*)malloc(sizeof(smart_dev_t));
	if(dev == NULL){
		SW_LOG_ERROR("malloc smart_dev_t failed!");
		return false;
	}
	memset(dev, 0, sizeof(smart_dev_t));
	dev->dev_state.mode = MODE_DISCONNECTED_ROUTER;
	dev->dev_state.is_connected_server = false;
	dev->dev_state.state = 0;
	dev->dev_state.powerSwitch = 0;
	dev->dev_state.save_flag = 0;

	wifi_get_macaddr(STATION_IF, sta_mac);

	sprintf(dev->dev_para.device_name , "%02X:%02X:%02X:%02X:%02X:%02X",sta_mac[0], sta_mac[1],sta_mac[2],sta_mac[3],sta_mac[4],sta_mac[5]);
	SW_LOG_DEBUG("MAC=%s", dev->dev_para.device_name);
	dev->dev_para.save_flag = 0;
	dev->dev_para.channel_flag = -1;

	m_dev = dev;
}

smart_dev_t *sw_get_devinfo(void)
{
	return m_dev;
}

dev_state_t sw_get_dev_state(void)
{
	if(m_dev == NULL){
		SW_LOG_DEBUG("dev is not init");
		return MODE_MAX;
	}
	return m_dev->dev_state.mode;
}

char *sw_get_device_name(void)
{
	if(m_dev == NULL){
		SW_LOG_DEBUG("dev is not init");
		return NULL;
	}
	return m_dev->dev_para.device_name;

}

bool sw_set_dev_state(dev_state_t state)
{
	if(m_dev == NULL){
		SW_LOG_DEBUG("dev is not init");
		return false;
	}
	SW_LOG_DEBUG("dev mode changed:%d -> %d", m_dev->dev_state.mode, state);
	m_dev->dev_state.mode = state;
	return true;
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/
void user_init(void)
{
	int smart_config_flag = -1, ret = -1;

	SW_LOG_DEBUG("SDK version:%s\n", system_get_sdk_version());
	SW_LOG_DEBUG("ESP8266 chip ID:0x%x\n",	system_get_chip_id());

	if(!sw_os_init())
		return;
//	sw_factory_settings();
	sw_printf_param();
	sw_set_wifi_cb();

	sw_gpio_init();

	sw_network_config_init();
	sw_udp_server_create(1);

	sw_parameter_get_int("smartconfig_boot_finished", &smart_config_flag);
	if(smart_config_flag != 1){
		SW_LOG_INFO("mast smartconfig first!");
	}
	else{
		SW_LOG_INFO("param[smartconfig_boot_finished]=%d", smart_config_flag);
		sw_dev_register_init();
	}
}

