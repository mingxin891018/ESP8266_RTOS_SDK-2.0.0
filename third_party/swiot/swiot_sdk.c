#include "swiot_config.h"
#include "swiot_common.h"
#include "swiot_sdk.h"
#include "swiot_setup.h"
#include "swiot_device.h"
#include "swiot_upgrade.h"
#include "swiot_devicefind.h"
#include "localcontrol.h"

#include "cJSON.h"

#define SWIOT_EVNET_LIST_SIZE       8

#define  SWIOT_ACCOUNT_NAME_LENGTH      32
#define  SWIOT_ACCOUNT_KEY_LENGTH       36
#define  SWIOT_ACCOUNT_BROKER_LENGTH    36

#define SWIOT_SERVICE_ID_LENGTH         32
#define SWIOT_RESPONSE_SIZE             8
#define SWIOT_POST_EVENT_SIZE           8

#define SWIOT_MQTT_MESSAGE_LENGTH       (1024)


#define SERVICE_LEDSWITCH           "ledSwitch"
#define SERVICE_SENDIRCODE          "sendIRCode"
#define SERVICE_MATCHSWITCH         "matchSwitch"
#define SERVICE_CHECKUPGRADE        "checkUpgrade"
#define SERVICE_UPGRADE             "upgrade"
#define EVENT_MATCHRESULT           "matchResult"
#define EVENT_CHECKUPGRADERESULT    "checkUpgradeResult"
#define EVENT_UPGRADERESULT         "upgradeResult"
#define PARAM_SWITCH                "switch"
#define PARAM_IRCODE                "IRCode"
#define PARAM_RESULT                "result"

typedef struct
{
    swiot_interact_type_e type;
    int message_id;
    int code;
    char *service_id;
    bool used;
}swiot_response_t;

typedef struct 
{
    char *event_id;
    char *param;
    bool used;
} swiot_post_event_t;

typedef struct
{
    char name[SWIOT_ACCOUNT_NAME_LENGTH];
	char key[SWIOT_ACCOUNT_KEY_LENGTH];
	char broker[SWIOT_ACCOUNT_BROKER_LENGTH];
} swiot_account_t;


#define SWIOT_DEVICE_SNID_LENGTH        32

typedef struct
{
    char sn[SWIOT_DEVICE_SNID_LENGTH];
} swiot_device_t;

typedef struct
{   
    swiot_account_t account;
    bool smart_configed;
    bool device_finded;
} swiot_cache_t;

typedef struct
{
    swiot_cache_t               cache;
    swiot_device_t              device;
    os_timer_t                  device_find_timer;
    void*                       event_list;
    uint8_t                     working;
    uint8_t                     *read_buffer;
    uint8_t                     *write_buffer;
    SWIOT_Cache_Read            cache_read;
    SWIOT_Cache_Write           cache_write;
    SWIOT_User_Handle           user_handle;
    swiot_post_event_t          post_events[SWIOT_RESPONSE_SIZE];
    swiot_response_t            responses[SWIOT_POST_EVENT_SIZE];
    void*                       mutex;
} swiot_t;

LOCAL swiot_t g_swiot;

int swiot_fota_upgrade(char* params);
int parser_upgrade_params( char* params,char *upgrade_url,char* md5,char* curversion,char* strategy);
//bool send_event(int what, int status, void *param,int isrespon,swiot_interact_type_e type,int message_id,char* service_id);
bool send_event(int what, int status, void *param);


int swiot_lccmd_handle(lc_controy_e cmd_type,void* cmd_handle,int cmd_id,char* cmd,void* param)
{
    bool ret = 0;
    cJSON *root = NULL;
    cJSON *item = NULL;
    swiot_response_t *response = NULL;
    char* str = NULL;

    switch(cmd_type) {
        case SEND_IRCODE_CMD:
                log_info("params:%s\n",cmd);
                root = cJSON_Parse(cmd);
                if(!root)
                {
                    log_error("cmd error !\n");
                    SWIOT_LC_Response(cmd_handle,cmd_id,404,NULL,0);
                    break;
                } 
                item = cJSON_GetObjectItem(root, PARAM_IRCODE);
                if(!item)
                {
                    log_error("paramstr error !\n");
                    SWIOT_LC_Response(cmd_handle,cmd_id,404,NULL,0);
                    break;
                }
                str = cJSON_Print(item);
                if(NULL == str)
                {
                    log_error("get paramstr error!\n");
                    SWIOT_LC_Response(cmd_handle,cmd_id,404,NULL,0);
                    break;
                }

               ret = send_event(EVENT_SEND_IRCODE, STATUS_NONE, str);
                
                if (ret)
                    SWIOT_LC_Response(cmd_handle,cmd_id,200,NULL,0);
                else
                    SWIOT_LC_Response(cmd_handle,cmd_id,500,NULL,0);

                if (str) {
                    free(str);
                }
                if(root)
                {
                    cJSON_Delete(root);
                }
        
            break;
        default:
            SWIOT_LC_Response(cmd_handle,cmd_id,404,NULL,0);
            break;
    }
    return 0;
}

int sw_found_device_set_status( int status )
{
    int ret = 500;
    int tick = SWIOT_Common_Get_Ms();
    int now = tick;

    if( device_get_status() != 0 )
    {
        device_find_set_core_status( (core_status_em)status );
        log_debug("in ,tick:%d,now:%d\n",tick,now);
        while((tick + 5000 > now) && (ret > 0)){
            now = SWIOT_Common_Get_Ms();
            ret = device_find_ontime();
            if( ret > 0 )
                vTaskDelay(ret);
        }
         log_debug("out,tick:%d,now:%d,ret:%d\n",tick,now,ret);
    }
    ret = 0;
    return ret;
}

int read_cache(void *buf, int len)
{
    int ret = -1;
    log_debug("read_cache in\n");
    if(g_swiot.cache_read)
    {
        ret = g_swiot.cache_read(buf, len);
        if(ret < 0)
        {
            log_error("read_cache read failed!\n");
        }
    } 
    log_debug("read_cache out ret = %d\n", ret);
    return ret;
}

int write_cache(void *buf, int len)
{
    int ret = -1;
    log_debug("write_cache in\n");
    if(g_swiot.cache_write)
    {
        ret = g_swiot.cache_write(buf, len);
        if(ret < 0)
        {
            log_error("write_cache write failed!\n");
        }
    } 
    log_debug("write_cache out ret = %d\n", ret);
    return ret;
}

bool send_event(int what, int status, void *param)
{
    event_t event;
    bool ret = 0;
    log_debug("send_event in what = %d status = %d param = 0x%08x\n", what, status, param);
    if(g_swiot.user_handle != NULL)
    {
        bzero(&event, sizeof(event_t));
        event.what = what;
        event.status = status;
        event.param = param;
        log_debug("send_event send in\n");
        ret = (*(g_swiot.user_handle))(&event);
        log_debug("send_event send out\n");
    }
    log_debug("send_event out ret = %d\n", ret);
}

swiot_response_t *get_unused_reposonse()
{
    swiot_response_t * response = NULL;
    int i = 0;
    for(; i < SWIOT_RESPONSE_SIZE; ++i)
    {
        if(!g_swiot.responses[i].used)
        {
            response = &g_swiot.responses[i];
            response->used = 1;
            return response;
        }
    }
    return response;
}

swiot_post_event_t *get_unused_post_event()
{
    swiot_post_event_t * post_event = NULL;
    int i = 0;
    for(; i < SWIOT_POST_EVENT_SIZE; ++i)
    {
        if(!g_swiot.post_events[i].used)
        {
            post_event = &g_swiot.post_events[i];
            post_event->used = 1;
            return post_event;
        }
    }
    return post_event;
}

/**
 * init 方法
 * sn               设备唯一标识
 **/
void SWIOT_SDK_Init(char *sn)
{
    os_printf("SWIOT_SDK_Init in Version : %s\n", SWIOT_SDK_VERSION);
    bzero(&g_swiot, sizeof(swiot_t));
    snprintf(g_swiot.device.sn, sizeof(g_swiot.device.sn), "%s", sn);
    SWIOT_Common_Init();
    log_debug("SWIOT_SDK_Init sn = %s, device.sn = %s\n", sn, g_swiot.device.sn);
    log_debug("SWIOT_SDK_Init out\n");
}

/**
 * Set_Cache 方法
 * read             SWIOT_Cache_Read cache 读接口
 * write            SWIOT_Cache_Write cache 写接口
 * ps:至少需要512字节,二进制读写,reset 需要清除
 **/
void SWIOT_SDK_SetCache(SWIOT_Cache_Read read, SWIOT_Cache_Write write)
{
    swiot_cache_t *cache = NULL;
    int ret;
    log_debug("SWIOT_SDK_SetCache in\n");
    cache = &(g_swiot.cache);
    g_swiot.cache_read = read;
    g_swiot.cache_write = write;
    ret = read_cache(cache, sizeof(swiot_cache_t));
    if(ret >= 0)
    {
        log_debug("cache smart_configed = %d, device_finded = %d\n", cache->smart_configed, cache->device_finded);
        log_debug("cache name = %s, key = %s, broker = %s\n", cache->account.name, cache->account.key, cache->account.broker);
    }
    log_debug("SWIOT_SDK_SetCache out\n");
}

/**
 * Set SWIOT_User_Handle 方法
 * handle           SDK 句柄
 * user_handle      user 处理事件函数
 **/
void SWIOT_SDK_SetUserHandle(SWIOT_User_Handle user_handle)
{
    log_debug("SWIOT_SDK_SetUserHandle in user_handle = 0x%08x\n", user_handle);
    g_swiot.user_handle = user_handle;
    log_debug("SWIOT_SDK_SetUserHandle out\n");
}

void system_request(swiot_interact_type_e type, int message_id, char* paramstr, char* exstr)
{
    bool ret = 0;
    cJSON *root = NULL;
    cJSON *item = NULL;
    swiot_response_t *response = NULL;
    char* str = NULL;
    log_debug("system_request in type = %d message_id = %d paramstr = %s exstr = 0x%08x\n", type, message_id, paramstr, exstr);
    if(exstr)
    {
        log_debug("exstr = %s\n", exstr);
    }
    switch(type)
    {
        case SWIOT_SERVICE_SET:
            break;
        case SWIOT_SERVICE_GET:
            break;
        case SWIOT_SERVICE_COM:
            if(strcmp(exstr, SERVICE_LEDSWITCH) == 0)
            {   
                ret = send_event(EVENT_LED_SWITCH, STATUS_NONE, NULL);
                response = get_unused_reposonse();
                if(response)
                {
                    response->type = type;
                    response->message_id = message_id;
                    response->service_id = SERVICE_LEDSWITCH;
                    if(ret)
                    {
                        response->code = 200;
                    } else 
                    {
                        response->code = 400;
                    }
                    response->used = 1;
                } 
            } else if(strcmp(exstr, SERVICE_SENDIRCODE) == 0)
            { 

                root = cJSON_Parse(paramstr);
                if(!root)
                {
                    log_error("paramstr error !\n");
                    break;
                } 
                item = cJSON_GetObjectItem(root, PARAM_IRCODE);
                if(!item)
                {
                    log_error("paramstr error !\n");
                    break;
                }
                str = cJSON_Print(item);
                if(NULL == str)
                {
                    log_error("get paramstr error!\n");
                    break;
                }
//                log_error("qqqqqqqqqqqq");
                ret = send_event(EVENT_SEND_IRCODE, STATUS_NONE, str);
                free(str);
 //               log_error("wwwwwwwwwwwwwwww");
                response = get_unused_reposonse();
  //              log_error("eeeeeeeeeeeeeee");
                if(response)
                {
  //                  log_error("rrrrrrrrrrrrrrrrr");
                    response->type = type;
                    response->message_id = message_id;
                    response->service_id = SERVICE_SENDIRCODE;
                    if(ret)
                    {
                        response->code = 200;
                    } else 
                    {
                        response->code = 400;
                    }
                    response->used = 1;
                } 
     //           log_error("rooooooooot:%x\n",root);
                if(root)
                {
                    cJSON_Delete(root);
                }
            } else if(strcmp(exstr, SERVICE_MATCHSWITCH) == 0)
            {
                root = cJSON_Parse(paramstr);
                if(!root)
                {
                    log_error("paramstr error !\n");
                    break;
                } 
                item = cJSON_GetObjectItem(root, PARAM_SWITCH);
                if(!item)
                {
                    log_error("paramstr error !\n");
                    break;
                }
                str = cJSON_Print(item);
                if(NULL == str)
                {
                    log_error("get paramstr error!\n");
                    break;
                }
                ret = send_event(EVENT_MATCH_SWITCH, STATUS_NONE, (void*)str);
                free(str);
                response = get_unused_reposonse();
                if(response)
                {
                    response->type = type;
                    response->message_id = message_id;
                    response->service_id = SERVICE_MATCHSWITCH;
                    if(ret)
                    {
                        response->code = 200;
                    } else 
                    {
                        response->code = 400;
                    }
                    response->used = 1;
                } 
                if(root)
                {
                    cJSON_Delete(root);
                }
            } else if(strcmp(exstr, SERVICE_CHECKUPGRADE) == 0)
            {
                ret = send_event(EVENT_CHECK_UGPRADE, STATUS_NONE, NULL);
                response = get_unused_reposonse();
                if(response)
                {
                    response->type = type;
                    response->message_id = message_id;
                    response->service_id = SERVICE_CHECKUPGRADE;
                    if(ret)
                    {
                        response->code = 200;
                    } else 
                    {
                        response->code = 400;
                    }
                    response->used = 1;
                } 
            } 
            break;
        case SWIOT_CONTROL_VALIDATE:
        case SWIOT_CONTROL_UPGRADE:
            {
                //ret = send_event(EVENT_UPGRADE, STATUS_NONE, NULL);
                ret = swiot_fota_upgrade(paramstr);
                //log_debug("swiot_fota_upgrade 2222222222222222222\n");
                response = get_unused_reposonse();
                //log_debug("swiot_fota_upgrade 33333333333333333333\n");
                if(response)
                {
                     //log_debug("swiot_fota_upgrade 444444444444444444444444444\n");
                    response->type = type;
                    response->message_id = message_id;
                    response->service_id = SERVICE_UPGRADE;
                    if(ret)
                    {
                        response->code = 200;
                    } else 
                    {
                        response->code = 400;
                    }
                    response->used = 1;
                }  
            } 
            break;
        case SWIOT_CONTROL_DISABLE:
            break;
        case SWIOT_CONTROL_ENABLE:
            break;
        case SWIOT_CONTROL_DELETE:
            break;
        default:
            break;
    }
    log_debug("system_request out\n");
}


void ICACHE_FLASH_ATTR work_task(void *pvParameters)
{
    log_debug("work_task in\n");
    int yeildret = 0;
    int i = 0;
    swiot_response_t * response = NULL;
    swiot_post_event_t *post_event = NULL;
    while(g_swiot.working)
    {
       //log_error("memsize1111111111111111111:%d\n",get_mem_mark());
        yeildret = SWIOT_Device_Yeild(500);
        for(i = 0; i < SWIOT_RESPONSE_SIZE; ++i)
        {
            response = &g_swiot.responses[i];
            //log_error("uuuuuuuuuuuuuuu7uu\n");
            if(response->used)
            {
                //log_error("uuuuuuuuuu6uuuuuuu\n");
                SWIOT_Device_System_Response(response->type, response->message_id, response->code, "{}", response->service_id);
                //log_error("uuuuuuuuu5uuuuuuuu\n");
                response->used = 0;
            }
        }
        //log_error("uuuuuuuuuuuuuuuu4u\n");
        for(i = 0; i < SWIOT_POST_EVENT_SIZE; ++i)
        {
            post_event = &g_swiot.post_events[i];
            if(post_event->used)
            {
                if(post_event->param)
                {
                   // log_error("uuuuuuuu3uuuuuuuuu\n");
                    SWIOT_Device_Event_Post(post_event->param, post_event->event_id);
                   // log_error("uuuuuuu2uuuuuuuuuu\n");
                            free(post_event->param);
                    post_event->param = NULL;
                }
                post_event->used = 0;
            }
        }
        //log_error("uuuuuuuuuuuuuuuuu1\n");
        //log_error("memsize2222222222222222222222:%d\n",get_mem_mark());
        vTaskDelay(1000 / portTICK_RATE_MS); 
    }
    vTaskDelete(NULL);
    log_debug("work_task out\n");
}

void ICACHE_FLASH_ATTR connect_task(void *pvParameters)
{
    int ret = 0;
    swiot_conn_info_pt device_info;
    swiot_device_param_t device_param;

    log_debug("connect_task in\n"); 
    g_swiot.read_buffer = os_malloc(SWIOT_MQTT_MESSAGE_LENGTH);
    if(g_swiot.read_buffer == NULL)
    {
        log_error("malloc read_buffer failed\n");
        return;
    }
    bzero(g_swiot.read_buffer, SWIOT_MQTT_MESSAGE_LENGTH);
    
    g_swiot.write_buffer = os_malloc(SWIOT_MQTT_MESSAGE_LENGTH);
    if(g_swiot.write_buffer == NULL)
    {
        log_error("malloc write_buffer failed\n");
        return;
    }
    bzero(g_swiot.write_buffer, SWIOT_MQTT_MESSAGE_LENGTH);
    log_debug("connect_task SWIOT_Setup_SetDevInfo\n");
    ret = SWIOT_Setup_SetDevInfo(DEVICE_PRODUCT_KEY, g_swiot.device.sn, SWIOT_SDK_VERSION);
    
    log_debug("connect_task SWIOT_Setup_SetConnInfo\n");
    SWIOT_Setup_SetConnInfo(g_swiot.cache.account.name, g_swiot.cache.account.key, g_swiot.cache.account.broker);
    
    log_debug("connect_task SWIOT_Setup_GetConnInfo\n");
    device_info = SWIOT_Setup_GetConnInfo();
    bzero(&device_param, sizeof(swiot_device_param_t));
    
    device_param.port = device_info->port;
    device_param.host = device_info->host;
    device_param.client_id = device_info->client_id;
    device_param.username = device_info->username;
    device_param.password = device_info->password;
    device_param.pub_key = device_info->pub_key;

    device_param.command_timeout_ms = 2000;
    device_param.pread_buf = g_swiot.read_buffer;
    device_param.pread_bufsize = SWIOT_MQTT_MESSAGE_LENGTH;
    device_param.pwrite_buf = g_swiot.write_buffer;
    device_param.pwrite_bufsize = SWIOT_MQTT_MESSAGE_LENGTH;
    
    log_debug("connect_task SWIOT_Device_Construct\n");
    ret = SWIOT_Device_Construct(&device_param);
    if(ret != 0)
    {
        log_error("SWIOT_Device_Construct failed!\n");
        send_event(EVENT_CONNECT, STATUS_FAILED, NULL);
        goto ERROR;
    }
    send_event(EVENT_CONNECT, STATUS_SUCCESSED, NULL);
    
    log_debug("connect_task SWIOT_Device_System_Register\n");
    ret = SWIOT_Device_System_Register(system_request);
    if(ret != 0)
    {
        log_error("SWIOT_Device_System_Register failed!\n");
        SWIOT_Device_Destroy();
        goto ERROR;
    }

    if (0 != SWIOT_Device_Check(SWIOT_SDK_VERSION,SWIOT_MAC,SWIOT_MANUFACTURER,SWIOT_HARD_VERSION)) {
        log_error("Check device error\n");
        goto ERROR;
    }

   // send_event(EVENT_CONNECT, STATUS_SUCCESSED, NULL,0,0,0,0);
    send_event(EVENT_CONNECT, STATUS_SUCCESSED, NULL);
    xTaskCreate(work_task, "work_task", 4096, NULL, 2, NULL);

    goto RETURN;
ERROR:
    sw_found_device_set_status( -1 );
    log_info("find in\n");
    device_find_deinit();
    log_info("find out\n");
    //if( g_swiot.read_buffer != NULL )
   // {
	//	os_free( g_swiot.read_buffer );
    //    g_swiot.read_buffer = NULL;
    //}
	//if( g_swiot.write_buffer != NULL )
   // {
	//	os_free( g_swiot.write_buffer );
   //     g_swiot.write_buffer = NULL;
    //}
   
    g_swiot.cache.device_finded = 0;
//    send_event(EVENT_CONNECT, STATUS_FAILED, NULL,0,0,0,0);
    send_event(EVENT_CONNECT, STATUS_FAILED, NULL);
    vTaskDelete(NULL);
    log_error("connect_task error\n");
    return;
RETURN:
    sw_found_device_set_status(1);
    device_find_deinit();
    vTaskDelete(NULL);
   // log_debug("connect_task out\n");
    return;
}


void connect_info(char* connect_info)
{
    cJSON *item = NULL;
    cJSON *root = cJSON_Parse(connect_info);
    log_debug("connect_info in connect_info = %s\n", connect_info);
    if(root)
    {
        if(!g_swiot.cache.device_finded)
        {
            item = cJSON_GetObjectItem(root, "username");
            if(item != NULL)
            {
                snprintf(g_swiot.cache.account.name, sizeof(g_swiot.cache.account.name), "%s", item->valuestring);
            }
            item = cJSON_GetObjectItem(root, "tcpEndpoint");
            if(item != NULL)
            {
                snprintf(g_swiot.cache.account.broker, sizeof(g_swiot.cache.account.broker), "%s", item->valuestring);
            }
            item = cJSON_GetObjectItem(root, "secret");
            if(item != NULL)
            {
                snprintf(g_swiot.cache.account.key, sizeof(g_swiot.cache.account.key), "%s", item->valuestring);
            }
            g_swiot.cache.device_finded = 1;
            log_debug("connect_info name = %s, key = %s, broker = %s\n", g_swiot.cache.account.name, g_swiot.cache.account.key, g_swiot.cache.account.broker);
            write_cache(&(g_swiot.cache), sizeof(g_swiot.cache));
            send_event(EVENT_DEVICE_FIND, STATUS_SUCCESSED, NULL);
            // os_timer_disarm(&(g_swiot.device_find_timer));
            xTaskCreate(connect_task, "connect_task", 1024, NULL, 2, NULL);
        }
        cJSON_Delete(root);
    } 
    else 
    {
        send_event(EVENT_DEVICE_FIND, STATUS_FAILED, NULL);
    }
    log_debug("connect_info out\n");
}

void ICACHE_FLASH_ATTR devicefind_task(void *pvParameters)
{
    int ret = 0;
    log_debug("devicefind_task in\n");
    if(!g_swiot.cache.device_finded)
    {
        ret = device_find_init( DEVICE_FIND_PORT, DEVICE_PRODUCT_KEY, g_swiot.device.sn, connect_info );
        if( ret != 0 )
        {
            log_error("device_find_init error\n");
            send_event(EVENT_DEVICE_FIND, STATUS_FAILED, NULL);
        }
        else 
        {
            log_debug("device_finding \n");
            send_event(EVENT_DEVICE_FIND, STATUS_DOING, NULL);
        }
    } 
    else 
    {
        xTaskCreate(connect_task, "connect_task", 1024, NULL, 2, NULL);
    }
    vTaskDelete(NULL);
    log_debug("devicefind_task out\n");
}


void ICACHE_FLASH_ATTR smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            log_debug("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            log_debug("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            log_debug("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                log_debug("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                log_debug("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            log_debug("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
	
	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            log_debug("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                log_debug("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
                
                g_swiot.cache.smart_configed = 1;
                write_cache(&(g_swiot.cache), sizeof(g_swiot.cache));
                send_event(EVENT_NETWORK_CONFIG, STATUS_SUCCESSED, NULL);
                xTaskCreate(devicefind_task, "devicefind_task", 1024, NULL, 2, NULL);
                
               if (0 != SWIOT_LC_Construct(g_swiot.device.sn)) {
                    log_error("init lc construct error\n");
                    SWIOT_LC_Destroy();
                }

                SWIOT_LC_Register(swiot_lccmd_handle,NULL);
            } else {
                send_event(EVENT_NETWORK_CONFIG, STATUS_FAILED, NULL);
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				//airkiss_start_discover();
			}
            smartconfig_stop();
            break;
    }
}

void ICACHE_FLASH_ATTR smartconfig_task(void *pvParameters)
{
    log_debug("smartconfig_task in\n");
    send_event(EVENT_NETWORK_CONFIG, STATUS_DOING, NULL);
    smartconfig_start(smartconfig_done);
    
    vTaskDelete(NULL);
    log_debug("smartconfig_task out\n");
}

/**
 * Start 方法
 **/
void SWIOT_SDK_Start()
{
    swiot_cache_t *cache = NULL;
    log_debug("SWIOT_SDK_Start in\n");
    g_swiot.working = 1;
    cache = &(g_swiot.cache);
    log_debug("SWIOT_SDK_Start smart_configed = %d\n", cache->smart_configed);
    if(!cache->smart_configed)
    {
        wifi_set_opmode(STATION_MODE);
        xTaskCreate(smartconfig_task, "smartconfig_task", 1024, NULL, 2, NULL);
    } 
    else 
    {
        xTaskCreate(devicefind_task, "devicefind_task", 1024, NULL, 2, NULL);

        if (0 != SWIOT_LC_Construct(g_swiot.device.sn)) {
            log_error("init lc construct error\n");
            SWIOT_LC_Destroy();
        }

        SWIOT_LC_Register(swiot_lccmd_handle,NULL);
    }
    log_debug("SWIOT_SDK_Start out\n");
}


/**
 * 发送 事件
 * event_type           事件类型
 *                      取值为 EVENT_TYPE_MATCH_RESULT, EVENT_TYPE_CHECK_UPGRADE_RESULT, EVENT_TYPE_UPGRADE_PROGRESS
 * val                  事件内容
 *                      EVENT_TYPE_MATCH_RESULT  ==> char *
 *                      EVENT_TYPE_CHECK_UPGRADE_RESULT ==> char *
 *                      EVENT_TYPE_UPGRADE_PROGRESS ==> int  [-1, 100] 100 表示升级成功   -1 表示升级失败
 * ret                  0 发送成功
 *                      -1 发送失败
 **/
int SWIOT_SDK_Post_Event(int event_type, void *val)
{
    int ret = -1;
    switch(event_type)
    {
        case EVENT_TYPE_MATCH_RESULT:
            ret = SWIOT_SDK_Post_MatchResultEvent((char*)val);
            break;
        case EVENT_TYPE_CHECK_UPGRADE_RESULT:
            ret = SWIOT_SDK_Post_CheckUpgradeResultEvent((char *)val);
            break;
        case EVENT_TYPE_UPGRADE_PROGRESS:
            ret = SWIOT_SDK_Post_UpgradeResultEvent((int)val);
            break;
        default:
            break;
    }
    return ret;
}

/**
* 升级进度上报
* @param        progress                进度上报
* @return                               SWIOT_SUCCESS :成功
*                                       SWIOT_EGENERIC:失败
*/
int SWIOT_SDK_PostUpgress(int progress,const char* firmware,const char* curversion,const char* strategy)
{
    int ret = SWIOT_EGENERIC;
    char str_progress[8];

    POINTER_SANITY_CHECK(firmware,SWIOT_EGENERIC);
    POINTER_SANITY_CHECK(curversion,SWIOT_EGENERIC);
    POINTER_SANITY_CHECK(strategy,SWIOT_EGENERIC);

    snprintf(str_progress,sizeof(str_progress),"%d",progress);

    ret = SWIOT_Device_Upgrade_Progress(firmware,curversion,str_progress,"",strategy);

    return ret;
}


/**
 * 发送一键匹配 Event
 * ret              0 发送成功
 *                  -1 发送失败
 **/
int SWIOT_SDK_Post_MatchResultEvent(char *val)
{
    bool ret = -1;
    char *param = NULL;
    int length = 0;
    swiot_post_event_t *post_event = NULL;
    if(val)
    {
        length = strlen(val);
    }
    if(length)
    {
        length += 16;
        param = (char *)malloc(length + 16);
        if(param)
        {
            bzero(param, length);
            snprintf(param, length, "{\"%s\":\"%s\"}", PARAM_RESULT,  val);
            post_event = get_unused_post_event();
            log_debug("SWIOT_SDK_Post_MatchResultEvent post_event = 0x%08x\n", post_event);
            if(post_event)
            {
                post_event->event_id = EVENT_MATCHRESULT;
                post_event->param = param;
                post_event->used = 1;
                ret = 0;
            }
            else 
            {
                ret = -1;
            }
        }
    }
    return ret;
}

/**
 * 发送检测升级结果 Event
 * val              界面展示字符
 * ret              0 发送成功
 *                  -1 发送失败
 **/
int SWIOT_SDK_Post_CheckUpgradeResultEvent(char *val)
{
    bool ret = -1;
    char *param = NULL;
    int length = 0;
    swiot_post_event_t *post_event = NULL;
    if(val)
    {
        length = strlen(val);
    }
    if(length)
    {
        length += 16;
        param = (char *)malloc(length + 16);
        if(param)
        {
            bzero(param, length);
            snprintf(param, length, "{\"%s\":\"%s\"}", PARAM_RESULT,  val);
            post_event = get_unused_post_event();
            log_debug("SWIOT_SDK_Post_CheckUpgradeResultEvent post_event = 0x%08x\n", post_event);
            if(post_event)
            {
                post_event->event_id = EVENT_MATCHRESULT;
                post_event->param = param;
                post_event->used = 1;
                ret = 0;
            }
            else 
            {
                ret = -1;
            }
        }
    }
    return ret;
}

/**
 * 发送升级结果 Event
 * val              界面展示字符
 * ret              0 发送成功
 *                  -1 发送失败
 **/
int SWIOT_SDK_Post_UpgradeResultEvent(int val)
{
    bool ret = -1;
    char *param = NULL;
    int length = 0;
    swiot_post_event_t *post_event = NULL;

    length = 10;
    length += 16;
    param = (char *)malloc(length + 16);
    if(param)
    {
        bzero(param, length);
        snprintf(param, length, "{\"%s\":\"%d\"}", PARAM_RESULT,  val);
        post_event = get_unused_post_event();
        log_debug("SWIOT_SDK_Post_UpgradeResultEvent post_event = 0x%08x\n", post_event);
        if(post_event)
        {
            post_event->event_id = EVENT_MATCHRESULT;
            post_event->param = param;
            post_event->used = 1;
            ret = 0;
        }
        else 
        {
            ret = -1;
        }
    }
    return ret;
}

/**
 * Stop 方法
 **/
void SWIOT_SDK_Stop()
{
    log_debug("SWIOT_SDK_Stop in\n");
    g_swiot.working = 0;
    log_debug("SWIOT_SDK_Stop out\n");
}

/**
 * Destory 方法
 * handle           SDK 句柄
 **/
void SWIOT_SDK_Destory(void *handle)
{
    log_debug("SWIOT_SDK_Destory in\n");
    SWIOT_Device_Destroy();
    if( g_swiot.read_buffer != NULL )
    {
		os_free( g_swiot.read_buffer );
        g_swiot.read_buffer = NULL;
    }
	if( g_swiot.write_buffer != NULL )
    {
		os_free( g_swiot.write_buffer );
        g_swiot.write_buffer = NULL;
    }
    
    SWIOT_LC_Destroy();

    SWIOT_Common_Destroy();
    log_debug("SWIOT_SDK_Destory out\n");
}

int swiot_fota_upgrade(char* params)
{
    char url[1024] = {0};
    char md5[64] = {0};
    char curversion[32] = {0};
    char strategy[32] = {0};
    if (0 != parser_upgrade_params(params,url,md5,curversion,strategy)) {
        log_error("params upgrade params error! params = %s\n",params);
        return 0; 
    }

    if ( 0 != sw_upgrade(url,md5,curversion,strategy)) {
        log_error("upgrade error,url=%s md5=%s,curversion=%s\n",url,md5,curversion);
        return 0;
    }
    //log_debug("swiot_fota_upgrade 11111111111111111111111111\n");
    return 1;
}
int parser_upgrade_params( char* params,char *upgrade_url,char* md5,char* curversion,char* strategy)
{
    int ret = -1;

    cJSON* parse_obj = NULL;
    cJSON* value_obj = NULL;

    STRING_PTR_SANITY_CHECK(params, SWIOT_EGENERIC);

    parse_obj = cJSON_Parse(params);
    if (NULL == parse_obj) {
        log_error("parse json error , params = %s\n",params);
        goto END;
    }

    value_obj = cJSON_GetObjectItem(parse_obj,"url");
    if (NULL == value_obj || cJSON_String != value_obj->type) {
        log_error("parser \"url\" error , params = %s\n",params);
        goto END;
    }

    if (upgrade_url) {
        strncpy(upgrade_url,value_obj->valuestring,strlen(value_obj->valuestring) + 1);
    }

    value_obj = cJSON_GetObjectItem(parse_obj,"md5");
    if (NULL == value_obj || cJSON_String != value_obj->type) {
        log_error("parser \"md5\" error, params = %s\n",params);
        goto END;
    }

    if (md5) {
        strncpy(md5,value_obj->valuestring,strlen(value_obj->valuestring) + 1);
    }

    value_obj = cJSON_GetObjectItem(parse_obj,"firmwareVersion");
    if (NULL == value_obj || cJSON_String != value_obj->type) {
        log_error("parser \"firmwareVersion\" error, params = %s\n",params);
       goto END;
    }

    if (curversion) {
        strncpy(curversion,value_obj->valuestring,strlen(value_obj->valuestring) + 1);
    }

    value_obj = cJSON_GetObjectItem(parse_obj,"strategy");
    if (NULL == value_obj || cJSON_String != value_obj->type) {
        log_info("parser \"strategy\" error, params = %s\n",params);
 //       goto END;
    } else if (strategy) {
        strncpy(strategy,value_obj->valuestring,strlen(value_obj->valuestring) + 1);
    }
    
    ret = 0;
END:
    if (parse_obj)
        cJSON_Delete(parse_obj);
    return ret;
}