#ifndef __UDPCAST_H__
#define __UDPCAST_H__

int swiot_udpcast_start(char* ip,int port,char* data,int data_size);

int swiot_udpcast_ontimer(int now);

int swiot_udpcast_stop();

#endif //__UDPCAST_H__