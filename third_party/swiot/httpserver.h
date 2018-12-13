/**
* @date : 2018/9/27 14:21
*/
#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

/**
 * [swiot_packet_deal_func  http包回调函数]
 * @param  code 	[返回码]
 * @param  uri  	[路径]
 * @param  palyload [负载]
 * @param  len      [负载长度]
 */
typedef void (*swiot_packet_deal_func)( void* client,char* method,char* uri,char* palyload,int head_len,int size,int session_id,void* param );

/**
 * [sw_http_server_init http服务初始化]
 * @param  ip   [ip地址]
 * @param  port [端口]
 * @return      [成功返回句柄]
 */
int swiot_http_server_init( char* ip,short port,swiot_packet_deal_func func,void* param );


/**
 * [swiot_http_server_respond 发送回复]
 * @param  handle    [句柄]
 * @param  code      [回复码]
 * @param  data      [数据]
 * @param  data_size [数据大小]
 * @return           [成功返回0]
 */
int swiot_http_server_response( void* client,int session_id,int code,char* data,int data_size );


/**
* [swiot_http_server_ontimer 定时器]
* @param now [当前时间]
*/
int swiot_http_server_ontimer(int now);

/**
 * [swiot_http_server_destroy 销毁http服务]
 * @param  handle [句柄]
 * @return        [成功返回0]
 */
int swiot_http_server_destroy();


#endif  //__HTTPSERVER_H__