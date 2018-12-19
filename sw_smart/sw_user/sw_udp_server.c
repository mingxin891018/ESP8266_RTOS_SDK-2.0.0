/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_udp_server.c
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-15
*   Describe: null
*
*===============================================================*/
#include "sw_udp_server.h"

#include "esp_common.h"
#include "espconn/espconn.h"
#include "sw_udp_server.h"
#include "cJSON.h"

#include "user_config.h"
#include "sw_debug_log.h"
#include "sw_parameter.h"

#define PRODUCT_KEY "2dGkWmko"

struct sta_config
{
	uint8_t ssid[32];         /**< SSID of target AP*/
	uint8_t password[64];     /**< password of target AP*/
};

os_timer_t inform_timer;
os_timer_t progress_timer;
static struct espconn *m_udp_server_local = NULL;
static struct sta_config m_tmp_config;

void inform_devid_cb(void *param)
{
	char str[64] = {0};
	memset(str, 0, 64);

	sprintf(str,"{\"deviceId\":\"%s\",\"status\":0,\"version\":\"2.0\"}",sw_get_device_name());
	SW_LOG_INFO("udp smartconfig response data = %s",str);
	espconn_sent(m_udp_server_local, str, strlen(str));
}

void inform_cb(void *param)
{
	struct station_config inf;
	struct station_config sta_conf;
	int ret;

	if(wifi_get_opmode() == STATION_MODE)
		return;

	ret = wifi_set_opmode_current(STATION_MODE);
	if(!ret){
		SW_LOG_ERROR("set station mode failed!");
		return;
	}
	SW_LOG_INFO("Set STA mode success");

	memset(&sta_conf, 0, sizeof(struct station_config));
	memcpy(&sta_conf.ssid, m_tmp_config.ssid, 32);
	memcpy(&sta_conf.password, m_tmp_config.password, 64);
	sta_conf.bssid_set = 0;
	ret = wifi_station_set_config(&sta_conf);
	if(ret)
	{
		SW_LOG_INFO("SoftAP save success,Reboot to STA");

		memset(&inf,0,sizeof(inf));
		wifi_station_get_config(&inf);

		SW_LOG_INFO("SoftAP Config ssid = %s\n",inf.ssid);
		SW_LOG_INFO("SoftAP Config password = %s\n",inf.password);
		SW_LOG_INFO("SoftAP Config bssid_set = %d\n",inf.bssid_set);

		os_timer_disarm(&inform_timer);
		SW_LOG_INFO(" change to Smartconfig mode after reboot\n");

		system_restart();
	}
}

//{"url":"http://ehome.iot.sunniwell.net:7150/apis/device/register","productId":"2dGkWmko"}
bool analysis_jsdata(const char *data, WIFI_MODE mode)
{
	cJSON *root = NULL;
	cJSON *js = NULL;
	int length = 0;
	char str[64] = {0};

	root = cJSON_Parse(data);
	if(root == NULL){
		SW_LOG_DEBUG("recv data is not json!");
		return false;
	}

	switch(mode){
		case SOFTAP_MODE:
			SW_LOG_INFO("AP smartconfig!");
			js = cJSON_GetObjectItem(root,"fin");
			if(js && js->type == cJSON_Number){
				SW_LOG_INFO("AP smartconfig is finished,delete upd server,fin = %d",js->valueint);
				espconn_delete(m_udp_server_local);

				os_timer_disarm(&inform_timer);
				os_timer_setfn(&inform_timer, inform_cb, NULL);
				os_timer_arm(&inform_timer, 1000, 1);
			}

			js = cJSON_GetObjectItem(root,"ssid");
			if(js && js->type == cJSON_String){   
				SW_LOG_INFO("SSID = %s",js->valuestring);
				memcpy(m_tmp_config.ssid,js->valuestring,strlen(js->valuestring));
			}

			js = cJSON_GetObjectItem(root,"password");
			if(js && js->type == cJSON_String){
				SW_LOG_INFO("PWD=%s",js->valuestring);
				memcpy(m_tmp_config.password,js->valuestring,strlen(js->valuestring));
			}
			break;

		case STATION_MODE:
			SW_LOG_INFO("station smartconfig!");
			js = cJSON_GetObjectItem(root,"productId");
			if(js && js->type == cJSON_String){
				length = strlen(js->valuestring);
				if(length == strlen(PRODUCT_KEY) && (0 == strncmp(PRODUCT_KEY, js->valuestring, length))){
					SW_LOG_INFO("product_id check success!");
					
					sprintf(str,"{\"deviceId\":\"%s\",\"status\":0,\"version\":\"2.0\"}",sw_get_device_name());
					SW_LOG_INFO("udp smartconfig response data = %s",str);
					espconn_sent(m_udp_server_local, str, strlen(str));
					
					//配网开始，开始发送response 数据
					os_timer_disarm(&progress_timer);
					os_timer_setfn(&progress_timer, inform_devid_cb, NULL);
					os_timer_arm(&progress_timer, 100, 1);//循环回复
				}
				else{
					SW_LOG_INFO("productId check filed!");
				}
			}
			
			js = cJSON_GetObjectItem(root,"fin");
			if(js && js->type == cJSON_Number){
				espconn_delete(m_udp_server_local);
				
				//配网结束，停止发送response 数据
				os_timer_disarm(&progress_timer);
				
				sw_parameter_set_int("smartconfig_boot_finished", 1);
				sw_dev_register_init();
				sw_parameter_save();
			}
			break;
	}

	js = cJSON_GetObjectItem(root,"url");
	if(js && js->type == cJSON_String)
	{
		SW_LOG_INFO("sunniwell_url=%s",js->valuestring);
		sw_parameter_set("sunniwell_url", js->valuestring, strlen(js->valuestring));
	}

	cJSON_Delete(root);
	return true;
}

void udp_recv_cb(void *arg, char *pdata, unsigned short len)
{
	uint8_t mode = 0;
	m_udp_server_local = arg;
	remot_info *premot = NULL;
	char *pos;

	if(espconn_get_connection_info(m_udp_server_local,&premot,0) == ESPCONN_OK)
	{
		m_udp_server_local->proto.udp->remote_port = premot->remote_port;
		m_udp_server_local->proto.udp->remote_ip[0] = premot->remote_ip[0];
		m_udp_server_local->proto.udp->remote_ip[1] = premot->remote_ip[1];
		m_udp_server_local->proto.udp->remote_ip[2] = premot->remote_ip[2];
		m_udp_server_local->proto.udp->remote_ip[3] = premot->remote_ip[3];

		SW_LOG_DEBUG("recv data len:%d,from ip:%d.%d.%d.%d port:%d", len, m_udp_server_local->proto.udp->remote_ip[0],
				m_udp_server_local->proto.udp->remote_ip[1], m_udp_server_local->proto.udp->remote_ip[2],
				m_udp_server_local->proto.udp->remote_ip[3], m_udp_server_local->proto.udp->remote_port);
	}

	SW_LOG_DEBUG("recv data = %s", pdata);

	mode = wifi_get_opmode();
	if(strstr(pdata,"upgrade=1")){	//本地升级
		SW_LOG_DEBUG("upgrade start!");
	}
	else if(mode == SOFTAP_MODE || mode == STATION_MODE){
		SW_LOG_DEBUG("analysis udp recv data!");
		if((strncmp(pdata, "swiot_req=2dGkWmko", len) == 0) && (strlen("swiot_req=2dGkWmko") == len ))
			SW_LOG_INFO("productId is smart Plug");
			analysis_jsdata(pdata, wifi_get_opmode());
	}
	else{
		SW_LOG_DEBUG("dev mode error!can not analysis udp data");
	}
}

void udp_send_cb(void* arg)
{
	struct espconn* m_udp_server_local = arg;

	SW_LOG_DEBUG("send data to ip:%d.%d.%d.%d port:%d", m_udp_server_local->proto.udp->remote_ip[0],
			m_udp_server_local->proto.udp->remote_ip[1], m_udp_server_local->proto.udp->remote_ip[2],
			m_udp_server_local->proto.udp->remote_ip[3], m_udp_server_local->proto.udp->remote_port);
}

void sw_udp_server_create(char flag)
{   
	static struct espconn udp_client;
	static esp_udp udp;
	int8_t res = 0;

	udp_client.type = ESPCONN_UDP;
	udp_client.proto.udp = &udp;
	udp.local_port = UDP_SERVER_LOCAL_PORT;

	SW_LOG_DEBUG("udpServerCreate");

	espconn_regist_recvcb(&udp_client, udp_recv_cb);
	espconn_regist_sentcb(&udp_client, udp_send_cb);

	res = espconn_create(&udp_client);
	if (res != 0)
	{
		SW_LOG_ERROR("udp server creat err ret:%d", res);
	}

	m_udp_server_local = &udp_client;
}


