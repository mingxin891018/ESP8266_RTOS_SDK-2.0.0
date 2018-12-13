#include <stdio.h>
#include <string.h>
#include "swiot_common.h"
#include "openssl/ssl.h"
#include "cJSON.h"
#include "mqtt/MQTTClient.h"


#define CJSON_GETINT(node,value) if (cJSON_Number == (node)->type) {\
								 	(value) = (node)->valueint;\
								 } else if (cJSON_String == (node)->type) {\
								 	(value) = atoi((node)->valuestring);\
								 } else {\
								 	log_error("get id error\n");\
								 	(value) = 0;\
								 }

#define CJSON_GETSTRING(node,pstr) if (cJSON_String == (node)->type) {\
								 		(pstr) = (node)->valuestring;\
								 	} else\
								 		(pstr) = "";


#define SSL_CA_CERT_KEY_INIT(s,a,b,c,d,e,f)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\
                                             ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\
                                             ((ssl_ca_crt_key_t *)s)->cert = c;\
                                             ((ssl_ca_crt_key_t *)s)->cert_len = d;\
                                             ((ssl_ca_crt_key_t *)s)->key = e;\
                                             ((ssl_ca_crt_key_t *)s)->key_len = f;

typedef struct 
{
	char client_id[80];
	char username[80];
	char password[80];
}swiot_device_count_t,*swiot_device_count_pt;

typedef struct 
{
	char host[80];
	int  port;
	const char* pub_key;
}swiot_device_connect_t,*swiot_device_connect_pt;

typedef struct 
{
	MQTTClient* mqtt;										/*mqtt handle*/	
	swiot_device_count_t count;								/*mqtt count*/
	swiot_device_connect_t connect;							/*mqtt connect info*/
	system_request_callback system_callback;				/*topic handle pro*/
	swiot_wait_reply_t reply_list[TOPIC_REPLY_LIST_LEN];	/*reply list*/
	unsigned int gloab_message_id;									/*id for all message*/
	int ready;
}swiot_device_handle_t,*swiot_device_handle_pt;

/**
* @brief 使用单例模式,一是因为mqtt回调函数无法传达句柄参数，只能通过全局方式调用句柄,另外对于单个设备
*		 来说只会创建一个mqtt连接，所以单例也能满足需求
*/
swiot_device_handle_t* g_device_handle;		


int swiot_handle_init( void );
int swiot_handle_destroy( void );
int swiot_networt_connect( Network* n,
						   const char* host,
						   int port,
						   const char* pub_key );
int swiot_subscribe_default( MQTTClient* mqtt,
							 int subscribe_type );
swiot_wait_reply_pt swiot_add_wait_reply( int message_id );
int swiot_recieve_wait_reply( const char* topic_filter,
							  const char* payload );
int swiot_reset_wait_reply( swiot_wait_reply_pt wait_reply_pt );
int swiot_getnext_message_id();
int swiot_publish_sync_wait(char* topic_filter,
					   char* payload,
					   int message_id );
int swiot_close();
int swiot_connect();
int swiot_common_publish_packet(char* buf,
								int buf_size,
								int* message_id,
								const char* params,
								const char* method);
int swiot_common_publish_reply_packet(char* buf,
							  int buf_size,
							  int message_id,
							  int code,
							  const char* data);
int swiot_common_topic_reply_packet(char* buf,
							  int buf_size,
							  const char* productkey,
							  const char* devicename,
							  swiot_interact_type_e topic_type,
							  const char* service_id);
int swiot_parser_topic(char* topic_filter,
					   char* productkey,
					   char* devicename,
					   char** topic_id);
int swiot_parser_request_paload(char* payload,
								int* message_id,
								char* params,
								int params_len);
int swiot_receive_service_proc(char* topic_id,
					   char* productkey,
					   char* devicename,
					   char* payload);
int swiot_receive_control_proc(char* topic_id,
					   char* productkey,
					   char* devicename,
					   char* payload);


static void swiot_message_arrived(MessageData* data)
{
	//char test[2048] = {0};
	 
#if 1
	char productkey[64] = {0};
	char devicename[64] = {0};
	char topic[80] = {0};
	char* topic_id = NULL;
	int size = 0;

	if (g_device_handle == NULL||g_device_handle->ready==0)
	{
		log_error("mqtt not ready!\n");
		return;
	}
	log_info("Message arrived: %s topic_id:%s \n", data->message->payload,data->topicName->lenstring.data);
	/*pre handle*/
	size = (sizeof(topic) - 1) > (data->topicName->lenstring.len)?(data->topicName->lenstring.len):(sizeof(topic) - 1);
	memcpy( topic,data->topicName->lenstring.data,size );
	size = data->message->payloadlen;
	*((char*)data->message->payload + size) = 0;

   

    if (0 == swiot_recieve_wait_reply(topic,data->message->payload)) {
    	log_warn("get reply topic = %s,payload = %s\n",topic,data->message->payload);
    	return;
    }

    if (0 != swiot_parser_topic(topic,productkey,devicename,&topic_id)) {
    	log_error("parser topic error , topic_filter = %s\n",topic);
    	return;
    }
    //log_error("11111111\n")
    if (strstr(topic,"/thing/service/")) {
		if(strlen(topic_id) > strlen("_reply")
		 && strncmp(topic_id + strlen(topic_id) - strlen("_reply"), "_reply", strlen("_reply")) == 0){

		} else if (0 != swiot_receive_service_proc(topic_id,productkey,devicename,data->message->payload) ) {
    		log_error("not match server,topic_filter = %s\n",topic);
    		return;
    	}
    } else {
        //log_error("333333333333\n");
    	if (0 != swiot_receive_control_proc(topic_id,productkey,devicename,data->message->payload) ) {
    		log_error("not match control,topic_filter = %s\n",topic);
    		return;
    	}
    }
#endif
}

/**
* 	@brief 建立直连设备mqtt通道
*
* 	@param 建立连接所需的参数
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Construct( swiot_device_param_pt param )
{
    Network* network = NULL;
    int rc = 0;

    POINTER_SANITY_CHECK(param,0);
    POINTER_SANITY_CHECK(param->host,0);
    POINTER_SANITY_CHECK(param->client_id,0);
    POINTER_SANITY_CHECK(param->username,0);
    POINTER_SANITY_CHECK(param->password,0);
    POINTER_SANITY_CHECK(param->pread_buf,0);
    POINTER_SANITY_CHECK(param->pwrite_buf,0);

  log_info("start ....\n");

    network = (Network*)malloc(sizeof(Network));
    if (NULL == network) {
    	log_error("not enough memory\n");
    	return -1;
    }

  log_info("handle in ....\n");
    if (0 != swiot_handle_init()) {
    	log_error("Init device handle error\n");
    	free(network);
    	return -1;
    }
  log_info("handle out ....\n");
#if 1
    /*Init mqtt*/
    MQTTClientInit(g_device_handle->mqtt,
    			               network,
    	     				   param->command_timeout_ms,
    	                       param->pwrite_buf,
    	                       param->pwrite_bufsize,
    	                       param->pread_buf,
    	                       param->pread_bufsize);

    /*cp count*/
    strncpy(g_device_handle->count.client_id,param->client_id,sizeof(g_device_handle->count.client_id));
    strncpy(g_device_handle->count.username,param->username,sizeof(g_device_handle->count.username));
    strncpy(g_device_handle->count.password,param->password,sizeof(g_device_handle->count.password));

    /*cp connect info*/
    strncpy(g_device_handle->connect.host,param->host,sizeof(g_device_handle->connect.host));
    g_device_handle->connect.pub_key = param->pub_key;
    g_device_handle->connect.port = param->port; 

    if (0 != swiot_connect()) {
    	log_error("connect error");
    	goto FAILT_RETURN;
    }
   #endif

    log_info("goto fail\n");

	return 0;
FAILT_RETURN:
	swiot_handle_destroy();

	if (NULL != network) {
		free(network);
		network = NULL;
	}
	return -1;
}

/**
*   @brief 	销毁直连设备mqtt通道
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Destroy( void )
{
	int rc = 0;

	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt->ipstack,-1);

	/*unsubcribe*/
	if (0 != (rc = swiot_subscribe_default(g_device_handle->mqtt,TOPIC_UNSUBSCRIBE_TYPE))) {
		log_error("unsubcribe topic error , return code = %d\n",rc);
		return -1;
	}

	/*close connect*/
	if (0 != (rc = MQTTDisconnect(g_device_handle->mqtt))) {
		log_error("disconnect error , return code = %d\n",rc);
		return -1;
	}

	/*release memory*/
	swiot_handle_destroy();
	free(g_device_handle->mqtt->ipstack);
	return 0;
}

/**
*   @brief 	设备登陆
* 
*	@secret 登陆密钥
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Login( const char* secret )
{
	#define LOGIN_PARAMS "{\"deviceName\":\"%s\",\"productKey\":\"%s\"}"

	int rc = 0,message_id = 0;
	char topic_filter[64] = {0};
	char packet[256] = {0};
	char params[256] = {0};
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();
	
	POINTER_SANITY_CHECK(secret,-1);
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);

	/*topic*/
	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "login");

	
	snprintf(params,
			 sizeof(params),
			 LOGIN_PARAMS,
			 device_info_pt->devicename,
			 device_info_pt->productkey);

	/*packet*/
	if (0 != (rc = swiot_common_publish_packet(packet,sizeof(packet),&message_id,params,""))) {
		log_error("packet publish error\n");
		return -1;
	}

	/*publish*/
	if (REPLY_CODE_OK != (rc = swiot_publish_sync_wait(topic_filter,packet,message_id))) {
		log_error("login packet error , topic_filter = %s payload = %s\n",topic_filter,packet);
		return -1;
	}

	return 0;
}

/**
*	@brief 	设备登出
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Logout( void )
{
	#define LOGOUT_PARAMS "{\"deviceName\":\"%s\",\"productKey\":\"%s\"}"

	int rc = 0,message_id = 0;
	char topic_filter[64] = {0};
	char packet[256] = {0};
	char params[256] = {0};
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();
	
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);

	/*topic*/
	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "logout");

	snprintf(params,
		 sizeof(params),
		 LOGIN_PARAMS,
		 device_info_pt->devicename,
		 device_info_pt->productkey);

	/*packet*/
	if (0 != (rc = swiot_common_publish_packet(packet,sizeof(packet),&message_id,params,""))) {
		log_error("packet publish error\n");
		return -1;
	}

	/*publish*/
	if (REPLY_CODE_OK != (rc = swiot_publish_sync_wait(topic_filter,packet,message_id))) {
		log_error("logout error , topic_filter = %s payload = %s\n",topic_filter,packet);
		return -1;
	}

	return 0;
}

/**
*	@brief 		定时器
*
*	@timeout 	超时时间
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Yeild( int timeout )
{
	int rc = 0;
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
    
    //log_debug("Yeild in");
	rc = MQTTYield(g_device_handle->mqtt,timeout);
   //log_debug("Yeild rc =%d\n",rc);
	if( MQTTIsConnected(g_device_handle->mqtt) == 0 || g_device_handle->ready == 0 ) {
		log_info("reconnect \n");
		swiot_close();
		rc = swiot_connect();
	}
    //log_debug("yeid out,rc = %d\n",rc);
	return rc;
}

/**
* 	@brief 注册topic处理回调函数
*				平台下发的系统级topic将由此回调函数处理
* 
*   @callback 回调函数地址
*   
*   @return   0 成功,-1 失败
*/
int SWIOT_Device_System_Register( system_request_callback callback )
{
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(callback,-1);

	g_device_handle->system_callback = callback;

	return 0;
}

/**
*   @brief 回复系统请求
*
*   @type  			请求/回复类型
*   @message_id		消息id
*   @code           回复码
*   @paramstr       回复消息体中的param字段值
*   @exstr          扩展字段
*/
int SWIOT_Device_System_Response( swiot_interact_type_e type,
								  int message_id,
								  int code,
								  const char* paramstr,
								  const char* exstr)
{	
	int rc = 0;
	char topic_filter[64] = {0};
	char payload[64] = {0};
	MQTTMessage message = {0};
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();

	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(paramstr,-1);

	/*packet topic*/
	if (0 != swiot_common_topic_reply_packet(topic_filter,
											 sizeof(topic_filter),
											 device_info_pt->productkey,
											 device_info_pt->devicename,
											 type,
											 exstr)) {
		log_error("packet reply topic error\n");
		return -1;
	}

	/*packet payload*/
	if (0 != swiot_common_publish_reply_packet(payload,sizeof(payload),message_id,code,paramstr)) {
		log_error("packet publish payload error\n");
		return -1;
	}

	message.qos = QOS0;
	message.payload = payload;
	message.payloadlen = strlen(payload);

	/*publish*/
	log_debug("publish packet , topic_filter = %s payload=%s\n",topic_filter,payload);
	if (0 != (rc = MQTTPublish(g_device_handle->mqtt,topic_filter,&message))) {
		log_error("publish packet error , topic_filter = %s payload=%s\n",topic_filter,payload);
		return -1;
	}

	return 0;
}

/**
*  @brief 属性上报
*
*  @property        负载属性信息的json体
*  @return          0 成功,-1 失败
*/
int SWIOT_Device_Property_Post( const char* property )
{
	int message_id = 0;
	char topic_filter[64] = {0};
	char payload[64] = {0};
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();

	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(property,-1);

	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "event",
			 "property",
			 "post");

	/*packet*/
	if (0 != swiot_common_publish_packet(payload,sizeof(payload),&message_id,property,"thing.event.property.post")) {
		log_error("packet publish error\n");
		return -1;
	}

	/*publish*/
	if (REPLY_CODE_OK != swiot_publish_sync_wait(topic_filter,payload,message_id)) {
		log_error("property post error , topic_filter = %s payload = %s\n",topic_filter,payload);
		return -1;
	}

	return 0;
}

/**
*  @brief 事件上报
* 
*  @event          负载事件信息的json体
*  @event_id       事件id
*
*  @return         0 成功,-1 失败
*/
int SWIOT_Device_Event_Post( const char* event,
							 const char* event_id )
{	
	int ret = -1;
	int message_id = 0;
	char method[64] = {0};
	char topic_filter[64] = {0};
	char* payload = NULL;
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();

	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(event,-1);
	POINTER_SANITY_CHECK(event_id,-1);

	payload = (char*)malloc(2048);
	if (NULL == payload) {
		log_error("malloc error\n");
		goto END;
	}
	
	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "event",
			 event_id,
			 "post");

	snprintf(method,
		 sizeof(method),
		 "thing.event.%s.post",
		 event_id);
	/*packet*/
	if (0 != swiot_common_publish_packet(payload,2048,&message_id,event,method)) {
		log_error("packet publish error\n");
		goto END;
	}

	/*publish*/
	if (REPLY_CODE_OK != swiot_publish_sync_wait(topic_filter,payload,message_id)) {
		log_error("event post error , topic_filter = %s payload = %s\n",topic_filter,payload);
		goto END;
	}

	ret = 0;
END:
	if (payload) {
		free(payload);
	}
	return ret;
}


/**
*  @brief 设备信息上报
*
*  @version        	 设备版本号
*  @mac            	 设备mac地址
*  @manufacturer  	 设备厂商
*  @hardware_version 硬件厂商
*
*  @return           0 成功，-1失败
*/
int SWIOT_Device_Check(const char* version,
        			   const char* mac,
        			   const char* manufacturer,
        			   const char* hardware_version)
{
	#define CHECK_PARAMS "{\"sn\":\"%s\",\"mac\":\"%s\",\"manufacturer\":\"%s\",\"hardwareVersion\":\"%s\",\"firmwareVersion\":\"%s\"}"
	int rc = -1;
	int message_id = 0;
	char topic_filter[64] = {0};
	char* packet  = NULL;
	char* payload = NULL;
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();

	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(version,-1);
	POINTER_SANITY_CHECK(mac,-1);
	POINTER_SANITY_CHECK(manufacturer,-1);
	POINTER_SANITY_CHECK(hardware_version,-1);

	payload = (char*)malloc(256);
	if (NULL == payload) {
		log_error("malloc error\n");
		goto END;
	}

	packet = (char*)malloc(256);
	if (NULL == packet) {
		log_error("malloc error");
		goto END;
	}

	/*topic*/
	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_EXT_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "ota",
			 "device",
			 "check");

	
	snprintf(payload,
			 256,
			 CHECK_PARAMS,
			 device_info_pt->devicename,
			 mac,
			 manufacturer,
			 hardware_version,
			 version);

	/*packet*/
	if (0 != (rc = swiot_common_publish_packet(packet,256,&message_id,payload,""))) {
		log_error("packet publish error\n");
		goto END;
	}

	/*publish*/
	if ((REPLY_CODE_OK != (rc = swiot_publish_sync_wait(topic_filter,packet,message_id))) && (REPLY_CHECK_SAME != rc)){
		log_error("check packet error , topic_filter = %s payload = %s,rc = %d\n",topic_filter,packet,rc);
		goto END;
	}

	rc = 0;
END:
	if (payload) {
		free(payload);
	}
	if (packet) {
		free(packet);
	}
	return rc;
}

/**
*  @brief 升级进度上报
* 
*  @firmware    	  当前升级版本的md5号
*  @curversion        当前升级的md5号
*  @step              升级进度
*  @desc              描述字段
*  @strategy          升级策略
*/
int SWIOT_Device_Upgrade_Progress( const char* firmware,
        						   const char* curversion,
        						   const char* step,
        						   const char* desc,
        						   const char* strategy)
{
	#define UPGRADE_PARAMS "{\"firmware\":\"%s\",\"curVersion\":\"%s\",\"step\":\"%s\",\"desc\":\"%s\",\"strategy\":\"%s\"}"

	int ret = -1,rc = -1,message_id = -1;
	char topic_filter[64] = {0};
	char* payload = NULL;
	char* packet = NULL;
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();

	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(firmware,-1);
	POINTER_SANITY_CHECK(curversion,-1);
	POINTER_SANITY_CHECK(step,-1);
	POINTER_SANITY_CHECK(desc,-1);
	POINTER_SANITY_CHECK(strategy,-1);

	payload = (char*)malloc(256);
	if (NULL == payload) {
		log_error("malloc error\n");
		goto END;
	}

	packet = (char*)malloc(256);
	if (NULL == packet) {
		log_error("malloc error\n");
		goto END;
	}

	/*topic*/
	snprintf(topic_filter,
			 sizeof(topic_filter),
			 TOPIC_COMMON_EXT_EXT_EXT_FMT,
			 device_info_pt->productkey,
			 device_info_pt->devicename,
			 "ota",
			 "device",
			 "progress");

	
	snprintf(payload,
			 256,
			 UPGRADE_PARAMS,
			 firmware,
			 curversion,
			 step,
			 desc,
			 strategy);

	/*packet*/
	if (0 != (rc = swiot_common_publish_packet(packet,256,&message_id,payload,""))) {
		log_error("packet publish error\n");
		goto END;
	}

	/*publish*/
	if (REPLY_CODE_OK != (rc = swiot_publish_sync_wait(topic_filter,packet,message_id))) {
		log_error("login packet error , topic_filter = %s payload = %s\n",topic_filter,packet);
		goto END;
	}

	ret = 0;
END:
	if (payload) {
		free(payload);
	}
	if (packet) {
		free(packet);
	}
	return ret;
}

int swiot_handle_init( void )
{
	int i = 0;
	swiot_wait_reply_t* reply_pt = NULL;

	#if 1
	/*check*/
	//POINTER_SANITY_CHECK(g_device_handle,-1);

	g_device_handle = (swiot_device_handle_t*)malloc(sizeof(swiot_device_handle_t));
    if (NULL == g_device_handle) {
    	log_error("not enough memory\n");
    	return -1;
    }

    memset(g_device_handle,0,sizeof(swiot_device_handle_t));

    g_device_handle->mqtt = (MQTTClient*)malloc(sizeof(MQTTClient));
    if (NULL == g_device_handle->mqtt) {
    	log_error("not enough memory\n");
    	free(g_device_handle);
    	return -1;
    }

    for (i = 0;i < TOPIC_REPLY_LIST_LEN;i++) {
    	reply_pt = g_device_handle->reply_list + i;

    	/*Init cell*/
    	swiot_reset_wait_reply(reply_pt);
    }
#endif

	return 0;
}

int swiot_handle_destroy( void )
{
	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);

	swiot_close();
	if (NULL != g_device_handle->mqtt) {
		free(g_device_handle->mqtt);
		g_device_handle->mqtt = NULL;
	}
	free(g_device_handle);
	g_device_handle = NULL;
	return 0;

}

int swiot_networt_connect( Network* n,
						   const char* host,
						   int port,
						   const char* pub_key ) 
{	
	ssl_ca_crt_key_t ssl_cck;
	int  rc = 0;

    /*connect*/
    if (NULL != pub_key) {
    	log_debug("Use ssl connect\n");

    	/*Init ssl*/
    	SSL_CA_CERT_KEY_INIT(&ssl_cck,(char*)pub_key,strlen(pub_key),0,0,0,0);
    	NetworkInitSSL(n);

    	if ((rc = NetworkConnectSSL(n,(char*)host,port,&ssl_cck,TLSv1_1_client_method(),SSL_VERIFY_NONE, 8192)) !=  
    	1) {
    		log_error("SSL connect error , return code = %d\n",rc);
    		return -1;
    	}
    } else {
    	log_debug("Use tcp connect\n");

    	/*Init tcp*/
    	NetworkInit(n);

    	if ((rc = NetworkConnect(n,(char*)host,port)) != 0) {
    		log_error("TCP connect error , return code = %d\n",rc);
    		return -1;
    	}
    	log_error("socket:%d\n",n->my_socket);
    }

    return 0;
}

int swiot_subscribe_default( MQTTClient* mqtt,
							 int subscribe_type )
{	
	int i = 0 , rc = 0;
	MQTTSubackData data={0};
	swiot_device_info_pt device_info_pt = SWIOT_Setup_GetDevInfo();
	static swiot_topic_fmt_t subscribe_topics[] = 
	{
		{"",TOPIC_COMMON_FMT,"service","+"},
		{"",TOPIC_COMMON_EXT_FMT, "event", "+", "post_reply"},
        {"",TOPIC_COMMON_EXT_FMT, "event", "property", "post_reply"},
        {"",TOPIC_COMMON_EXT_FMT, "service", "property", "set"},
        {"",TOPIC_COMMON_EXT_FMT, "service", "property", "get"},
       // {"",TOPIC_COMMON_EXT_EXT_FMT,"disable"},
       // {"",TOPIC_COMMON_EXT_EXT_FMT,"enable"},
       // {"",TOPIC_COMMON_EXT_EXT_FMT,"delete"},
        {"",TOPIC_COMMON_EXT_EXT_EXT_FMT,"ota","device","check_reply"},
        {"",TOPIC_COMMON_EXT_EXT_EXT_FMT,"ota","device","progress_reply"},
        {"",TOPIC_COMMON_EXT_EXT_EXT_FMT,"ota","device","upgrade"},
        {"",TOPIC_COMMON_EXT_EXT_EXT_FMT,"ota","device","validate"}
	};

	/*subscribe*/
	for (i = 0;i < ARR_LEN(subscribe_topics);i++) {
		/*packet topic*/
		snprintf(subscribe_topics[i].topic,
				 sizeof(subscribe_topics[i].topic),
				 subscribe_topics[i].fmt,
				 device_info_pt->productkey,
				 device_info_pt->devicename,
				 subscribe_topics[i].agr1,
 				 subscribe_topics[i].agr2,
				 subscribe_topics[i].agr3);

		if (TOPIC_SUBSCRIBE_TYPE == subscribe_type) {
			if ((SUCCESS != (rc=MQTTSubscribeWithResults(mqtt,subscribe_topics[i].topic,QOS1,swiot_message_arrived,&data))) || 
				(0x80 == data.grantedQoS )){
				log_error("subscribe topic = %s error , return code = %d,rc=%d\n",subscribe_topics[i].topic,data.grantedQoS,rc);
				return -1;
			}
			log_debug("subscribe topic = %s ,return code = %02x \n",subscribe_topics[i].topic,data.grantedQoS & 0xff);

		} else {
			log_debug("unsubscribe topic = %s\n",subscribe_topics[i].topic);
			if (SUCCESS != (rc=MQTTUnsubscribe(mqtt,subscribe_topics[i].topic))) {
				log_error("unsubscribe topic = %s error , return code = %d\n",subscribe_topics[i].topic,rc);
				return -1;
			}
		}
	}

	return 0;
}

swiot_wait_reply_pt swiot_add_wait_reply( int message_id )
{
	swiot_wait_reply_t* reply_cell_pt = NULL;
	swiot_wait_reply_t* reply_list;
	int i = 0;

	POINTER_SANITY_CHECK(g_device_handle,0);
	ZERO_SANITY_CHECK(message_id,0);

	reply_list = g_device_handle->reply_list;
	for (i = 0;i < TOPIC_REPLY_LIST_LEN;i++) {
		if (REPLY_CODE_INIT == reply_list[i].reply_code) {
			log_info("add wait reply list success , index = %d\n",i);
			reply_cell_pt = reply_list + i;

			reply_cell_pt->id = message_id;
			reply_cell_pt->reply_code = REPLY_CODE_WAIT;
			break;
		}
	}
	return reply_cell_pt;
}

int swiot_recieve_wait_reply( const char* topic_filter,
							  const char* payload )
{
	int i = 0,message_id = 0,code = 0;
	char* str = NULL;
	swiot_wait_reply_t* reply_list;

	cJSON* josn_obj = NULL;
	cJSON* value_obj = NULL;
	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(topic_filter,-1);
	POINTER_SANITY_CHECK(payload,-1);

	josn_obj = cJSON_Parse(payload);
	if (NULL == josn_obj) {
		log_error("parse josn error , json str = %s\n",payload);
		return -1;
	}

	/*get id*/
	value_obj = cJSON_GetObjectItem(josn_obj,"id");
	if (NULL == value_obj) {
		log_error("get \"id\" error , josn str = %s\n",payload);
		cJSON_Delete(josn_obj);
		return -1;
	}

	/*parser id*/
	CJSON_GETINT(value_obj,message_id);
	ZERO_SANITY_CHECK(message_id,-1);

	/*find reply code*/
	reply_list = g_device_handle->reply_list;
	for (i = 0;i < TOPIC_REPLY_LIST_LEN;i++) {
		if (message_id == reply_list[i].id) {
			/*get code*/
			value_obj = cJSON_GetObjectItem(josn_obj,"code");
			if (NULL == value_obj) {
				log_error("get \"code\" error , josn str = %s\n",payload);
				cJSON_Delete(josn_obj);
				return -1;
			}

			CJSON_GETINT(value_obj,code);

			/*get data*/
			value_obj = cJSON_GetObjectItem(josn_obj,"data");
			if (NULL == value_obj) {
				log_error("the \"data\" is NULL,json str = %s\n",payload);
			}

			CJSON_GETSTRING(value_obj,str);
			log_info("get reply return code = %d,data = %s, topic_filter = %s\n",code,str,topic_filter);

			reply_list[i].reply_code = code;
			strncpy(reply_list[i].data,str,sizeof(reply_list[i].data));
			cJSON_Delete(josn_obj);
			return 0;
		}
	}

	cJSON_Delete(josn_obj);
	return -1;
}

int swiot_reset_wait_reply( swiot_wait_reply_pt wait_reply_pt )
{
	POINTER_SANITY_CHECK(wait_reply_pt,-1);

	wait_reply_pt->id = 0;
	wait_reply_pt->reply_code = REPLY_CODE_INIT;
	memset(wait_reply_pt->data,0,sizeof(wait_reply_pt->data));
	return 0;
}

int swiot_getnext_message_id()
{
	/*check*/
	POINTER_SANITY_CHECK(g_device_handle,-1);

	return ++(g_device_handle->gloab_message_id);
}

int swiot_publish_sync_wait(char* topic_filter,
					   char* payload,
					   int message_id )
{
	int rc = -1,count = 0;
	MQTTMessage message = {0};
	swiot_wait_reply_pt wait_reply_pt = NULL;

	/*check*/
	POINTER_SANITY_CHECK(topic_filter,-1);
	POINTER_SANITY_CHECK(payload,-1);

	message.qos = QOS0;
	message.payload = payload;
	message.payloadlen = strlen(payload);

	/*publish*/
	log_info("publish packet , topic_filter = %s payload=%s\n",topic_filter,payload);
	if (0 != (rc = MQTTPublish(g_device_handle->mqtt,topic_filter,&message))) {
		log_error("publish packet error , topic_filter = %s payload=%s\n",topic_filter,payload);
		return -1;
	}

	/*wait for ack*/
	if ( NULL == (wait_reply_pt = swiot_add_wait_reply(message_id))) {
		log_error("add wait reply error , message_id = %d\n",message_id);
		return -1;
	}

	while (count++ < MAX_WAIT_REPLY_TIMES) {
		if (REPLY_CODE_WAIT != wait_reply_pt->reply_code){
			log_info("publish success ,code = %d\n",wait_reply_pt->reply_code);
			rc = wait_reply_pt->reply_code;
			swiot_reset_wait_reply(wait_reply_pt);
			return rc;
		}
		SWIOT_Device_Yeild( 1000 );
	}

	log_error("wait ack time out ,topic_filter = %s\n",topic_filter);
	return -1;
}

int swiot_close() 
{
	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt->ipstack,-1);

	if (g_device_handle->mqtt->ipstack->disconnect) {
		log_debug("destroy:%d\n",g_device_handle->mqtt->ipstack->my_socket);
		g_device_handle->mqtt->ipstack->disconnect(g_device_handle->mqtt->ipstack);
	}
	g_device_handle->ready=0;
	return 0;
}

int swiot_connect()
{
	int rc = 0;
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
	MQTTConnackData data;

	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt,-1);
	POINTER_SANITY_CHECK(g_device_handle->mqtt->ipstack,-1);

	if (0 != (rc = swiot_networt_connect(g_device_handle->mqtt->ipstack,
										 g_device_handle->connect.host,
										 g_device_handle->connect.port,
										 g_device_handle->connect.pub_key))) {
    	log_error("Connect error\n");
    	return -1;
    }

    connectData.clientID.cstring = (char*)g_device_handle->count.client_id;
	connectData.username.cstring = (char*)g_device_handle->count.username;
	connectData.password.cstring = (char*)g_device_handle->count.password;
	log_debug("client_id:%s username:%s password:%s\n",connectData.clientID.cstring,connectData.username.cstring,connectData.password.cstring);
    if (0 != (rc = MQTTConnectWithResults(g_device_handle->mqtt, &connectData,&data))) {
        log_error("Return code from MQTT connect is %d,connack:%d\n", rc,data.rc);
        return -1;
    } else {
        log_debug("MQTT Connected\n");
    }

    /*subscribe genery topic*/
    if (0 != (rc = swiot_subscribe_default(g_device_handle->mqtt,TOPIC_SUBSCRIBE_TYPE)) ) {
    	log_error("unsubscribe topic\n");
    	swiot_subscribe_default(g_device_handle->mqtt,TOPIC_UNSUBSCRIBE_TYPE);
    	return -1;
    }
    g_device_handle->ready=1;
	return 0;
}

int swiot_common_publish_packet(char* buf,
								int buf_size,
								int* message_id,
								const char* params,
								const char* method)
{
	#define COMMON_PUBLISH_PACKET "{\"id\":%d,\"version\":\"1.0\",\"params\":%s,\"method\":\"%s\"}"

	POINTER_SANITY_CHECK(buf,-1);
	POINTER_SANITY_CHECK(params,-1);
	POINTER_SANITY_CHECK(method,-1);
	POINTER_SANITY_CHECK(message_id,-1);

	*message_id = swiot_getnext_message_id();
	snprintf(buf,
		  	 buf_size, 
		  	 COMMON_PUBLISH_PACKET,
			 *message_id,
			 params,
			 method);
	return 0;
}
int swiot_common_publish_reply_packet(char* buf,
							  int buf_size,
							  int message_id,
							  int code,
							  const char* data)
{
	#define COMMON_REPLY_PACKET "{\"id\":%d,\"code\":%d,\"data\":%s}"

	POINTER_SANITY_CHECK(buf,-1);
	POINTER_SANITY_CHECK(data,-1);

	snprintf(buf,
		  	 buf_size, 
		  	 COMMON_REPLY_PACKET,
			 message_id,
			 code,
			 data);
	return 0;
}
int swiot_common_topic_reply_packet(char* buf,
							  int buf_size,
							  const char* productkey,
							  const char* devicename,
							  swiot_interact_type_e topic_type,
							  const char* service_id)
{
	POINTER_SANITY_CHECK(productkey,-1);
	POINTER_SANITY_CHECK(devicename,-1);
	POINTER_SANITY_CHECK(buf,-1);

	if (SWIOT_SERVICE_SET == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_FMT,
			 productkey,
			 devicename,
			 "service",
			 "property",
			 "set_reply");
	} else if (SWIOT_SERVICE_GET == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_FMT,
			 productkey,
			 devicename,
			 "service",
			 "property",
			 "get_reply");
	} else if (SWIOT_SERVICE_COM == topic_type) {
		POINTER_SANITY_CHECK(service_id,-1);
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_FMT"/%s_reply",
			 productkey,
			 devicename,
			 "service",
			 service_id);
	} else if (SWIOT_CONTROL_DISABLE == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_FMT,
			 productkey,
			 devicename,
			 "disable_reply");
	} else if (SWIOT_CONTROL_ENABLE == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_FMT,
			 productkey,
			 devicename,
			 "enable_reply");
	} else if (SWIOT_CONTROL_DELETE == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_FMT,
			 productkey,
			 devicename,
			 "delete_reply");
	} else if (SWIOT_CONTROL_UPGRADE == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_EXT_FMT,
			 productkey,
			 devicename,
			 "ota",
			 "device",
			 "upgrade_reply");
	} else if (SWIOT_CONTROL_VALIDATE == topic_type) {
		snprintf(buf,
			 buf_size,
			 TOPIC_COMMON_EXT_EXT_EXT_FMT,
			 productkey,
			 devicename,
			 "ota",
			 "device",
			 "validate_reply");
	}else
		return -1;

	return 0;
}

int swiot_parser_topic(char* topic_filter,
					   char* productkey,
					   char* devicename,
					   char** topic_id)
{
	char* p_start = NULL,*p_end = NULL;
	POINTER_SANITY_CHECK(topic_filter,-1);
	POINTER_SANITY_CHECK(productkey,-1);
	POINTER_SANITY_CHECK(devicename,-1);
	POINTER_SANITY_CHECK(topic_id,-1);

	p_start = strstr(topic_filter,"/sys/");
	POINTER_SANITY_CHECK(p_start,-1);

	p_start += 5;
	p_end = p_start;

	/*parser productkey*/
	while (*p_end != '/' && p_end - topic_filter < strlen(topic_filter))
		p_end++;

	memcpy(productkey,p_start,p_end - p_start);
	productkey[p_end - p_start] = '\0';


	/*parser devicename*/
	p_start = ++p_end;
	while (*p_end != '/' && p_end - topic_filter < strlen(topic_filter))
		p_end++;

	memcpy(devicename,p_start,p_end - p_start);
	devicename[p_end - p_start] = '\0';

	*topic_id = p_end;
	return 0;
}

int swiot_parser_request_paload(char* payload,
								int* message_id,
								char* params,
								int params_len)
{
	cJSON* josn_obj = NULL;
	cJSON* value_obj = NULL;
	int id = 0;
	char* str = NULL;

	POINTER_SANITY_CHECK(payload,-1);
	POINTER_SANITY_CHECK(message_id,-1);
	POINTER_SANITY_CHECK(params,-1);

	josn_obj = cJSON_Parse(payload);
	if (NULL == josn_obj) {
		log_error("parse josn error , json str = %s\n",payload);
		return -1;
	}

	/*get id*/
	value_obj = cJSON_GetObjectItem(josn_obj,"id");
	if (NULL == value_obj) {
		log_error("get \"id\" error , josn str = %s\n",payload);
		cJSON_Delete(josn_obj);
		return -1;
	}


	CJSON_GETINT(value_obj,id);

	/*get params*/
	value_obj = cJSON_GetObjectItem(josn_obj,"params");
	if (NULL == value_obj) {
		log_warn("get \"params\" error , josn str = %s\n",payload);
		cJSON_Delete(josn_obj);
		return -1;
	}

	log_debug("itemname:%s valuestring:%d\n",value_obj->string,value_obj->type);
	str = cJSON_Print(value_obj);
	if (NULL == str) {
		log_warn("get params NULL\n");
		cJSON_Delete(josn_obj);
		return -1;
	}

	strncpy(params,str,params_len);
	*message_id = id;

	if(str)
		free(str);
	cJSON_Delete(josn_obj);
	return 0;
}

int swiot_receive_service_proc(char* topic_id,
					   char* productkey,
					   char* devicename,
					   char* payload)
{
	int ret = -1;
	int message_id = 0;
	//char* params = NULL;
	char params[2048];
	char* service_id = NULL;

	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->system_callback,-1);
	POINTER_SANITY_CHECK(topic_id,-1);
	POINTER_SANITY_CHECK(productkey,-1);
	POINTER_SANITY_CHECK(devicename,-1);
	POINTER_SANITY_CHECK(payload,-1);

	//params = (char*)malloc(2048);
    //log_error("aaaaaaaaaa\n");
	//if (NULL == params) {
	//	log_error("malloc error\n");
	//	goto END;
	//}
	//log_error("memsize333333333333333333333333333:%d\n",get_mem_mark());
    //log_error("bbbbbbbbbbb\n");

	if (0 != swiot_parser_request_paload(payload,&message_id,params,2048)) {
		log_warn("parser payload error , payload = %s\n",payload);
		goto END;
	}

    //log_error("cccccccccccc\n");
	if (strstr(topic_id,"/service/property/set")) {
        //log_error("dddddddddddddddd\n");
		g_device_handle->system_callback(SWIOT_SERVICE_SET,message_id,params,NULL);
        //log_error("eeeeeeeeeeeeeeee\n");
	} else if (strstr(topic_id,"/service/property/get")) {
        //log_error("fffffffffffffff\n");
		g_device_handle->system_callback(SWIOT_SERVICE_GET,message_id,params,NULL);
        //log_error("ggggggggggggggggg\n");
	} else if (strstr(topic_id,"/thing/service")){
    	service_id = strstr(topic_id, "/thing/service");
   		service_id = service_id + strlen("/thing/service/");
        //log_error("hhhhhhhhhhhhhhhhhh\n");
        #if 1
   		g_device_handle->system_callback(SWIOT_SERVICE_COM,message_id,params,service_id);
   		#endif
        //log_error("iiiiiiiiiiiiiiiiiiiiii\n");
	} else {
		goto END;
	}
    //log_error("jjjjjjjjjjjjjjj\n");
	ret = 0;
END:
    //log_error("kkkkkkkkkkkkkkkkkk\n");
	//if (params) {
	//	free(params);
	//}
	return ret;
}
int swiot_receive_control_proc(char* topic_id,
					   char* productkey,
					   char* devicename,
					   char* payload)
{
	int ret = -1;
	int message_id = 0;
	char* params = NULL;

	POINTER_SANITY_CHECK(g_device_handle,-1);
	POINTER_SANITY_CHECK(g_device_handle->system_callback,-1);
	POINTER_SANITY_CHECK(topic_id,-1);
	POINTER_SANITY_CHECK(productkey,-1);
	POINTER_SANITY_CHECK(devicename,-1);
	POINTER_SANITY_CHECK(payload,-1);

	params = (char*)malloc(2048);
	if (NULL == params) {
		log_error("malloc error\n");
		goto END;
	}

	if (0 != swiot_parser_request_paload(payload,&message_id,params,2048)) {
		log_error("parser payload error , payload = %s\n",payload);
		goto END;
	}

	if (strstr(topic_id,"/thing/disable")) {
		g_device_handle->system_callback(SWIOT_CONTROL_DISABLE,message_id,params,NULL);
	} else if (strstr(topic_id,"/thing/enable")) {
		g_device_handle->system_callback(SWIOT_CONTROL_ENABLE,message_id,params,NULL);
	} else if (strstr(topic_id,"/thing/delete")) {
		g_device_handle->system_callback(SWIOT_CONTROL_DELETE,message_id,params,NULL);
	} else if (strstr(topic_id,"/ota/device/upgrade")) {
		g_device_handle->system_callback(SWIOT_CONTROL_UPGRADE,message_id,params,NULL);
	} else if (strstr(topic_id,"/ota/device/validate")) {
		g_device_handle->system_callback(SWIOT_CONTROL_VALIDATE,message_id,params,NULL);
	} else
		goto END;

	ret = 0;
END:	
	if (params) {
		free(params);
	}
	return ret;
}
