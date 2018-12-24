/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_dev_register.c
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-19
*   Describe: null
*
*===============================================================*/
#include "sw_dev_register.h"

#include "esp_common.h"
#include "espconn/espconn.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "cJSON.h"

#include "user_config.h"
#include "sw_common.h"
#include "sw_debug_log.h"
#include "sw_parameter.h"
#include "sw_udp_server.h"

#define PRODUCT_SECRET "H6rm0QYoTIcu9RNe"	//"D1BKC2EVnIl6LKPS"

#define DEV_REGISTER_NAME 	"tDevRegisterProc"
#define DEV_REG_SUN_NAME	"tDevRegSunniProc"

typedef enum {
	STATE_INIT_DATA = 1,
	STATE_CREATE_SOCK,
	STATE_CONNECT_SERVER,
	STATE_SEND,
	STATE_RECV,
	STATE_ANALYSIS
}connect_state_t;

enum {
	SICHUAN_MOFANG = 3,
	ZHEJIANG_DIANXIN = 4
};

xTaskHandle xRegisterTask_SW;
xTaskHandle xDevRegisterSunni_SW;

xSemaphoreHandle dev_reg_sunni_handle;

static int host2addr(const char *hostname , struct in_addr *in) 
{
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	rv = getaddrinfo(hostname, 0 , &hints , &servinfo);
	if (rv != 0)
	{    
		SW_LOG_ERROR("getaddrinfo error!,ret = %d.", rv);
		return rv;
	}    

	// loop through all the results and get the first resolve
	for (p = servinfo; p != 0; p = p->ai_next)
	{    
		h = (struct sockaddr_in *)p->ai_addr;
		in->s_addr = h->sin_addr.s_addr;
	}    

	freeaddrinfo(servinfo); // all done with this structure
	return 0;
}

/*
朝歌平台封包格式
{
	"code": 200,
		"msg": "OK",
		"result": {
			"username": "2dGkWmko.80:7D:3A:7C:A1:C9",
			"secret": "7kF3qhtS6c1QDcO4",
			"sslEndpoint": "ssl://120.24.69.144:8883",
			"tcpEndpoint": "tcp://120.24.69.144:1883",
			"channel": ""
		},
		"ok": true
}
*/
int connect_analysis(const char *buf, int recv_len)
{
	char param_buf[128] = {0};
	char *pos = NULL, *pos2 = NULL;  
	cJSON *root = NULL, *js = NULL, *js_result = NULL;

	if(buf == NULL || strlen(buf) == 0){
		SW_LOG_INFO("param error!");
		return -1;
	}

	pos = strstr(buf,"\r\n\r\n");
	if(pos == NULL){
		SW_LOG_ERROR("http head over error!");
		return -1;
	}
	
	pos2 = strchr(pos, '{');
	if(pos == NULL){
		SW_LOG_ERROR("check js over { error!");
		return -1;
	}

	root = cJSON_Parse(pos2);
	if(root == NULL){
		SW_LOG_ERROR("js root error!");
		return -1;
	}

	js = cJSON_GetObjectItem(root,"code");
	if(js && js->type == cJSON_Number){
		SW_LOG_INFO("code = %d\n",js->valueint);
		if(js->valueint != 200){
			goto ANALY_ERROR;
		}
	}

	js_result = cJSON_GetObjectItem(root,"result");//zjdx  scdx  dxjt
	if(js_result && js_result->type == cJSON_Object){
		js = cJSON_GetObjectItem(js_result,"channel");//zjdx  scdx  dxjt
		if(js && js->type == cJSON_String){
			SW_LOG_INFO("channel_flag = %s", js->valuestring);
			if(memcmp(js->valuestring,"scdx",4)== 0)
				sw_parameter_set_int("channel_flag", 3);
		}
	}

	js = cJSON_GetObjectItem(js_result,"secret");
	if(js && js->type == cJSON_String){
		SW_LOG_INFO("json secret=%s\n",js->valuestring);

		memset(param_buf, 0, sizeof(param_buf));
		strcpy(param_buf, js->valuestring);
		sw_parameter_set("mqtt_secret", param_buf, strlen(param_buf));
	}

	js = cJSON_GetObjectItem(js_result,"tcpEndpoint");
	if(js && js->type == cJSON_String){
		SW_LOG_INFO("json tcpEndpoint=%s\n",js->valuestring);

		memset(param_buf, 0, sizeof(param_buf));
		strcpy(param_buf, js->valuestring);
		sw_parameter_set("mqtt_url", param_buf, strlen(param_buf));
	}

	js = cJSON_GetObjectItem(js_result,"username");
	if(js && js->type == cJSON_String)
	{    
		SW_LOG_INFO("json username=%s\n",js->valuestring);

		memset(param_buf, 0, sizeof(param_buf));
		strcpy(param_buf, js->valuestring);
		sw_parameter_set("mqtt_username", param_buf, strlen(param_buf));
	}

	js = cJSON_GetObjectItem(js_result,"sslEndpoint");
	if(js && js->type == cJSON_String)
	{    
		SW_LOG_INFO("json sslEndpoint=%s\n",js->valuestring);

		memset(param_buf, 0, sizeof(param_buf));
		strcpy(param_buf, js->valuestring);
		sw_parameter_set("mqtt_sslendpoint", param_buf, strlen(param_buf));
	}

	js = cJSON_GetObjectItem(js_result,"channel");
	if(js && js->type == cJSON_String)
	{    
		SW_LOG_INFO("json channel=%s\n",js->valuestring);

		memset(param_buf, 0, sizeof(param_buf));
		strcpy(param_buf, js->valuestring);
		sw_parameter_set("channel_flag", param_buf, strlen(param_buf));
	}
	sw_parameter_save();
	SW_LOG_INFO("end!");

ANALY_ERROR:
	if(root)
		cJSON_Delete(root);

	return 0;
}

bool connect_recv(int sock_fd, char *buf, int buf_length, int *recv_len)
{
	static int recv_nu = 0;

	if(recv_nu >= 5){
		SW_LOG_INFO("recv try count = %d.", recv_nu);
		recv_len = 0;
		return true;
	}

	memset(buf, 0, buf_length);
	if(sock_fd < 0 || buf == NULL || buf_length < 0 || recv_len == NULL){
		SW_LOG_ERROR("param error!");
		return false;
	}

	*recv_len = recv(sock_fd, buf, buf_length, 0); 
	if(*recv_len <= 0){
		SW_LOG_ERROR("recv data error, ret = %d.", *recv_len);
		recv_nu ++;
		return false;
	}

	SW_LOG_INFO("recv data length=%d,data:\n%s", *recv_len, buf);

	return true;
}

bool connect_send(int sock_fd, char *buf, int length)
{
	int ret = 0;
	static int send_nu = 0;

	if(send_nu >= 5){
		SW_LOG_INFO("send try count = %d.", send_nu);
		send_nu = 0;
		return true;
	}

	if(sock_fd < 0){
		SW_LOG_ERROR("sock_fd=%d, error!", sock_fd);
		return false;
	}

	ret = send(sock_fd, buf, length, 0);
	if(ret <= 0){
		SW_LOG_ERROR("send data failed, ret = %d.", ret);
		send_nu ++;
		return false;
	}

	SW_LOG_INFO("send data length = %d.", ret);
	return true;
}

bool connect_server(int sock_fd, uint16_t server_port, const char *server_ip)
{
	int ret = -1;
	static int count = 0;
	struct in_addr in;
	struct sockaddr_in server_addr;

	if(count >= 5){
		SW_LOG_INFO("connect try conunt=%d!", count);
		count = 0;
		return true;
	}
	if(sock_fd < 0){
		SW_LOG_ERROR("sock_fd=%d, error!", sock_fd);
		return false;
	}

	if(1 != inet_aton( server_ip, &in.s_addr )){
		SW_LOG_ERROR("ip string is error! ip=%s", server_ip);
	}
	
	//connect
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = in;
	server_addr.sin_port = htons(server_port);
	
	SW_LOG_INFO("ip_str = %s, ip = %0x, port = %0x", server_ip, server_addr.sin_addr.s_addr, server_addr.sin_port);
	ret = connect(sock_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
	if(ret < 0){
		SW_LOG_ERROR("connect error!, ret = %d", ret)
		count ++;
		return false;
	}
	SW_LOG_INFO("connect server %s successed!", server_ip);
	return true;
}

bool create_socket(int *sock_fd)
{
	//socket
	*sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(*sock_fd < 0)
		return false;
	
/*
	//bind
	struct sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(struct sockaddr_un));
	local_addr.sun_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = 80;
	if(bind(*sock_fd, &local_addr, sizeof(struct sockaddr_in)) < 0){
		SW_LOG_ERROR("bind error!");
		*sock_fd = -1;
		retufn false;
	}
*/
	SW_LOG_INFO("create socket success, sock_fd = %d.", *sock_fd);
	return true;
}

bool init_send_data(char *buf, int length, const char *path, const char * ip_str)
{
	int i = 0;
	char *content = NULL;
	char sign[64] = {0};
	char timestamp_str[32] = {0};
	content = malloc(256);
	
	if(content == NULL || buf == NULL){
		SW_LOG_ERROR("malloc content failed or send buf is null.");
		return false;
	}

	memset(buf, 0, length);
	u32_t timestamp = sntp_get_current_timestamp();

	if(timestamp == 0){
		sprintf(timestamp_str, "%d", 0);
	}
	else{ 
		sprintf(timestamp_str, "%d000", timestamp);
	}

	memset(content, 0, 256);
	memset(sign, 0, sizeof(sign));

	sprintf(content, "deviceName%s_productKey%s_timestamp%s", sw_get_device_name(), PRODUCT_KEY, timestamp_str);
	SW_LOG_INFO("content=%s.", content);
	
	hmac_sha256_get(sign,content,strlen(content),PRODUCT_SECRET,strlen(PRODUCT_SECRET));
	for(i=0; i<32; i++) {    
		sprintf(&buf[i*2],"%02x",sign[i]);
	}    
	SW_LOG_INFO("sign = %s",buf);

	memset(content, 0, 256);
	snprintf(content, 256,"productId=%s&deviceId=%s&sign=%s&timestamp=%s&signMethod=HmacSHA256", 
			PRODUCT_KEY, sw_get_device_name(), buf, timestamp_str);

	memset(buf, 0, length);
	snprintf(buf, length, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type:    application/x-www-form-urlencoded\r\nContent-length: %d\r\n\r\n%s",
			path, ip_str, strlen(content), content);
	SW_LOG_INFO("send buf:\n%s", buf);

	if(content)
		free(content);
	return true;
}

//http://ehome.iot.sunniwell.net:7150/apis/device/register
static int do_register_dev(const char *url)
{
	static int recy_count = 0;
	char *buf = NULL;
	bool exit = false;
	int flag = 0, sock_fd = -1, connect_fd = -1, recv_len = -1, ret = -1;
	uint16_t port = 0;
	connect_state_t connect_state = STATE_INIT_DATA;
	sw_url_t *url_info = NULL;
	struct in_addr ip = {0};

	SW_LOG_INFO("register url=%s", url);
	url_info = (sw_url_t *)malloc(sizeof(sw_url_t));
	if(url_info == NULL){
		SW_LOG_ERROR("malloc sw_url_t failed!");
		goto REG_ERR;
	}

	buf = malloc(512);
	if(buf == NULL){
		SW_LOG_ERROR("malloc buf failed!");
		goto REG_ERR;
	}

	//解析url
	memset(url_info, 0, sizeof(sw_url_t));
	if(sw_url_parse(url_info, url) < 0){
		SW_LOG_ERROR("parse url failed!");
		goto REG_ERR;
	}

	//获取端口号
	port = ((url_info->port &0x00FF)<<8)|((url_info->port&0xFF00)>>8);

	//获取IP
	if(url_info->is_ip){
		SW_LOG_INFO("ip = %d, port = %d",url_info->ip, port);
	}
	else{
		SW_LOG_INFO("hostname = %s, port = %d",url_info->hostname, port);
GET_AGAIN:
		ret = host2addr(url_info->hostname, &ip);
		if(ret == 202){
			vTaskDelay(3000 / portTICK_RATE_MS);
			goto GET_AGAIN;
		}else if(ret == 0)
			SW_LOG_INFO("ip_str=%s, ip = %0x", inet_ntoa(ip), ip);
	}

	//获取URL路径
	SW_LOG_INFO("path=%s",url_info->path);

	while(!exit){
		switch(connect_state){
			case STATE_INIT_DATA:
				if(init_send_data(buf, 512, url_info->path, inet_ntoa(ip))){
					connect_state = STATE_CREATE_SOCK;
				}else{
					SW_LOG_ERROR("init send buf failed!");
					connect_state = STATE_INIT_DATA;
					vTaskDelay(3000 / portTICK_RATE_MS);
				}
				break;

			case STATE_CREATE_SOCK:
				if(create_socket(&sock_fd))
					connect_state = STATE_CONNECT_SERVER;
				else{
					vTaskDelay(3000 / portTICK_RATE_MS);
					connect_state = STATE_CREATE_SOCK;
				}
				break;

			case STATE_CONNECT_SERVER:
				if(connect_server(sock_fd, port, inet_ntoa(ip)))
					connect_state = STATE_SEND;
				else{
					connect_state = STATE_CONNECT_SERVER;
					vTaskDelay(3000 / portTICK_RATE_MS);
				}
				break;

			case STATE_SEND:
				if(connect_send(sock_fd, buf, strlen(buf)))
					connect_state = STATE_RECV;
				else{
					connect_state = STATE_SEND;
					vTaskDelay(3000 / portTICK_RATE_MS);
				}
				break;

			case STATE_RECV:
				if(connect_recv(sock_fd, buf, 512, &recv_len))
					connect_state = STATE_ANALYSIS;
				else{
					if(recy_count > 2){
						connect_state = STATE_ANALYSIS;
						break;
					}
					connect_state = STATE_SEND;
					recy_count ++;
					vTaskDelay(3000 / portTICK_RATE_MS);
				}
				break;

			case STATE_ANALYSIS:
				flag = connect_analysis(buf, recv_len);
				exit = true;
		}
	}

REG_ERR:
	if(sock_fd >= 0)
		close(sock_fd);
	if(buf != NULL)
		free(buf);
	if(url_info != NULL)
		free(url_info);
	return flag;

}

static void ICACHE_FLASH_ATTR dev_register_sunniwell(void *param)
{
	int channel_flag = -1;
	char buf[PARAMETER_MAX_LEN];
	
	memset(buf, 0, sizeof(buf));
	sw_parameter_get("sunniwell_url", buf, sizeof(buf));
	if(strlen(buf) == 0){
		goto SUN_RETURN;
	}

	sw_network_config_vsem_token();
	channel_flag = do_register_dev(buf);
	if(channel_flag > 0)
		sw_parameter_set_int("channel_flag", channel_flag);

SUN_RETURN:
	xSemaphoreGive(dev_reg_sunni_handle);
	vTaskDelete(NULL);

}

static bool do_register_platfrom(int channel_flag)
{
	if(channel_flag < 0){
		SW_LOG_ERROR("channel_flag error!");
		return false;
	}

	switch(channel_flag){
		case SICHUAN_MOFANG:
			SW_LOG_INFO("platform is SICHUAN_MOFANG!");

			break;
		case ZHEJIANG_DIANXIN:
			SW_LOG_INFO("platform is ZHEJIANG_DIANXIN!");

			break;
	}

	return false;
}

static void ICACHE_FLASH_ATTR dev_register_task_proc(void *param)
{
	int channel_flag = -1, ret;

	sw_parameter_get_int("channel_flag", &channel_flag);
	SW_LOG_DEBUG("param[channel_flag]=%d", channel_flag);

	if(channel_flag <= 0){
		ret = xTaskCreate(dev_register_sunniwell, (uint8 const *)DEV_REG_SUN_NAME, 1024*3, NULL, tskIDLE_PRIORITY+2, &xDevRegisterSunni_SW);
		if (ret != pdPASS){    
			SW_LOG_ERROR("create thread %s failed!\n", DEV_REG_SUN_NAME);
			goto ERR_RETURN;
		}
		SW_LOG_ERROR("create thread %s successed!\n", DEV_REG_SUN_NAME);
		
		xSemaphoreTake(dev_reg_sunni_handle, portMAX_DELAY);
		SW_LOG_INFO("Semaphore return");
	}
	
	sw_parameter_get_int("channel_flag", &channel_flag);
	if(do_register_platfrom(channel_flag)){
		sw_parameter_save();
	}

ERR_RETURN:
	vTaskDelete(NULL);
}

bool sw_dev_register_init()
{
	int ret;
	
	vSemaphoreCreateBinary(dev_reg_sunni_handle);
	xSemaphoreTake(dev_reg_sunni_handle, 0);

	ret = xTaskCreate(dev_register_task_proc, (uint8 const *)DEV_REGISTER_NAME, 1024*3, NULL, tskIDLE_PRIORITY+2, &xRegisterTask_SW);
	if (ret != pdPASS){    
		SW_LOG_ERROR("create thread %s failed!\n", DEV_REGISTER_NAME);
	}
	SW_LOG_ERROR("create thread %s successed!\n", DEV_REGISTER_NAME);

}



