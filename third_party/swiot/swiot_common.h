/**
* @date 	: 2018/6/20 18:02
* @author   : zhoutengbo
*/
#ifndef __SWIOT_IMPL_H__
#define __SWIOT_IMPL_H__

#include "swiot_config.h"
#include "esp_common.h"
#define  SWDEBUG(format,...) 		os_printf(format, ##__VA_ARGS__)
#include "swiot_device.h"
#include "swiot_setup.h"


/**
 * 设备发现相关
 * DEVICE_FIND_PORT             设备发现监听端口
 * DEVICE_PRODUCT_KEY           设备产品 KEY
 **/
#define DEVICE_FIND_PORT        4001
#define DEVICE_PRODUCT_KEY      "nLdFblXy"


#ifndef LOG_LEVEL 
#define LOG_LEVEL       LOG_ERROR
#endif

#define HTTP_OK                         200
#define HTTP_BAD_REQUEST                400
#define HTTP_INTERNEL_SERVER_ERROR      500
#define HTTP_UNAUTHORIZED               401
#define HTTP_NOT_FOUND                  404



#define log_error(format,...)           do {  \
                                            if(LOG_LEVEL <= LOG_ERROR) {  \
                                                SWDEBUG("[E][%s][%s][%d]"format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
                                            }  \
                                        } while(0);  
#define log_warn(format, ...)           do {  \
                                            if(LOG_LEVEL <= LOG_WARN) {  \
                                                SWDEBUG("[W][%s][%s][%d]"format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
                                            }  \
                                        } while(0);
 
#define log_info(format, ...)           do {  \
                                            if(LOG_LEVEL <= LOG_INFO) {  \
                                                SWDEBUG("[I][%s][%s][%d]"format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
                                            }  \
                                        } while(0);

#define log_debug(format, ...)          do {  \
                                            if(LOG_LEVEL <= LOG_DEBUG) {  \
                                                SWDEBUG("[D][%s][%s][%d]"format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
                                            }  \
                                        } while(0);


#define POINTER_SANITY_CHECK(ptr, err) \
    do { \
        if (NULL == (ptr)) { \
            log_error("Invalid argument, %s = %p\n", #ptr, ptr); \
            return (err); \
        } \
    } while(0)

#define STRING_PTR_SANITY_CHECK(ptr, err) \
    do { \
        if (NULL == (ptr)) { \
            log_error("Invalid argument, %s = %p\n", #ptr, (ptr)); \
            return (err); \
        } \
        if (0 == strlen((ptr))) { \
            log_error("Invalid argument, %s = '%s'\n", #ptr, (ptr)); \
            return (err); \
        } \
    } while(0)

#define ZERO_SANITY_CHECK(num, err) \
    do { \
        if (0 > (num)) { \
            log_error("Invalid argument, %s = %p\n", #num, num); \
            return (err); \
        } \
    } while(0)

void SWIOT_Common_Init();
void SWIOT_Common_Destroy();

uint32_t SWIOT_Common_Get_Ms();

char* url_get_param( char *url, char *name, char *value, int valuesize );

char *url_str_getval( char *buf, int buflen, char *name, char *value, int valuelen );

/*构建回复客户端的统一http头*/
int url_get_httpresp(int httpstatus, int sessionid, int content_length, char *respbuf, int bufsize);

char* url_trip_params( char* url,char* out,int out_size );
#endif //__SWIOT_IMPL_H__