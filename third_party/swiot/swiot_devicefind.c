#include "esp_common.h"
#include "espconn.h"

#include "swiot_common.h"
#include "swiot_devicefind.h"


#if 1
static device_find_ct g_device_find_ct;
void ICACHE_FLASH_ATTR udpclient_recv(void *arg, char *pdata, unsigned short len);

/* 字符串拷贝 */
int32_t osl_strncpy( const char *str1, const char *str2, int32_t maxlen )
{
	char *p1 = (char*)str1;
	char *p2 = (char*)str2;
	while( p1<str1+maxlen )
	{
		*p1 = *p2;
		if( *p1 == 0 )
			break;
		p1++;
		p2++;
	}
	return (int32_t)(p1 - str1);
}

int32_t osl_strncmp_nocase( const char *str1, const char *str2, int32_t size )
{
	uint8_t *p1 = (uint8_t *)str1;
	uint8_t *p2 = (uint8_t *)str2;
	uint8_t c1, c2;

	while( *p1 && *p2 && 
			p1 < ((uint8_t*)str1) + size )
	{
		c1 = toupper( *p1 );
		c2 = toupper( *p2 );
		if( c1 < c2 )
			return -1;
		else if( c2 < c1 )
			return 1;

		p1++;
		p2++;
	}
	if( p1 < ((uint8_t*)str1) + size )
	{
		c1 = toupper( *p1 );
		c2 = toupper( *p2 );
		if( c1 < c2 )
			return -1;
		else if( c2 < c1 )
			return 1;
		else
			return 0;
	}
	else
		return 0;
}


char* osl_str_trim( char *str )
{
	int32_t i = 0, n = 0;
	while( str[n] == ' ' || str[n] == '\t' || str[n] == '\n' || str[n] == '\r' )
		n++;
	if( 0 < n )
	{
		while( str[n+i] != 0 )
		{
			str[i] = str[n+i];
			i++;
		}
	}
	else
	{
		while( str[i] != 0 )
			i++;
	}
	i--;
	while( 0 < i && ( str[i] == 0 || str[i] == ' ' || 
			str[i] == '\t' || str[i] == '\n' || str[i] == '\r' ) )
	{
		i--;
	}
	str[i+1] = 0;
	return str;
}

int32_t get_entry_set(entry_set *entry_set_pt, char* str_pt )
{
    int32_t ret = -1;
    entry_set_pt->name[0] = 0;
    entry_set_pt->value[0] = 0;
    str_pt = osl_str_trim( str_pt );

    char* pt = strchr(str_pt, '=');
    if(pt == NULL)
    {
        return ret;
    }
    pt[0] =0;
    strncpy(entry_set_pt->name, str_pt, MAX_ENTRY_NAME_SIZE);
    entry_set_pt->name[MAX_ENTRY_NAME_SIZE -1] = 0;
    pt[0] = '=';

    strncpy(entry_set_pt->value, pt+1, MAX_ENTRY_VALUE_SIZE );
    entry_set_pt->value[MAX_ENTRY_VALUE_SIZE -1] = 0;

    ret = 0;
    return ret;
}

static int32_t get_msg_status( entry_set*  entry_set_pt  )
{
    int32_t msg_status = CONNECT_NULL;
    
    if( 0 == osl_strncmp_nocase(entry_set_pt->name, SWIOT_REQ, sizeof(entry_set_pt->name) -1 ) 
        && 0 == osl_strncmp_nocase(entry_set_pt->value, g_device_find_ct.product_key, sizeof(entry_set_pt->value) -1  )  ){
        msg_status = CONNECT_REQ;
    }else if( 0 == osl_strncmp_nocase(entry_set_pt->name, SWIOT_CONNECT_INFO, sizeof(SWIOT_CONNECT_INFO) ) ){
        msg_status = CONNECT_RECV_CON_INFO;
    }else if( 0 == osl_strncmp_nocase(entry_set_pt->name, SWIOT_FIN, sizeof(SWIOT_FIN) ) ){
        msg_status = CONNECT_FIN;
    }
    return msg_status;
}

int32_t device_find_ontime()
{
    udpclient_recv(NULL,NULL,0);
    return 200;
}

/**
 * @parma  core_status  设置core连接状态
 **/
void device_find_set_core_status( core_status_em core_status )
{
    g_device_find_ct.connect_status = core_status;
    
}

void device_find_deinit()
{
     espconn_disconnect(&(g_device_find_ct.conn));
}

int32_t device_get_status()
{
    return  g_device_find_ct.status;
}

//回调函数
void ICACHE_FLASH_ATTR udpclient_recv(void *arg, char *pdata, unsigned short len)
{   
    int ret_code = len;
    int32_t msg_status = CONNECT_NULL;
    entry_set entry_set_tmp = { 0 };
    
    remot_info* pcon_info = NULL;
		
    struct espconn *conn = (struct espconn *)arg;
    log_debug("udpclient_recv pdata = %s\n",pdata);

    memcpy(g_device_find_ct.buf, pdata, len);
    g_device_find_ct.buf_len = ret_code;
    g_device_find_ct.buf[ret_code] = 0;
    log_debug("%s:%s\n", "addrstr", g_device_find_ct.buf);
    ret_code = get_entry_set(&entry_set_tmp, g_device_find_ct.buf);
    //clean buf
    g_device_find_ct.buf_len = 0;
    log_debug("key:%s<<----, val:%s<<----\n", entry_set_tmp.name, entry_set_tmp.value);
    if( ret_code == 0)
    {
        msg_status = get_msg_status( &entry_set_tmp );
    }
    log_debug("msg_status = %d\n", msg_status);
    // msg_status check
    switch( g_device_find_ct.status ){
    case CONNECT_WAIT_REQ:
        {   
            log_debug("CONNECT_WAIT_REQ\n");
            if( CONNECT_REQ == msg_status ){
                g_device_find_ct.status = CONNECT_REQ; 
                g_device_find_ct.udp_send_count = 0;
                //clean buf
                g_device_find_ct.buf_len = 0;
                return ;
            }

            break;
        }
    case CONNECT_REQ:
        {
            log_debug("CONNECT_REQ\n");
            if( CONNECT_RECV_CON_INFO == msg_status  ){
                g_device_find_ct.status = CONNECT_CON_WAIT; 
                g_device_find_ct.udp_send_count = 0;
                //clean buf
                g_device_find_ct.buf_len = 0;
                if( g_device_find_ct.connect_cb != NULL )
                {
                    log_debug("callback: value=%s\n", entry_set_tmp.value);
                    g_device_find_ct.connect_cb(entry_set_tmp.value);
                }else
                {
                    log_debug("g_device_find_ct.connect_cb == NULL\n");
                }
                return ;
            }
            // fill msg
            if( g_device_find_ct.buf_len <=0){
                g_device_find_ct.buf_len = snprintf(g_device_find_ct.buf,MAX_BUFFER_SIZE-1, "%s=%s", SWIOT_DEVID, g_device_find_ct.device_sn );
            }
            break;
        }
    case CONNECT_CON_WAIT:
        {
            log_debug("CONNECT_CON_WAIT\n");
            if( CONNECT_FIN  == msg_status ){
                log_debug("get fin\n");
                g_device_find_ct.status = CONNECT_FIN; 
                g_device_find_ct.udp_send_count = 0;
                //clean buf
                g_device_find_ct.buf_len = 0;
                return ;
            }
            // fill msg
            g_device_find_ct.buf_len = snprintf(g_device_find_ct.buf, MAX_BUFFER_SIZE-1, "%s=%d", SWIOT_STATUS, g_device_find_ct.connect_status );
            break;
        }
    case CONNECT_FIN:
        {
            log_debug("connect_status = %d\n", g_device_find_ct.connect_status);
            switch( g_device_find_ct.connect_status )
            {
            case CORE_CONNECT_FAILED:
            case CORE_CONNECTING:
                g_device_find_ct.status = CONNECT_WAIT_REQ;
                break;
            case CORE_CONNECT_SUCCESS:
                g_device_find_ct.status = CONNECT_NULL;
                break;
            default:
                log_error("error, connect_status = %d\n", g_device_find_ct.connect_status);
                g_device_find_ct.status = CONNECT_WAIT_REQ;
            }
            g_device_find_ct.udp_send_count = 0;
            //clean buf
            g_device_find_ct.buf_len = 0;
            break;
        }
    default:
        log_debug("CONNECT_CODE = %d\n", g_device_find_ct.status);
        break;
    }

    if( g_device_find_ct.buf_len > 0  && g_device_find_ct.next_send_time < SWIOT_Common_Get_Ms() )
    {
        // send msg
        if( g_device_find_ct.udp_send_count > UDP_SEND_COUNT_MAX )
        {
            log_debug("rech max send count\n");
            g_device_find_ct.status = CONNECT_WAIT_REQ;
            g_device_find_ct.udp_send_count = 0;
            //clean buf
            g_device_find_ct.buf_len = 0;
            return ;
        }
        log_debug("sendto: %d %s\n",g_device_find_ct.buf_len,g_device_find_ct.buf);
        
        espconn_get_connection_info(&(g_device_find_ct.conn), &pcon_info, 0);
		log_debug("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		log_debug("remote port: %d \r\n",pcon_info->remote_port);
        g_device_find_ct.conn.proto.udp->remote_ip[0] = pcon_info->remote_ip[0];
        g_device_find_ct.conn.proto.udp->remote_ip[1] = pcon_info->remote_ip[1];
        g_device_find_ct.conn.proto.udp->remote_ip[2] = pcon_info->remote_ip[2];
        g_device_find_ct.conn.proto.udp->remote_ip[3] = pcon_info->remote_ip[3];
        g_device_find_ct.conn.proto.udp->remote_port = pcon_info->remote_port;
        
        ret_code = espconn_send(&(g_device_find_ct.conn), g_device_find_ct.buf, g_device_find_ct.buf_len);
        log_debug("send ret = %d\n", ret_code);
        g_device_find_ct.udp_send_count ++;
        g_device_find_ct.next_send_time = SWIOT_Common_Get_Ms() + RESEND_TIME_INTERVAL;
    }
}

int32_t device_find_init(int32_t port, char* product_key, char* device_sn, connect_info_cb connect_cb)
{
    int32_t ret = 0;
    log_debug("init-------\n");
    g_device_find_ct.status = CONNECT_NULL;
    g_device_find_ct.udp_send_count =0;
    g_device_find_ct.next_send_time =0;
    g_device_find_ct.buf_len =0;
    g_device_find_ct.port = port;
    g_device_find_ct.connect_status = 0;
    g_device_find_ct.connect_cb = connect_cb;
    if(device_sn != NULL)
    {
        osl_strncpy(g_device_find_ct.device_sn, device_sn, MAX_ENTRY_VALUE_SIZE -1 );
        g_device_find_ct.device_sn[MAX_ENTRY_VALUE_SIZE -1] =0;
    }
    if(product_key!= NULL )
    {
        osl_strncpy(g_device_find_ct.product_key, product_key, MAX_ENTRY_VALUE_SIZE -1 );
        g_device_find_ct.product_key[MAX_ENTRY_VALUE_SIZE -1] =0; 
    }

    g_device_find_ct.conn.type = ESPCONN_UDP;           //要建立的连接类型为UDP
    g_device_find_ct.conn.proto.udp = &g_device_find_ct.conn_udp;
    g_device_find_ct.conn.proto.udp->local_port = port; //设置本地端口
    g_device_find_ct.conn.proto.udp->remote_port = port;
    g_device_find_ct.conn.proto.udp->remote_ip[0] = 255;
    g_device_find_ct.conn.proto.udp->remote_ip[1] = 255;
    g_device_find_ct.conn.proto.udp->remote_ip[2] = 255;
    g_device_find_ct.conn.proto.udp->remote_ip[3] = 255;
    
    espconn_regist_recvcb(&(g_device_find_ct.conn), udpclient_recv); 
    espconn_create(&(g_device_find_ct.conn));//建立UDP传输
    
    g_device_find_ct.status = CONNECT_WAIT_REQ;

    return ret;
}

#endif