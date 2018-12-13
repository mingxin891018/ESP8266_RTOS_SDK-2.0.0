#ifndef __SWIOT_DEVICEFIND_H__
#define __SWIOT_DEVICEFIND_H__

#include "esp_common.h"
#include "espconn.h"


#define MAX_ENTRY_NAME_SIZE 16
#define MAX_ENTRY_VALUE_SIZE 256


#define SWIOT_REQ               "swiot_req"
#define SWIOT_REQ_VALUE         "1"

#define SWIOT_DEVID             "swiot_devid"

#define SWIOT_CONNECT_INFO      "connect_info"
#define SWIOT_STATUS            "swiot_status"
#define SWIOT_FIN               "fin"

#define UDP_SEND_COUNT_MAX      5
#define MAX_BUFFER_SIZE         2048
#define RESEND_TIME_INTERVAL    2*1000    // 2 s

typedef struct entry_set_t{
    char name[MAX_ENTRY_NAME_SIZE];
    char value[MAX_ENTRY_VALUE_SIZE];
}entry_set;

typedef enum connet_status_t {
    CONNECT_NULL = 0,
    CONNECT_WAIT_REQ,
    CONNECT_REQ,
    CONNECT_SEND_DEVID,
    CONNECT_RECV_CON_INFO,
    CONNECT_CON_WAIT,
    CONNECT_FIN,
    CONNECT_MAX
}connet_status_em;

typedef enum core_status_t {
    CORE_CONNECT_FAILED = -1,
    CORE_CONNECTING = 0,
    CORE_CONNECT_SUCCESS = 1,
}core_status_em;

typedef void (*connect_info_cb)(char* connect_info);

typedef struct device_find_ct_t{
    int32_t port;
    connect_info_cb connect_cb;
    char device_sn[MAX_ENTRY_VALUE_SIZE];
    char product_key[MAX_ENTRY_NAME_SIZE];
    int32_t status;
    struct espconn conn;
    esp_udp conn_udp;
    int32_t udp_send_count;
    int32_t next_send_time;

    char buf[MAX_BUFFER_SIZE];
    char buf_len;
    int32_t connect_status;
} device_find_ct;


int32_t device_find_init(int32_t port, char* product_key, char* device_sn, connect_info_cb connect_cb);

int32_t device_find_ontime();

/**
 * @parma  core_status  设置core连接状态
 **/
void device_find_set_core_status( core_status_em core_status );

void device_find_deinit();

int32_t device_get_status();



#endif
