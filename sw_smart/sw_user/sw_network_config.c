/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_network_config.c
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-13
*   Describe: null
*
*===============================================================*/
#include "sw_network_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

#include "lwip/netif.h"
#include "esp_sta.h"
#include "espressif/esp_timer.h"
#include "espressif/esp_misc.h"
#include "smartconfig.h"

#include "sw_debug_log.h"
#include "sw_os.h"

#define SMART_CONFIG_NAME "tNetcfgProc"

xTaskHandle xSmartConfig_SW;
sw_vsemaphore_handle m_getip_sem_handle;
os_timer_t m_smartconfig_timer;//配网超时
static bool m_smartconfig_finished = false;

//wifi事件回调接口
void wifi_handle_event_cb(System_Event_t *evt)
{
	//SW_LOG_INFO("event %x", evt->event_id);
	switch (evt->event_id) {
		case EVENT_STAMODE_CONNECTED:
			SW_LOG_INFO("connect to ssid %s, channel %d",
					evt->event_info.connected.ssid, evt->event_info.connected.channel);
			break;
		case EVENT_STAMODE_DISCONNECTED:
			SW_LOG_INFO("disconnect from ssid %s, reason %d",
					evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE:
			SW_LOG_INFO("mode: %d -> %d",
					evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
			break;
		case EVENT_STAMODE_GOT_IP:
			SW_LOG_INFO("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
					IP2STR(&evt->event_info.got_ip.ip),
					IP2STR(&evt->event_info.got_ip.mask),
					IP2STR(&evt->event_info.got_ip.gw));
			printf("\n");
			sw_vsemaphore_give(m_getip_sem_handle);
			set_led1_on();
			break;
		case EVENT_SOFTAPMODE_STACONNECTED:
			SW_LOG_INFO("station: " MACSTR "join, AID = %d",
					MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED:
			SW_LOG_INFO("station: " MACSTR "leave, AID = %d",
					MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
			break;
		default:
			break;
	}
}

//注册wifi事件回调接口,监控wifi网络O
bool sw_set_wifi_cb(void)
{
	struct station_config sta_conf = {0};
	uint8_t stat = wifi_get_opmode();

	memset(&sta_conf, 0, sizeof(struct station_config));
	switch(stat){
		case 1:SW_LOG_INFO("WIFI is station mode");break;
		case 2:SW_LOG_INFO("WIFI is soft-AP mode");break;
		case 3:SW_LOG_INFO("WIFI is station + soft-AP moden");break;
	}
	if(!wifi_station_get_config(&sta_conf))
		return false;
	SW_LOG_INFO("Got wifi info:");
	SW_LOG_INFO("AP ssid = %s", sta_conf.ssid);
	SW_LOG_INFO("AP password = %s", sta_conf.password);
	SW_LOG_INFO("AP bssid = %02x:%02x:%02x:%02x:%02x:%02x:", sta_conf.bssid[0], sta_conf.bssid[1], sta_conf.bssid[2], sta_conf.bssid[3], sta_conf.bssid[4], sta_conf.bssid[5]);
	wifi_set_event_handler_cb(wifi_handle_event_cb);
}

void ICACHE_FLASH_ATTR smartconfig_done(sc_status status, void *pdata)
{
	uint8_t phone_ip[4] = {0};
	struct station_config *sta_conf = NULL;

	SW_LOG_INFO("start config start, please send broadcast.\n");
	switch(status) {
		case SC_STATUS_WAIT:
			SW_LOG_INFO("SC_STATUS_WAIT\n");
			break;
		case SC_STATUS_FIND_CHANNEL:
			SW_LOG_INFO("SC_STATUS_FIND_CHANNEL\n");
			break;
		case SC_STATUS_GETTING_SSID_PSWD:
			SW_LOG_INFO("SC_STATUS_GETTING_SSID_PSWD\n");
			sc_type *type = pdata;
			if (*type == SC_TYPE_ESPTOUCH) {
				SW_LOG_INFO("SC_TYPE:SC_TYPE_ESPTOUCH\n");
			} else {
				SW_LOG_INFO("SC_TYPE:SC_TYPE_AIRKISS\n");
			}
			break;
		case SC_STATUS_LINK:
			sta_conf = pdata;
			wifi_station_set_config(sta_conf);
			wifi_station_disconnect();
			wifi_station_connect();

			SW_LOG_INFO("SC_STATUS_LINK");
			SW_LOG_INFO("AP ssid = %s", sta_conf->ssid);
			SW_LOG_INFO("AP password = %s", sta_conf->password);
			SW_LOG_INFO("AP bssid = %02x:%02x:%02x:%02x:%02x:%02x:",
					sta_conf->bssid[0], sta_conf->bssid[1], sta_conf->bssid[2],
					sta_conf->bssid[3], sta_conf->bssid[4], sta_conf->bssid[5]);
			break;
		case SC_STATUS_LINK_OVER:
			SW_LOG_INFO("SC_STATUS_LINK_OVER");
			if (pdata != NULL) {
				memcpy(phone_ip, (uint8*)pdata, 4);
				SW_LOG_INFO("Phone ip: %d.%d.%d.%d",
						phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
			}
			smartconfig_stop();
			SW_LOG_INFO("smartconfig already link config AP");
			sw_parameter_set_int("smartconfig_boot_finished", 0);
			sw_parameter_save();
			m_smartconfig_finished = true;
			sw_vsemaphore_give(m_getip_sem_handle);
			break;
	}
}

static void smartconfig_timer_cb(void)
{
	SW_LOG_INFO("smartconfig timeout, restart smartconfig");

	if(!m_smartconfig_finished){
		os_timer_arm(&m_smartconfig_timer, 4*60*1000, 0);
		smartconfig_stop();
		smartconfig_start(smartconfig_done, 1);
	}
	else{
		SW_LOG_INFO("smartconfig is OK,stop smartconfig timer!");
	}
}

//smartconfig配网线程回调函数
void network_config_proc(void *param)
{
	SW_LOG_INFO("smartconfig start");
	os_timer_disarm(&m_smartconfig_timer);
	os_timer_setfn(&m_smartconfig_timer, (os_timer_func_t *)smartconfig_timer_cb, NULL);
	os_timer_arm(&m_smartconfig_timer, 4*60*1000, 0);
	SW_LOG_INFO("smartconfig set os timer OK");

	wifi_set_opmode_current(STATION_MODE);
	smartconfig_start(smartconfig_done, 1);
	SW_LOG_INFO("set smartconfig_done ok.");
	
	vTaskDelete(NULL);
}

//外部调用等待wifi网络OK
void sw_network_config_vsem_token()
{
	sw_vsemaphore_take(m_getip_sem_handle, portMAX_DELAY);
}

bool sw_network_config_init(void)
{
	sw_vsemaphore_create(m_getip_sem_handle);
	sw_vsemaphore_take(m_getip_sem_handle, 0);

	return true;
}

//开启smart_config
bool sw_network_config_start(int number)
{
	int ret;
	wifi_station_disconnect();
	sw_set_led_twinkle(200);
	ret = xTaskCreate(network_config_proc, SMART_CONFIG_NAME, 256,  NULL, tskIDLE_PRIORITY+2, &xSmartConfig_SW);
	if (ret != pdPASS){
		SW_LOG_ERROR("create thread %s failed", SMART_CONFIG_NAME);
	}
	SW_LOG_INFO("create thread %s success", SMART_CONFIG_NAME);

	return true;
}




























































