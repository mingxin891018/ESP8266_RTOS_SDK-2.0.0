#include "httpserver.h"
#include "swiot_platform.h"
#include "swiot_common.h"

#define DATA_TICK_TIMEOUT 60000
#define DEAD_TICK_TIMEOUT 10000
#define LIST_SIZE 2
#define STEP_SIZE 2
#define CLIENT_READ_BUFSIZE 512
#define CLIENT_WRITE_BUFSIZE 512
#define AGLIN(x) (((x)/4) * 4 + 4)
#define WORKING 0
#define EXITING 1
#define EXITED  2

typedef struct 
{
	int skt;			//客户端skt
	char* read_buf;	
	int read_bufsize;	
	int read_datasize;		//读取数据量
	char* write_buf;
	int write_bufsize;
	int write_datasize;		//写入数据量
	int hlen;
	int clen;
	int data_tick;
	int dead_tick;
	int dead;
}client_info_t;

typedef struct
{
	int                    listen_skt;			//监听skt
	client_info_t*         skt_list;     		//skt队列     
	int                    skt_num;				//队列中skt数量
	int                    skt_list_size;		//队列大小
	//void*                  thread_handle;
	swiot_packet_deal_func func;
	void*                  param;
	int                    exit;
	void*                  mutex;
}server_handle_t;

server_handle_t g_server_handle;

int swiot_list_init(int size);
int swiot_list_add(client_info_t* client);
int swiot_list_remove(int skt);
int swiot_list_getlen();
client_info_t* swiot_list_get(int idx);
void swiot_list_definit();

int swiot_client_send(client_info_t* client,char* data,int data_size,int now);
int swiot_client_recv(client_info_t* client,int now);

void work_thread(void *params);


int swiot_active_listener(char* ip,short port);
int swiot_enable_listener(int skt);
int swiot_active_skt(int skt);
int swiot_enable_skt(client_info_t* client);
int swiot_check_client(client_info_t* client,int now);
int swiot_accept();
void swiot_handle(int now);
void swiot_parser_packet(client_info_t* client);

int swiot_get_path(char* head,int head_size,char* out_uri,int out_size,char* method,int method_size);

/**
 * [sw_http_server_init http服务初始化]
 * @param  ip   [ip地址]
 * @param  port [端口]
 * @return      [成功返回句柄]
 */
int swiot_http_server_init( char* ip,short port,swiot_packet_deal_func func,void* param )
{
	int rc = -1;

	memset(&g_server_handle,0,sizeof(server_handle_t));

	if (0 != swiot_list_init(LIST_SIZE)) {
		log_error("Init list error!\n");
		goto ERROR;
	}

	g_server_handle.listen_skt = swiot_active_listener(ip,port);
	if (0 > g_server_handle.listen_skt) {
		log_error("active httpserver listener error!\n");
		goto ERROR;
	}

	g_server_handle.mutex = SWIOT_Mutex_Create();
	if (NULL == g_server_handle.mutex) {
		log_error("Init mutex error\n");
		goto ERROR;
	}

	g_server_handle.func = func;
	g_server_handle.param = param;
	g_server_handle.exit = WORKING;

	/*rc = SWIOT_Thread_Create(g_server_handle.thread_handle,work_thread,"http_thread",1024, NULL,2 );
	if (0 == rc) 
	{
		log_error("Create thread error\n");
		goto ERROR;
	}*/

	return 0;
ERROR:
	swiot_list_definit();
	if (0 <= g_server_handle.listen_skt)
		swiot_enable_listener(g_server_handle.listen_skt);
	if (g_server_handle.mutex)
		SWIOT_Mutex_Destroy(g_server_handle.mutex);
	return -1;
}


/**
 * [swiot_http_server_response 发送回复]
 * @param  handle    [句柄]
 * @param  code      [回复码]
 * @param  data      [数据]
 * @param  data_size [数据大小]
 * @return           [成功返回0]
 */
int swiot_http_server_response( void* client,int session_id,int code,char* data,int data_size )
{
	char head_buf[512] = {0};
	int head_size = 0;
	client_info_t* p_client=(client_info_t*)client;
	int now = SWIOT_Common_Get_Ms();

	POINTER_SANITY_CHECK(client,-1);
	POINTER_SANITY_CHECK(g_server_handle.skt_list,-1);
	POINTER_SANITY_CHECK(g_server_handle.mutex,-1);
	ZERO_SANITY_CHECK(g_server_handle.listen_skt,-1);


	head_size = url_get_httpresp(code,session_id,data_size,head_buf,sizeof(head_buf));
	if (0 >= head_size) {
		log_error("Gener head error\n");
		return -1;
	}

	log_info("Send head:%s\n",head_buf);
	SWIOT_Mutex_Lock(g_server_handle.mutex);
	swiot_client_send(p_client,head_buf,head_size,now);
	swiot_client_send(p_client,data,data_size,now);
	SWIOT_Mutex_Unlock(g_server_handle.mutex);
	return 0;
}

/**
* [swiot_http_server_ontimer 定时器]
* @param now [当前时间]
*/
int swiot_http_server_ontimer(int now)
{
	int ret = -1;
	if (WORKING == g_server_handle.exit) {
		swiot_accept();
		swiot_handle(now);
		ret = 0; 
	} else if (EXITING == g_server_handle.exit) {
		g_server_handle.exit = EXITED;
		ret = 1;
	}

	return ret;
}

/**
 * [swiot_http_server_destroy 销毁http服务]
 * @param  handle [句柄]
 * @return        [成功返回0]
 */
int swiot_http_server_destroy()
{
	int tick;
	int now;
	int i;
	client_info_t* client = NULL;

	g_server_handle.exit = EXITING;
	now = tick = SWIOT_Common_Get_Ms();
	while(EXITED != g_server_handle.exit && now + 5000 > tick ) {
		tick = SWIOT_Common_Get_Ms();
		SWIOT_msleep(1);
	}

	if (EXITED != g_server_handle.exit) {
		log_error("eixt error!\n");
		return -1;
	}	

	log_debug("end\n");
	swiot_list_definit();		

	if (g_server_handle.mutex) {
		SWIOT_Mutex_Destroy(g_server_handle.mutex);
		g_server_handle.mutex = NULL;
	}

	return 0;
}


int swiot_list_init(int size)
{
	int ret = -1;

	g_server_handle.skt_list = (client_info_t*)malloc(AGLIN(sizeof(client_info_t) * size));

	if (NULL == g_server_handle.skt_list) {
		log_error("Malloc list error\n");
		goto END;
	}

	g_server_handle.skt_num = 0;
	g_server_handle.skt_list_size = size;

	ret = 0;
END:
	return ret;
}

int swiot_list_add(client_info_t* client)
{
	POINTER_SANITY_CHECK(g_server_handle.skt_list,-1);
	POINTER_SANITY_CHECK(client,-1);

	if ( g_server_handle.skt_num >= g_server_handle.skt_list_size ) {
		log_info("Extend skt list,step=%d\n",STEP_SIZE);
		g_server_handle.skt_list = (client_info_t*)realloc(g_server_handle.skt_list,sizeof(client_info_t)*(g_server_handle.skt_list_size + STEP_SIZE));
		if (NULL == g_server_handle.skt_list) {
			log_error("Realloc memory error\n");
			return -1;
		}

		g_server_handle.skt_list_size += STEP_SIZE;
		log_debug("Now skt_list_size=%d\n",g_server_handle.skt_list_size);
	}

	g_server_handle.skt_list[g_server_handle.skt_num++] = *client;
	log_debug("Now skt_num=%d\n",g_server_handle.skt_num);
	return 0;
}
int swiot_list_remove(int skt)
{
	int rest_num=0;
	int rc = -1;
	int i;
	int j;

	POINTER_SANITY_CHECK(g_server_handle.skt_list,-1);

	for(i = 0;i < g_server_handle.skt_num;i++) {
		if (g_server_handle.skt_list[i].skt == skt)
			break;
	}

	/**debuf*/
	log_info("========================================\n");
	for ( j = 0; j < g_server_handle.skt_num; ++j)
	{
		/* code */
		log_info("client[%d],skt=%d\n",j,g_server_handle.skt_list[j].skt);
	}
	log_info("========================================\n");

	if(i < g_server_handle.skt_num) {
		/**relase*/

		rest_num = g_server_handle.skt_num - i - 1;
		memmove(g_server_handle.skt_list+i,g_server_handle.skt_list+i+1,sizeof(client_info_t) * rest_num);
		g_server_handle.skt_num--;
		rc = 0;


		log_info("========================================\n");
		for ( j = 0; j < g_server_handle.skt_num; ++j)
		{
			/* code */
			log_info("client[%d],skt=%d\n",j,g_server_handle.skt_list[j].skt);
		}
		log_info("========================================\n");
	}

	return rc;
}

int swiot_list_getlen()
{
	POINTER_SANITY_CHECK(g_server_handle.skt_list,-1);

	return g_server_handle.skt_num;
}
client_info_t* swiot_list_get(int idx)
{
	POINTER_SANITY_CHECK(g_server_handle.skt_list,NULL);

	if (0 > idx||idx >= g_server_handle.skt_num)
		return NULL;

	return g_server_handle.skt_list+idx;
}

void swiot_list_definit()
{
	int i;
	if (g_server_handle.skt_list) {
		for (i = 0;i < g_server_handle.skt_num;i++) {
			if (g_server_handle.skt_list[i].read_buf) {
				free(g_server_handle.skt_list[i].read_buf);
				g_server_handle.skt_list[i].read_buf = NULL;
			}
			if (g_server_handle.skt_list[i].write_buf) {
				free(g_server_handle.skt_list[i].write_buf);
				g_server_handle.skt_list[i].write_buf = NULL;
			}
		}
		free(g_server_handle.skt_list);
		g_server_handle.skt_list = NULL;
		g_server_handle.skt_list_size = 0;
		g_server_handle.skt_num = 0;
	}
}

int swiot_client_send(client_info_t* client,char* data,int data_size,int now)
{
	int rest_size;
	int cp_size;
	int ret = -1;
	POINTER_SANITY_CHECK(client,-1);
	//POINTER_SANITY_CHECK(data,-1);

	if (data && 0 < data_size) {
		rest_size = client->write_bufsize - client->write_datasize;
		
		cp_size = rest_size > data_size?data_size:rest_size;
		memcpy(client->write_buf + client->write_datasize,data,cp_size);
		client->write_datasize += cp_size;
		log_debug("add data[%d] to send list,Now rest buf_size[%d] cp_size[%d]\n",data_size,rest_size,cp_size);
	
	}

	if (0 < client->write_datasize) {

		ret = SWIOT_Send(client->skt,client->write_buf,client->write_datasize);

		if (0 < ret) {
			log_debug("Send data_size:%d data:%s\n",ret,client->write_buf);
			client->write_datasize -= ret;
			memmove(client->write_buf,client->write_buf + ret,client->write_datasize);
			client->data_tick = now;
			log_debug("Now data_size:%d\n",client->write_datasize);
		} else if (0 > ret && (EAGAIN != errno && EINTR != errno)){
			log_error("Socket error,skt=%d\n",client->skt);
			return -1;
		}
	}
	return 0;
}
int swiot_client_recv(client_info_t* client,int now)
{
	int ret = -1;

	POINTER_SANITY_CHECK(client,-1);

	ret = SWIOT_Recv(client->skt,
					 client->read_buf+client->read_datasize,
					 client->read_bufsize - client->read_datasize - 1);

	if (0 < ret) {
		log_debug("Recv data_size = %d\n",ret);
		client->read_datasize += ret;
		client->data_tick = now;
		client->read_buf[client->read_datasize] = 0;
	} else if (0 == ret || (0 > ret && (EAGAIN != errno && EINTR != errno))) {
		log_error("Socket error,skt=%d ret=%d\n",client->skt,ret);
		return -1;
	}

	return 0;
}

void work_thread(void *params) 
{
	log_debug("thread start...\n");
	while (WORKING == g_server_handle.exit) {
		SWIOT_msleep(1);
		swiot_accept();
		swiot_handle(SWIOT_Common_Get_Ms());
	}

	g_server_handle.exit = EXITED;
	vTaskDelete(NULL);
}


int swiot_active_listener(char* ip,short port)
{
	int skt = -1;
	int rc = -1;
	int flag = 1;

	skt = SWIOT_Creat_Socket(AF_INET,SOCK_STREAM,0);
	if (0 > skt) {
		log_error("Create socket error,skt=%d\n",skt);
		return -1;
	}

	SWIOT_Setsocket_Opt( skt, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag) );

	rc = SWIOT_Bind_Socket(skt,SWIOT_Inet_Addr(ip),port);
	if (0 != rc) {
		log_error("Bind error,rc=%d\n",rc);
		return -1;
	}

	rc = SWIOT_Listen(skt,5);
	if (0 != rc) {
		log_error("Listen error,rc=%d\n",rc);
		return -1;
	}

	rc = SWIOT_Fcntl(skt, F_SETFL,O_NONBLOCK);
	if (0 != rc) {
		log_error("Set unblock error\n");
		return -1;
	}
	log_debug("set unblock,rc=%d\n",rc);

	return skt;
}

int swiot_enable_listener(int skt)
{
	ZERO_SANITY_CHECK(skt,-1);

	return SWIOT_Close_Socket(skt);
}

int swiot_accept()
{
	int rc = -1;
	int skt = -1;

	ZERO_SANITY_CHECK(g_server_handle.listen_skt,-1);

	skt = SWIOT_Accept(g_server_handle.listen_skt,NULL,NULL);

	if(0 <= skt) {
		log_info("Get skt = %d\n",skt);
		rc = swiot_active_skt(skt);
		if (0 != rc) {
			log_error("active skt=%d error\n",skt);
			SWIOT_Close_Socket(skt);
			return -1;
		}
	} 

	rc = 0;
	return rc;
}

int swiot_active_skt(int skt)
{
	int rc = -1;
	client_info_t client = {0};

	ZERO_SANITY_CHECK(skt,-1);

	rc = SWIOT_Fcntl(skt, F_SETFL,O_NONBLOCK);
	if (0 != rc) {
		log_error("Set unblock error\n");
		SWIOT_Close_Socket(skt);
		return -1;
	}

	client.read_bufsize = CLIENT_READ_BUFSIZE;
	client.read_buf = (char*)malloc(client.read_bufsize);
	if (NULL == client.read_buf) {
		log_error("Malloc client read_buf error\n");
		goto ERROR;
	}

	client.write_bufsize = CLIENT_WRITE_BUFSIZE;
	client.write_buf = (char*)malloc(client.write_bufsize);
	if (NULL == client.write_buf) {
		log_error("Malloc client write_buf error\n");
		goto ERROR;
	}

	client.skt = skt;
	client.hlen = -1;
	client.clen = -1;
	client.data_tick = 0;
	client.dead_tick = 0;
	client.dead = 0; 

	rc = swiot_list_add(&client);
	if (0 != rc) {
		log_error("Add skt error\n");
		goto ERROR;
	}

	return 0;
ERROR:
	if (client.read_buf)
		free(client.read_buf);
	if (client.write_buf) 
		free(client.write_buf);
	return -1;
}

int swiot_enable_skt(client_info_t* client)
{
	POINTER_SANITY_CHECK(client,-1);

	if (0 <= client->skt) {
		SWIOT_Close_Socket(client->skt);
	}

	if (client->read_buf) {
		free(client->read_buf);
		client->read_buf = NULL;
	}

	if (client->write_buf) {
		free(client->write_buf);
		client->write_buf = NULL;
	}

	log_info("Remove socket,skt=%d\n",client->skt);
	if(0 != swiot_list_remove(client->skt)) {
		log_error("Remove socket error,skt=%d\n",client->skt);
	}

	return 0;
}

int swiot_check_client(client_info_t* client,int now)
{
	int rc = -1;

	POINTER_SANITY_CHECK(client,-1);

	if (0 == client->data_tick || now < client->data_tick)
		client->data_tick = now;

	if ((0 == client->dead)
		 && (client->data_tick + DATA_TICK_TIMEOUT < now)) {
		log_info("skt=%d time out\n",client->skt);
		client->dead_tick = now;
		client->dead = 1;
	}

	if (client->dead 
		&& (client->dead_tick + DEAD_TICK_TIMEOUT < now)) {
		log_info("skt=%d ,deading...\n",client->skt);
		return -1;
	}
	return 0;
}

void swiot_handle(int now)
{
	//int skt_len = 0;
	client_info_t* client;
	int i;

	//skt_len = swiot_list_getlen();
	//if (0 >= skt_len) {
	//	//log_debug("get list len=%d\n",skt_len);
	//	return;
	//}

	for(i = 0;i < g_server_handle.skt_num;i++) {
		client = swiot_list_get(i);
		if (!client || 0 > client->skt) {
			log_error("get skt error from list,skt_num=%d i=%d ,skt=%d\n",g_server_handle.skt_num,i,client->skt);
			if (client)
				swiot_enable_skt(client);
			continue;
		}

		if (0 > swiot_check_client(client,now)) {
			log_info("client time out,deading...,skt=%d\n",client->skt);
			swiot_enable_skt(client);
			continue;
		}

		SWIOT_Mutex_Lock(g_server_handle.mutex);
		if(-1 == swiot_client_send(client,NULL,0,now)||-1 == swiot_client_recv(client,now)) {
			log_info("send or recv error,skt=%d\n",client->skt);
			swiot_enable_skt(client);
			SWIOT_Mutex_Unlock(g_server_handle.mutex);
			continue;
		}
		SWIOT_Mutex_Unlock(g_server_handle.mutex);

		swiot_parser_packet(client);
		//log_debug("Get client,skt=%d read_datasize=%d write_datasize=%d\n",client->skt,client->read_datasize,client->write_datasize);
	}
}


void swiot_parser_packet(client_info_t* client)
{
	char* p = NULL;
	char tmp_buf[64] = {0};
	char method[8] = {0};
	char uri[256] = {0};
	int packet_size = -1;
	int session_id = 0;

	if (NULL == client)
		return;
	//POINTER_SANITY_CHECK(client,);

	if (0 >= client->hlen) {
		p = strstr(client->read_buf,"\r\n\r\n");
		if (p && p - client->read_buf < client->read_datasize) {
			p += 4;
			client->hlen = p - client->read_buf;
			if (url_str_getval(client->read_buf,client->hlen,"Content-Length",tmp_buf,sizeof(tmp_buf))) {
				client->clen = atoi(tmp_buf);
			} else
				client->clen = 0;

			if (client->clen >= client->read_bufsize) {
				log_error("buf is not enougth\n");
				client->read_buf = (char*)realloc(client->read_buf,AGLIN(client->clen + client->read_datasize + 64));
				if (NULL == client->read_buf) {
					log_error("realloc read buf error size:%d\n",client->clen + client->read_datasize + 64);
					return;
				}
			}

			log_debug("head:%s\n",client->read_buf);
			log_debug("clen:%d\n",client->clen); 
		}
	}

	if (-1 != client->clen && (client->read_datasize >= (client->clen + client->hlen))) {
		log_debug("Get packet,hlen=%d clen=%d,data_size=%d\n",client->hlen,client->clen,client->read_datasize);
		
		if (url_str_getval(client->read_buf,client->hlen,"SessionId",tmp_buf,sizeof(tmp_buf)))
			session_id = atoi(tmp_buf);
		packet_size = client->clen + client->hlen;

		swiot_get_path(client->read_buf,client->hlen,uri,sizeof(uri),method,sizeof(method));

		if (g_server_handle.func)
			g_server_handle.func((void*)client,method,uri,client->read_buf,client->hlen,packet_size,session_id,g_server_handle.param);
		
		client->read_datasize -= packet_size;
		memmove(client->read_buf,client->read_buf + packet_size,client->read_datasize );		
		client->clen = client->hlen = -1;
	}
}


int swiot_get_path(char* head,int head_size,char* out_uri,int out_size,char* method,int method_size)
{
	int cpy_size = 0;
	//int ret = NULL;
	char* p = head;
	char* q;

	while( *p == ' '&& *p != '\r' && *p != '\n' && p - head < head_size )
		p++;

	q = p;
	while( *p != ' '&& *p != '\r' && *p != '\n' && p - head < head_size )
		p++;

	
	if(method) {
		cpy_size = method_size > (p-q) ?(p-q):method_size;
		memcpy(method,q,cpy_size);
		log_debug("method:%s\n",method);
	}

	while( *p == ' '&& *p != '\r' && *p != '\n' && p - head < head_size )
		p++;

	q = p;
	while( *p != ' '&& *p != '\r' && *p != '\n' && p - head < head_size )
		p++;

	if(out_uri) {
		cpy_size = out_size > (p-q) ?(p-q):out_size;
		memcpy(out_uri,q,cpy_size);
		log_debug("path:%s\n",out_uri);
	}

	return 0;
}


