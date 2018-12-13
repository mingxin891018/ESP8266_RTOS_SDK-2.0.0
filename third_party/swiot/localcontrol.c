#include "swiot_common.h"
#include "swiot_platform.h"
#include "localcontrol.h"
#include "udpcast.h"
#include "cJSON.h"

#define TCP_LISTEN_IP   "127.0.0.1"
#define TCP_LISTEN_PORT 4004

#define UDP_BROADCAST_IP "255.255.255.255"
#define UDP_BROADCAST_PORT 4005

#define UDP_BROADCAST_DATA "{\"port\":%d,\"devicename\":\"%s\"}"

#define WORK_THREAD_STACK_SIZE 1024

#define SEND_IRCODE_URI "/send/ircode"
#define MATCH_SWITCH_URI "/math/switch"
#define CHECK_UPGRADE_URI "/check/upgrade"
#define HEART_BEAT        "/heartBeat"

#define LCWORKING 0
#define LCEXITED 1
typedef struct 
{
	swiot_lccmd_callback cmd_callback;
	void* callback_param;
	void*                  thread_handle;
	int status;
}swiot_lchandle_t;

swiot_lchandle_t g_lc_handle;

void http_packet_handle( void* client,char* method,char* uri,char* palyload,int head_len,int size,int session_id,void* param );
lc_controy_e get_cmd_type(char*method,char*uri);
char* get_cmd_msg(char* data,int data_size);

void work_threadlc(void *params);

/**
* @本地控制初始化
*/
int SWIOT_LC_Construct(char* sn)
{
	int rc = -1;
	char tmp[128] = {0};
	memset(&g_lc_handle,0,sizeof(swiot_lchandle_t));
	if (0 != swiot_http_server_init(TCP_LISTEN_IP,TCP_LISTEN_PORT,http_packet_handle,NULL)) {
		log_error("Init http server error!\n");
		goto ERROR;
	}

	rc = snprintf(tmp,sizeof(tmp),UDP_BROADCAST_DATA,TCP_LISTEN_PORT,sn);
	if (0 != swiot_udpcast_start(UDP_BROADCAST_IP,UDP_BROADCAST_PORT,tmp,rc)) {
		log_error("Init udpcast error!\n");
		goto ERROR;
	}
	
	g_lc_handle.status = LCWORKING;
	rc = SWIOT_Thread_Create(g_lc_handle.thread_handle,work_threadlc,"lc_thread",WORK_THREAD_STACK_SIZE, NULL,125 );
	if (0 == rc) 
	{
		log_error("Create thread error\n");
		goto ERROR;
	}

	return 0;
ERROR:
	swiot_udpcast_stop();
	swiot_http_server_destroy();
	return -1;
}

/**
* @销毁本地控制模块
*/
int SWIOT_LC_Destroy()
{
	swiot_udpcast_stop();
	swiot_http_server_destroy();

	g_lc_handle.status = LCEXITED;
	return 0;
}


/**
* @注册命令回调函数
* @param cmd_handle [命令回调函数]
* @param param      [回调参数]
*/
int SWIOT_LC_Register(swiot_lccmd_callback cmd_handle,void* param)
{
	g_lc_handle.cmd_callback = cmd_handle;
	g_lc_handle.callback_param = param;
	return 0;
}

/**
* @命令回复
* @param cmd_id [命令ID]
* @param code   [回复码]
*/
int SWIOT_LC_Response(void* cmd_handle,int cmd_id,int code,char* data,int data_size)
{
	return swiot_http_server_response(cmd_handle,cmd_id,code,data,data_size);
}

void http_packet_handle( void* client,char* method,char* uri,char* palyload,int head_len,int size,int session_id,void* param )
{
	lc_controy_e cmd_type;
	char* cmd =NULL ;
	
	log_info("method:%s uri:%s palyload:%s head_len:%d size:%d session_id:%d\n",method,uri,palyload,head_len,size,session_id);

	cmd_type = get_cmd_type(method,uri);
	if (UNKOWN_CMD == cmd_type) {
		log_error("unkown cmd\n");
		SWIOT_LC_Response(client,session_id,404,NULL,0);
		return;
	} else if(HEART_BEAT_CMD == cmd_type) {
		SWIOT_LC_Response(client,session_id,200,NULL,0);
		return;
	}

	if (NULL == (cmd=get_cmd_msg(palyload+head_len,size-head_len))) {
		log_error("get cmd error,palyload=%s\n",palyload+head_len);
		SWIOT_LC_Response(client,session_id,404,NULL,0);
		return;
	}

	if (g_lc_handle.cmd_callback)
		g_lc_handle.cmd_callback(cmd_type,client,session_id,cmd,g_lc_handle.callback_param);

	if (cmd)
		free(cmd);
}	

lc_controy_e get_cmd_type(char*method,char*uri)
{
	lc_controy_e cmd_type=UNKOWN_CMD;
	if(memcmp(method,"POST",4) != 0) {
		log_error("the method is not right,method=%s\n",method);
		return UNKOWN_CMD;
	}

	if (strlen(uri) == strlen(SEND_IRCODE_URI)
		&&memcmp(uri,SEND_IRCODE_URI,strlen(SEND_IRCODE_URI)) == 0) {
		cmd_type = SEND_IRCODE_CMD;
	} else if (strlen(uri) == strlen(MATCH_SWITCH_URI)
		&&memcmp(uri,MATCH_SWITCH_URI,strlen(MATCH_SWITCH_URI)) == 0) {
		cmd_type = MATCH_SWITCH_CMD;
	} else if (strlen(uri) == strlen(CHECK_UPGRADE_URI)
		&&memcmp(uri,CHECK_UPGRADE_URI,strlen(CHECK_UPGRADE_URI)) == 0) {
		cmd_type = CHECK_UPGRADE_CMD;
	}  else if (strlen(uri) == strlen(HEART_BEAT)
		&&memcmp(uri,HEART_BEAT,strlen(HEART_BEAT)) == 0) {
		cmd_type = HEART_BEAT_CMD;
	}
	return cmd_type;
}
char* get_cmd_msg(char* data,int data_size)
{
	cJSON* json = NULL;
	cJSON* sub_json= NULL;
	char* str = NULL;

	json = cJSON_Parse(data);
	if (NULL == json) {
		log_error("Parser json error,json=%s\n",data);
		goto END;
	}

	sub_json = cJSON_GetObjectItem(json,"params");
	if (NULL == json) {
		log_error("Can't get \"params\",json=%s\n",data);
		goto END;
	}

	str = cJSON_Print(sub_json);
	if (NULL == str) {
		log_error("Can't conver to string,json=%s\n",data);
		goto END;
	}
END:
	if (json)
		cJSON_Delete(json);
	return str;
}

void ICACHE_FLASH_ATTR work_threadlc(void *params){
	int now = 0;
	while(LCWORKING == g_lc_handle.status) {
		now = SWIOT_Common_Get_Ms();
		swiot_http_server_ontimer(now);
		swiot_udpcast_ontimer(now);
		SWIOT_msleep(1);
	}

	 vTaskDelete(NULL);
}