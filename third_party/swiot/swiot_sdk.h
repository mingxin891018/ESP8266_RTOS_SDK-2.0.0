#ifndef __SWIOT_SDK_H__
#define __SWIOT_SDK_H__

#include "esp_common.h"
#include "spiffs/spiffs.h"


/**
 * 事件类型
 * EVENT_NETWORK_CONFIG             配网结果
 *  param                           NULL
 * EVENT_GET_CONN_INFO              获取连接信息结果
 *  param                           NULL
 * EVENT_CONNECT                    连接 mqtt 结果
 *  param                           NULL
 * EVENT_LED_SWITCH                 切换小夜灯
 *  param                           NULL
 * EVENT_SEND_IRCODE                发送红外码
 *  param                           IRCode
 *  收到该事件，红外发送接收到的红外码，发送成功返回true，否则返回false
 * EVENT_MATCH_SWITCH               一键匹配
 *  param                           NULL:关闭
 *                                  (void*)1:打开
 *  收到该事件，需要开启学习模式，学习键值通过SWIOT_SDK_Post_Event上报 event_type 为 EVENT_TYPE_MATCH_RESULT
 * EVENT_CHECK_UGPRADE              检查升级
 *  param                           NULL
 *  收到该事件，检查完成之后，需要调用SWIOT_SDK_Post_Event 上报检查结果 event_type 为 EVENT_TYPE_CHECK_UPGRADE_RESULT
 * EVENT_UPGRADE                    启动升级
 *  param                           NULL
 *  收到该事件，升级成功之后需要调用SWIOT_SDK_Post_Event上报升级结果 event_type 为EVENT_TYPE_UPGRADE_PROGRESS
 * 
 **/
enum
{
    EVENT_NETWORK_CONFIG,
    EVENT_DEVICE_FIND,
    EVENT_CONNECT,
    EVENT_LED_SWITCH,
    EVENT_SEND_IRCODE,
    EVENT_MATCH_SWITCH,
    EVENT_CHECK_UGPRADE,
    EVENT_UPGRADE
};

/**
 * Post 事件类型
 * EVENT_TYPE_MATCH_RESULT              一键匹配结果
 * EVENT_TYPE_CHECK_UPGRADE_RESULT      检查升级结果
 * EVENT_TYPE_UPGRADE_PROGRESS          升级进度
 **/
enum
{
    EVENT_TYPE_MATCH_RESULT,
    EVENT_TYPE_CHECK_UPGRADE_RESULT,
    EVENT_TYPE_UPGRADE_PROGRESS
};

/**
 * 事件状态
 * STATUS_NONE                      无状态
 * STATUS_DOING                     正在做
 * STATUS_SUCCESSED                 成功
 * STATUS_FAILED                    失败              
 **/
enum
{
    STATUS_NONE,
    STATUS_DOING,
    STATUS_SUCCESSED,
    STATUS_FAILED
};

/**
 * event_t
 * 事件数据结构
 **/ 
typedef struct
{
    int what;
    int status;
    void *param;

    int isrespon;   //是否需要回复
    swiot_interact_type_e type;
    int message_id;
    char* service_id;
} event_t;

/**
 * 回调函数,接收到app端指令之后回调给应用层
 **/

typedef bool (*SWIOT_User_Handle)(event_t *event);

/**
 * cache  读写接口
 **/
typedef int (*SWIOT_Cache_Read)(void *buf, int read_length);
typedef int (*SWIOT_Cache_Write)(void *buf, int write_length);

/**
 * init 方法
 * snid             设备唯一标识
 **/
void SWIOT_SDK_Init(char *snid);

/**
 * Set_Cache 方法
 * read             SWIOT_Cache_Read cache 读接口
 * write            SWIOT_Cache_Write cache 写接口
 * ps:至少需要512字节,二进制读写,reset 需要清除
 **/
void SWIOT_SDK_SetCache(SWIOT_Cache_Read read, SWIOT_Cache_Write write);

/**
 * Set SWIOT_User_Handle 方法
 * user_handle      user 处理事件函数
 **/
void SWIOT_SDK_SetUserHandle(SWIOT_User_Handle user_handle);

/**
 * Start 方法
 **/
void SWIOT_SDK_Start();

/**
 * 发送 事件
 * event_type           事件类型
 *                      取值为 EVENT_TYPE_MATCH_RESULT, EVENT_TYPE_CHECK_UPGRADE_RESULT, EVENT_TYPE_UPGRADE_PROGRESS
 * val                  事件内容
 *                      EVENT_TYPE_MATCH_RESULT  ==> char *
 *                      EVE NT_TYPE_CHECK_UPGRADE_RESULT ==> char *
 *                      EVENT_TYPE_UPGRADE_PROGRESS ==> int  [-1, 100] 100 表示升级成功   -1 表示升级失败
 * ret                  0 发送成功
 *                      -1 发送失败
 **/
int SWIOT_SDK_Post_Event(int event_type, void *val);

/**
* 升级进度上报
* @param        progress                进度上报
* @return                               SWIOT_SUCCESS :成功
*                                       SWIOT_EGENERIC:失败
*/
int SWIOT_SDK_PostUpgress(int progress,const char* firmware,const char* curversion,const char* strategy);

/**
 * Stop 方法
 **/
void SWIOT_SDK_Stop();

/**
 * Destory 方法
 **/
void SWIOT_SDK_Destory();

#endif