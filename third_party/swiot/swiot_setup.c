#include <stdio.h>
#include <string.h>
#include "swiot_common.h"
#include "swiot_ca.h"

swiot_device_info_t g_device_info;		/*全局设备信息*/
swiot_conn_info_t   g_conn_info;		/*全局连接信息*/

/**
* @brief 设置设备productkey与devicename
*
* @return 0 成功,-1 失败
*/
int SWIOT_Setup_SetDevInfo( const char* productkey,
							const char* devicename,
							const char* version )
{
	/*check*/
	STRING_PTR_SANITY_CHECK( productkey,-1 );
	STRING_PTR_SANITY_CHECK( devicename,-1 );
	STRING_PTR_SANITY_CHECK( version,-1 );

	strncpy( g_device_info.productkey,productkey,sizeof( g_device_info.productkey ) - 1 );
	strncpy( g_device_info.devicename,devicename,sizeof( g_device_info.devicename ) - 1 );
	strncpy( g_device_info.version,version,sizeof( g_device_info.version ) - 1 );

	return 0;
}

/**
* @brief 获取设备信息
*/
swiot_device_info_pt SWIOT_Setup_GetDevInfo()
{
	return &g_device_info;
}

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
							 const char* broker )
{
	/*check*/
	STRING_PTR_SANITY_CHECK(name, 0);
    STRING_PTR_SANITY_CHECK(key, 0);
    STRING_PTR_SANITY_CHECK(broker, 0);

	return swiot_setconn_info( name,key,broker );
}

/**
* @brief 获取设备连接信息
*
* @return mqtt连接信息
*/
swiot_conn_info_pt SWIOT_Setup_GetConnInfo()
{
	return &g_conn_info;
}


int swiot_setconn_info( const char* name,
							 const char* key,
							 const char* broker )
{
    log_debug("start to set conn info!broker:%s", broker);
    if(broker[0]=='t'&&broker[1]=='c'&&broker[2]=='p')
        g_conn_info.pub_key = NULL;
    else if(broker[0]=='s'&&broker[1]=='s'&&broker[2]=='l')
        g_conn_info.pub_key = swiot_ca_get(); 
    else
        return -1;


    memset(&g_conn_info, 0x0, sizeof(g_conn_info));
    strncpy(g_conn_info.username, name, SWIOT_SETUP_USERNAME_LEN);
    strncpy(g_conn_info.password, key, SWIOT_SETUP_PASSWORD_LEN);
    strncpy(g_conn_info.client_id, name, SWIOT_SETUP_CLIENT_LEN);

    size_t head = 0;
    size_t end = 0;
    size_t pos = 0;
    for (pos = 0; '\0' != broker[pos]; ++pos)
    {
        if (':' == broker[pos])
        {
            end = head == 0 ? end : pos;
            /* For head, should skip the substring "//". */
            head = head == 0 ? pos + 3 : head;
        }
    }
    if (head == 0 || end <= head)
    {
        log_error("Failure: cannot get the endpoint from broker address.");
        return -1;
    }

    size_t length = end - head + 1;
    for (pos = 0; pos < length - 1; ++pos)
    {
        g_conn_info.host[pos] = broker[head + pos];
    }
    g_conn_info.host[length-1]='\0';
    g_conn_info.port = atoi(&broker[end+1]);
    log_debug("host_name = %s, port = %d\n", g_conn_info.host, g_conn_info.port);
    log_debug("device_info set successfully!");

    return 0;
}
