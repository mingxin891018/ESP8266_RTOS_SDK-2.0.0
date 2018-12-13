#include "swiot_common.h"
#include "time.h"
#include "swiot_platform.h"

void* g_mutex ;

void SWIOT_Common_Init()
{
    g_mutex = SWIOT_Mutex_Create();
}

void SWIOT_Common_Destroy()
{
    if (g_mutex) {
        SWIOT_Mutex_Destroy(g_mutex);
        g_mutex = NULL;
    }
}

/* 取得UTC时间(s) */
uint32_t SWIOT_Common_Get_Ms()
{
    static int32_t first = 1;
	static struct timeval start;
	struct timeval now;
    uint32_t ret = 0;

    if(g_mutex)
        SWIOT_Mutex_Lock(g_mutex);
	gettimeofday( &now, NULL );
    if(g_mutex)
        SWIOT_Mutex_Unlock(g_mutex);

	if( first )
	{
		start = now;
		first = 0;
		return 0;
	}
	else {
        ret = (now.tv_sec-start.tv_sec)*1000 + now.tv_usec/1000 - start.tv_usec/1000;
	    return ret;
    }
}

char* url_get_param( char *url, char *name, char *value, int valuesize )
{
    int i=0;
    char *p;

    p = strstr( url, name );
  
    while( p != NULL )
    {
        if( ( p == url  || ( p > url && 
            ( *(p-1) == '?' || *(p-1) == ';' || *(p-1) == '&' || *(p-1) == ',') ) )
            &&  *(p+strlen(name)) == '=' )   
        {
            p = p + strlen(name);
            p++;
            
            i=0;
            for(;;p++)
            {
                if( *p==';' || *p=='&' || *p==',' || *p=='\0'|| *p==' ' )
                {
                    break;
                }
                if( i < valuesize-1 )
                {
                    value[i] = *p;
                    i++;
                }
                else
                    break;
            } 
            value[i] = '\0';
            return value;
        }
        else
            p = p + strlen(name);
          
        p = strstr(p,name);
    }

    return NULL;
}

char *url_str_getval( char *buf, int buflen, char *name, char *value, int valuelen )
{
    char *s, *e, *p;
    size_t len;
    size_t size;

    len = buflen >= 0 ? buflen : strlen(buf);

    memset( value, 0, valuelen );

    p = s = buf;
    while( p < buf+len )
    {
        /* 去掉开始的空格 */
        while( *p == ' ' ||  *p == ',' ||  *p == '\t' || *p == '\r' || *p == '\n' )
            p++;

        /* 记录行开始 */
        s = p;

        /* 找到行结束 */
        while( *p != '\r' && *p != '\n' && *p != '\0' )
            p++;
        e = p;

        /* 找到需要的字段 */
        if( memcmp( s, name, (int)strlen(name) ) == 0 )
        {
            /* 找到名称结束 */
            p = s;
            while( *p != ':' &&  *p != '=' &&  *p != '\t' && *p != '\r' && *p != '\n' && *p != '\0' )
                p++;
            if( *p == 0 || e <= p )
                goto NEXT_LINE;

            /* 找到值开始 */
            p++;
            while( *p == ' ' || *p == ':' ||  *p == '=' ||
                   *p == ',' || *p == '\t' || *p == '\r' || *p == '\n' )
            {
                p++;
            }
            while( s < e && *e == ' ' )
                e--;
            size = (valuelen-1)<(e-p) ? (valuelen-1):(e-p);
            if( value && 1 < valuelen )
                strncpy( value, p, (int)size );
            return value;
        }
NEXT_LINE:
        p = e + 1;
    }
    return NULL;
}

/*构建回复客户端的统一http头*/
int url_get_httpresp(int httpstatus, int sessionid, int content_length, char *respbuf, int bufsize)
{
   int resp_len = 0;

    switch( httpstatus )
    {
    case HTTP_OK:
        resp_len = snprintf(respbuf, bufsize,
            "HTTP/1.1 200 OK\r\n"
            "SessionId: %lld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/json;charset=UTF-8\r\n"
            "Content-Length: %d\r\n\r\n", sessionid, content_length );
        break;
    case HTTP_BAD_REQUEST:
        resp_len = snprintf(respbuf, bufsize,
            "HTTP/1.1 400 Bad Request\r\n"
            "SessionId: %lld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/json;charset=UTF-8\r\n"
            "Content-Length: %d\r\n\r\n", sessionid, content_length );
        break;
    case HTTP_INTERNEL_SERVER_ERROR:
        resp_len = snprintf(respbuf, bufsize,
            "HTTP/1.1 500 Internal Server Error\r\n"
            "SessionId: %lld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/json;charset=UTF-8\r\n"
            "Content-Length: %d\r\n\r\n", sessionid, content_length );
        break;
    case HTTP_UNAUTHORIZED:
        resp_len = snprintf(respbuf, bufsize,
            "HTTP/1.1 401 Unauthorized\r\n"
            "SessionId: %lld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/json;charset=UTF-8\r\n"
            "Content-Length: %d\r\n\r\n", sessionid, content_length );
        break;
    case HTTP_NOT_FOUND:
        resp_len = snprintf(respbuf, bufsize,
            "HTTP/1.1 404 Not Found\r\n"
            "SessionId: %lld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/json;charset=UTF-8\r\n"
            "Content-Length: %d\r\n\r\n", sessionid, content_length );
        break;
    default:
        break;
    }

    return resp_len;
}

char* url_trip_params( char* url,char* out,int out_size)
{
	int i=0;
    char *p;
    char* startpos = NULL;
    char* ret = NULL;
    int size = 0;

    p = url;

    while( *p && *p != '?' )
        p++;

    size = (int)(p - url);
    size = (out_size-1) > size?size:(out_size-1);
    memcpy(out,url,size);
    out[size]=0;
    ret = out;

    return ret;
}