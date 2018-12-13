/**
* @date 	: 2018/6/20 17:14
* @author	: zhoutengbo
*/

#ifndef __SWIOT_SETUP_H__
#define __SWIOT_SETUP_H__

#define SWIOT_SETUP_PRODUCTKEY_LEN 32
#define SWIOT_SETUP_DEVICENAME_LEN 32
#define SWIOT_SETUP_VERSION_LEN    8

#define SWIOT_SETUP_HOST_LEN	   32
#define SWIOT_SETUP_CLIENT_LEN     64
#define SWIOT_SETUP_USERNAME_LEN   32
#define SWIOT_SETUP_PASSWORD_LEN   32

/**
* @brief 设备相关信息
*/
typedef struct 
{
	char productkey[SWIOT_SETUP_PRODUCTKEY_LEN];			/*产品标志符*/
	char devicename[SWIOT_SETUP_DEVICENAME_LEN];				/*设备序列号*/
	char version[SWIOT_SETUP_VERSION_LEN];					/*设备版本号*/
}swiot_device_info_t,*swiot_device_info_pt;

/**
* @brief 设备连接信息
*/
typedef struct 
{
	/*服务器端口*/	
	int   port;	
	/*服务器地址*/
	char host[SWIOT_SETUP_HOST_LEN];
	/*客户端id*/
	char client_id[SWIOT_SETUP_CLIENT_LEN];
	/*用户名*/
	char username[SWIOT_SETUP_USERNAME_LEN];
	/*密码*/
	char password[SWIOT_SETUP_PASSWORD_LEN];

	/**
	*如果此字段为空，则表示普通的tcp连接，否则为ssl连接
	*/
	const char* pub_key;
}swiot_conn_info_t,*swiot_conn_info_pt;

/**
* @brief 设置设备productkey与devicename
*
* @return 0 成功,-1 失败
*/
int SWIOT_Setup_SetDevInfo( const char* productkey,
							const char* devicename,
							const char* version );

/**
* @brief 获取设备信息
*/
swiot_device_info_pt SWIOT_Setup_GetDevInfo();

/**
* @brief 设置设备连接信息
*
* @name  	用户名
* @key   	密码
* @broker	连接地址
*
* @return   NULL 失败
*/
int SWIOT_Setup_SetConnInfo( const char* name,
							 const char* key,
							 const char* broker );

/**
* @brief 获取设备连接信息
*
* @return mqtt连接信息
*/
swiot_conn_info_pt SWIOT_Setup_GetConnInfo();



int swiot_setconn_info( const char* name,
							 const char* key,
							 const char* broker );

#endif //__SWIOT_SETUP_H__

