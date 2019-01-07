/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_dev_register.h
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-19
*   Describe: null
*
*===============================================================*/
#ifndef __SW_DEV_REGISTER_H__
#define __SW_DEV_REGISTER_H__

//mqtt msg
typedef enum
{   
	MSG_GET_REPLY = 1,      //回复服务器get指令
	MSG_SET_REPLY,          //回复服务器set指令
	MSG_POST,               //上报状态
	MSG_UPGRADE_CHECK,      //升级检查
	MSG_UPGRADE_PROGRESS,   //升级进度
}MQTT_CMD;

//mqtt队列使用的数据结构
typedef struct
{
	MQTT_CMD msg_type;
	int sn;
}msg_st;

//服务器查询状态 dev<--server
#define SUB_TOPIC_GET                   "/sys/%s/%s/thing/service/property/get"
//dev-->server  
#define PUB_TOPIC_GET_REPLY             "/sys/%s/%s/thing/service/property/get_reply"

//服务器设置 dev<--server
#define SUB_TOPIC_SET                   "/sys/%s/%s/thing/service/property/set"
//dev-->server  
#define PUB_TOPIC_SET_REPLY             "/sys/%s/%s/thing/service/property/set_reply"

//设备状态上报 dev-->server
#define PUB_TOPIC_POST                  "/sys/%s/%s/thing/event/property/post"

//dev<--server  
#define SUB_TOPIC_POST_REPLY            "/sys/%s/%s/thing/event/property/post_reply"

#define PUB_TOPIC_LOGIN                 "/sys/%s/%s/thing/login"

#define PUB_TOPIC_UPGRADE_CHECK         "/sys/%s/%s/ota/device/check"
#define SUB_TOPIC_UPGRADE_CHECK_REPLY   "/sys/%s/%s/ota/device/check_reply"

#define SUB_TOPIC_UPGRADE               "/sys/%s/%s/ota/device/upgrade"
#define PUB_TOPIC_UPGRADE_PROGRESS      "/sys/%s/%s/ota/device/progress"

//服务器-->dev
#define SUB_TOPIC_VALIDATE              "/sys/%s/%s/ota/device/validate"



#endif //__SW_DEV_REGISTER_H__
