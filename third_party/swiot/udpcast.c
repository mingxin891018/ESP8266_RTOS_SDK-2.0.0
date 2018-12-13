#include "udpcast.h"
#include "swiot_common.h"
#include "swiot_platform.h"

#define UDPCAST_INTERAL 10000

typedef struct 
{
	int skt;
	int ip;
	int port;
	char data[256];
	int data_size;
	int tick;
}udp_handle_t;

udp_handle_t g_udp_handle;

int swiot_udpcast_start(char* ip,int port,char* data,int data_size)
{
	int skt = -1;
	int cp_size;
	int optval=1;

	POINTER_SANITY_CHECK(ip,-1);
	POINTER_SANITY_CHECK(data,-1);
	ZERO_SANITY_CHECK(data_size,-1);

	memset(&g_udp_handle,0,sizeof(udp_handle_t));
	g_udp_handle.skt = -1;

	skt = SWIOT_Creat_Socket( AF_INET,SOCK_DGRAM,0 );
	if(0 > skt) {
		log_error("Create udp skt error\n");
		goto FAIL;
	}

	SWIOT_Setsocket_Opt(skt,SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));

	g_udp_handle.skt = skt;
	g_udp_handle.ip = SWIOT_Inet_Addr(ip);
	g_udp_handle.port = SWIOT_htons(port);
	cp_size = sizeof(g_udp_handle.data) > data_size ?data_size:sizeof(g_udp_handle.data);
	memcpy(g_udp_handle.data,data,cp_size);
	g_udp_handle.data_size = cp_size;
	return 0;
FAIL:
	if(skt >= 0) {
		SWIOT_Close_Socket(skt);
	}
	return -1;
}

int swiot_udpcast_ontimer(int now)
{
	ZERO_SANITY_CHECK(g_udp_handle.skt,-1);
	int ret = -1;

	if (0 == g_udp_handle.tick|| g_udp_handle.tick > now) 
		g_udp_handle.tick = now;

	if (g_udp_handle.tick + UDPCAST_INTERAL < now) {
		log_info("broadcast udp:%s\n",g_udp_handle.data);
		ret = SWIOT_Send_To(g_udp_handle.skt,g_udp_handle.data,g_udp_handle.data_size,g_udp_handle.ip,g_udp_handle.port);
		log_info("broadcast ret:%d\n",ret);
		g_udp_handle.tick = now;
	}

	return 0;
}

int swiot_udpcast_stop()
{
	if (0 <= g_udp_handle.skt) {
		SWIOT_Close_Socket(g_udp_handle.skt);
		g_udp_handle.skt = -1;
	}
	return 0;
}
