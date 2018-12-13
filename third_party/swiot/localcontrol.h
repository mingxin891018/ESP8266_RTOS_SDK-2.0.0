/**
* date : 2018/9/28 18:09
*/
#ifndef __LOCALCONTROL_H__
#define __LOCALCONTROL_H__


typedef enum
{
	SEND_IRCODE_CMD,
	MATCH_SWITCH_CMD,
	CHECK_UPGRADE_CMD,
	HEART_BEAT_CMD,
	UNKOWN_CMD,
}lc_controy_e;

/**
* @命令回调函数
* @cmd_type [命令类型]
* @cmd_id   [命令序号]
* @cmd      [命令内容]
* @param    [回调参数]
*/
typedef int (*swiot_lccmd_callback)(lc_controy_e cmd_type,void* cmd_handle,int cmd_id,char* cmd,void* param);

/**
* @本地控制初始化
*/
int SWIOT_LC_Construct(char* sn);

/**
* @销毁本地控制模块
*/
int SWIOT_LC_Destroy();


/**
* @注册命令回调函数
* @param cmd_handle [命令回调函数]
* @param param      [回调参数]
*/
int SWIOT_LC_Register(swiot_lccmd_callback cmd_handle,void* param);

/**
* @命令回复
* @param cmd_id [命令ID]
* @param code   [回复码]
*/
int SWIOT_LC_Response(void* cmd_handle,int cmd_id,int code,char* data,int data_size);


#endif //__LOCALCONTROL_H__