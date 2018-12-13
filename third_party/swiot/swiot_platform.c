#include <stdio.h>
#include <sockets.h>
#include "swiot_platform.h"
#include "freertos/semphr.h"



void* SWIOT_Mutex_Create()
{
	xSemaphoreHandle sem = xSemaphoreCreateMutex();
	return (void*)sem;
}
void SWIOT_Mutex_Destroy( void* mutex )
{
	xSemaphoreHandle sem = (xSemaphoreHandle)mutex;
	vSemaphoreDelete(sem);
}
void SWIOT_Mutex_Lock( void* mutex )
{
	xSemaphoreHandle sem = (xSemaphoreHandle)mutex;
	xSemaphoreTake(sem, portMAX_DELAY);
}
void SWIOT_Mutex_Unlock( void* mutex )
{
	xSemaphoreHandle sem = (xSemaphoreHandle)mutex;
	xSemaphoreGive(sem);
}

/**
 * [创建套接字]
 * @param  domain   [协议族，常用协议族有AF_INET（要使用ipv4地址）、AF_INET6、AF_LOCAL ]
 * @param  type     [指定Socket类型,常用的SOCKET类型有SOCK_STREAM、SOCK_DGRAM、SOCK_RAW、SOCK_PACKET、SOCK_SEQPACKET]
 * @param  protocol [指定协议，常用协议有IPPROTO_TCP,IPPROTO_UDP,IPPROTO_STCP,IPPROTO_TIPC，与type不可随意组合]
 * @return          [成功返回套接字，否则 < 0]
 */
int SWIOT_Creat_Socket(int domain,int type,int protocol)
{
	return lwip_socket(domain,type,protocol);
}

/**
 * [销毁套接字]
 * @param  skt [套接字]
 * @return     [错误 < 0]
 */
int SWIOT_Close_Socket(int skt)
{
	return lwip_close(skt);
}

/**
 * [绑定套接字]
 * @param  skt  [需要绑定的套接字]
 * @param  ip   [绑定的Ip地址]
 * @param  port [绑定的端口]
 * @return      [成功返回0]
 */
int SWIOT_Bind_Socket(int skt,unsigned int ip,unsigned short port)
{
	struct sockaddr_in addr = {0};
	
	addr.sin_family = AF_INET;
	if (ip == SWIOT_Inet_Addr("127.0.0.1"))
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		addr.sin_addr.s_addr = ip;
    addr.sin_port = SWIOT_htons(port);

    return lwip_bind(skt,(const struct sockaddr*)&addr,sizeof(addr));
}


/**
 *  * [监听函数]
 *   * @param  skt     [套接字]
 *    * @param  backlog [监听队列长度]
 *     * @return         [成功返回0 失败 < 0]
 *      */
int SWIOT_Listen( int skt,int backlog )
{
        return lwip_listen( skt,backlog );
}


/**
 * [设置套接字选项]
 * @param  skt        [套接字]
 * @param  level      [所在协议层]
 * @param  optname    [需要访问的选项名]
 * @param  optval     [选项值的缓冲]
 * @param  optval_len [选项的长度]
 * @return            [失败<0]
 */
int SWIOT_Setsocket_Opt(int skt,int level,int optname,void* optval,int optval_len)
{
	return lwip_setsockopt( skt,level,optname,optval,optval_len );
}

/**
 * [建立连接]
 * @param  skt  [套接字]
 * @param  ip   [连接ip]
 * @param  port [连接port]
 * @return      [失败 < 0 如果为非阻塞 且errno == EINPROGRESS 表示连接正在建立中]
 */
int SWIOT_Connect( int skt,unsigned int ip,unsigned short port )
{
	struct sockaddr_in addr = {0};

	addr.sin_family = AF_INET;
	addr.sin_port   = port;
	addr.sin_addr.s_addr = ip;

	return lwip_connect(skt,(const struct sockaddr*)&addr,sizeof( addr ));
}

int SWIOT_Fcntl(int s, int cmd, int val)
{
	return lwip_fcntl(s,cmd,val);
}


/**
 * [接受连接]
 * @param  skt  [套接字]
 * @param  ip   [对端ip]
 * @param  port [对端port]
 * @return      [<0 返回失败]
 */
int SWIOT_Accept( int skt,unsigned int *ip,unsigned short *port )
{
	int ret = -1;
	struct sockaddr_in addr = {0};
	socklen_t len = sizeof(addr);

	ret = lwip_accept( skt,(struct sockaddr*)&addr,&len );

	if( ret > 0 )
	{
		if( ip )
		*ip = addr.sin_addr.s_addr;

		if( port )
			*port = addr.sin_port;
	}
	
	return ret;
}

/**
 * [接受数据]
 * @param  skt [接受的套接字]
 * @param  buf [接受缓存区]
 * @param  len [缓存区大小]
 * @return     [== 0 表示对方连接中断 <0 且 errno == EAGAIN(栈满或空) || errno == EINTR(表示系统调用被中断) 表示可以继续读 其余为异常]
 */
int SWIOT_Recv( int skt,char *buf,int len )
{
	int ret = -1;

	ret = lwip_recv( skt,buf,len,0 );

	return ret;
}

/**
 * [发送数据]
 * @param  skt [发送的套接字]
 * @param  buf [发送缓存区]
 * @param  len [缓存区长度]
 * @return     [== 0 表示对方连接中断 <0 且 errno == EAGAIN(栈满或空) || errno == EINTR(表示系统调用被中断) 表示可以继续写 其余为异常]
 */
int SWIOT_Send( int skt,char *buf,int len )
{
	int ret = -1;

	ret = lwip_send( skt,buf,len,0 );

	return ret;
}

/**
 * [接收一个数据报，并保存数据报源地址]
 * @param  skt       [套接口]
 * @param  buf       [接收缓存区]
 * @param  len       [缓存区大小]
 * @param  from_ip   [源地址ip]
 * @param  from_port [源地址port]
 * @return           []
 */
int SWIOT_Recv_From(int skt,char* buf,int len,unsigned int* from_ip,unsigned short*from_port)
{
	struct sockaddr_in addr = {0};
	int ret = -1;
	socklen_t addr_len = sizeof( addr );

	ret = lwip_recvfrom( skt,buf,len,0,(struct sockaddr*)&addr,&addr_len );

	if( ret > 0 )
	{
		if( from_ip )
			*from_ip = addr.sin_addr.s_addr;

		if( from_port )
			*from_port = addr.sin_port;
	}

	return ret;
}

/**
 * [发送数据报]
 * @param  skt     [套接口]
 * @param  buf     [发送缓存区]
 * @param  len     [缓存区大小]
 * @param  to_ip   [目的端ip]
 * @param  to_port [目的端port]
 * @return         [description]
 */
int SWIOT_Send_To(int skt,char* buf,int len,unsigned int to_ip,unsigned short to_port)
{
	struct sockaddr_in addr = {0};
	int ret = -1;

	addr.sin_family = AF_INET;
	addr.sin_port   = to_port;
	addr.sin_addr.s_addr = to_ip;

	ret = lwip_sendto( skt,buf,len,0,(const struct sockaddr*)&addr,sizeof(addr) );

	return ret;	
}

