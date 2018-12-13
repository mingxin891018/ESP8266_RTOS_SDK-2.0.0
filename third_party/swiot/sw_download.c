#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_libc.h>
#include <netdb.h>
#include <sockets.h>
#include "http_url.h"
#include "sw_download.h"
#include "init_connect.h"
#include "swiot_common.h"

#define INITIALIZE 0
#define DOWNLOAD_SUCCESS 1
#define WRITE_SUCCESS -1
#define WRITE_ERROR -2

#define CONNECT_SUCCESS -4

#define URL_ERROR -5
#define GET_IP_ERROR -6

#define OPEN_SOCKET_ERROR -7
#define OPEN_SOCKET_SUCCESS -8

#define ERROR_404 -9
#define DOWNLOAD_ERROR -11

#define SEND_REQUEST_ERROR -12
#define SEND_REQUEST_SUCCESS -13

#define RECEIVE_ERROR -14
#define CONNECTING -15
#define SENDING -16
#define BUFFER_FULL -17
#define RECEIVING_RESPONSE -18
#define BUFFER_NULL -19
#define WRITE_IN_FILE_AGAIN -20
#define RECEIVE_OVER -21

#define SAVE_DIR_NOT_EXIST -23

#define MAXLINE 512
#define SERV_PORT 80
#define BUF_SIZE 2048
#define MAX_WRITE 512

sw_upgrade_progress_handle callback;
sw_download_handle g_download_handle;
int overtime;
char filename[100];
struct sockaddr_in servaddr;
char request[2000];
char buffer[BUF_SIZE];
int port;
int sockfd;
char *hostIP = NULL;
FILE *fp;
int state;
PURL purl;
char tempdir[100];
int laststate;
int startms;
fd_set ready_sockfd;
struct timeval timeout;
int have_send;
int request_len;
int write_in_buffer;
int if_parse;
char *begin_write;
int truelen;
int write_total_len;
int count;
int progress;



int parse_header()
{
    int ret = 0;
    char *pos = NULL;

    if (write_in_buffer > 20)
    {
    if ((pos = strstr(buffer, " ")) != NULL)
    {
        if (strncmp("404", pos + 1, 3) == 0)
        {
                ret = ERROR_404;
                return ret;
            }
            else if (strncmp("200", pos + 1, 3) == 0)
            {
                ret = 2;
            }
            else
            {
                ret = 3;
            }
        }
    }
    if ((pos = strstr(buffer, "\r\n\r\n")) != NULL)
    {
         log_error("recv:%s\n",buffer);
        begin_write = pos + 4;
        count = write_in_buffer - (begin_write - buffer);
    }
    log_error("11111111111111111111\n");
    truelen = getContentLength(buffer);
    log_error("22222222222222222222\n");
    if (truelen > 0 && g_download_handle) {
        log_error("truelen:%d\n>>>>>>>>>>>>>",truelen);
        g_download_handle(buffer, truelen,true);
        log_error("truelen:%d out..............\n",truelen);
    }
            
    return ret;
}

int receive_response()
{
    int n;
    char *temp;
    int will_read;
    int parse_ret;
    if (write_in_buffer < BUF_SIZE)
    {
        will_read = BUF_SIZE - write_in_buffer;
        temp = buffer + write_in_buffer;
        n = Read(sockfd, temp, will_read);
        log_info("have read %d \n", n);
        if (n <= 0)
        {
            if (if_parse == 0)
            {
                parse_ret = parse_header();
                if (parse_ret != 2)
            {
                    log_error("请求服务不存在\n");
                    if (write_in_buffer < 20)
                    {
                        return SEND_REQUEST_SUCCESS;
                    }
                    else
                    {
                    return ERROR_404;
                    }
            }
            else
            {
                if_parse = 1;
            }
        }
        else
        {
            begin_write = buffer;
            count = write_in_buffer;
        }
            if (begin_write == buffer)
            {
                if ((write_total_len + write_in_buffer) == truelen)
                {
                    return RECEIVE_OVER;
                }
                else
                {
                    return RECEIVE_ERROR;
                }
            }
            else
            {
                if ((write_in_buffer - (begin_write - buffer)) == truelen)
                {
                    return RECEIVE_OVER;
                }
                else
                {
                    return RECEIVE_ERROR;
                }
            }
        }
        else
        {
            write_in_buffer += n;
        }
        if (write_in_buffer == BUF_SIZE)
        {
            if (if_parse == 0)
            {
                if (parse_header() != 2)
                {
                    log_error("请求服务不存在\n");
                    return ERROR_404;
                }
                else
                {

                    if_parse = 1;
                }
            }
            else
            {
                begin_write = buffer;
                count = write_in_buffer;
            }
            return BUFFER_FULL;
        }
        else
        {
            return RECEIVING_RESPONSE;
        }
    }
}
int write_in_file()
{

    int ret = 0;
    //printf("begfin write in file \n ");
    //printf("count %d \n", count);
    if (count != 0)
    {
        if (count > MAX_WRITE)
        {
           // printf("begin - buffer %d \n ", begin_write - buffer);
            if (g_download_handle)
                g_download_handle(begin_write, MAX_WRITE,false);

            
            write_total_len += MAX_WRITE;
            begin_write += MAX_WRITE;
            count -= MAX_WRITE;
            ret = WRITE_IN_FILE_AGAIN;
        
        }
        else
        {
            if (g_download_handle)
                g_download_handle(begin_write, count,false);
           
            write_total_len += count;
            count = 0;
            write_in_buffer = 0;
            ret = BUFFER_NULL;

        }
    }
    if (write_total_len == truelen)
    {
        ret = WRITE_SUCCESS;
    }
    else
    {

        if (((((int)(write_total_len / ((float)(truelen)) * 100)) / 10) - progress) > 0)
        {
            progress += 1;
           // printf("progress %d \n", progress);
            callback(progress * 10, filename);
        }
    }
    return ret;
}
int open_socket()
{
    if ((sockfd = Socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        return OPEN_SOCKET_ERROR;
    }
    //lwip_ioctl(sockfd, FIONBIO, 1);
   // fcntl(sockfd, F_SETFL, O_NONBLOCK);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
   // inet_pton(AF_INET, hostIP, &servaddr.sin_addr);
    //ipaddr_aton(hostIP, (ip_addr_t *)&servaddr.sin_addr);
    servaddr.sin_addr.s_addr = inet_addr(hostIP);
    log_error("ip:%d port:%d\n",inet_addr(hostIP),htons(port));
    servaddr.sin_port = htons(port);
    return OPEN_SOCKET_SUCCESS;
}
int connect_to_server()
{
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        FD_ZERO(&ready_sockfd);
        return CONNECTING;
    }
    else
    {

        return CONNECT_SUCCESS;
    }
}
int Select()
{
    FD_SET(sockfd, &ready_sockfd);
   // printf("Selcet \n");
    int ret = select(sockfd + 1, &ready_sockfd, NULL, NULL, &timeout);
    // int ret = select(sockfd + 1, &ready_sockfd, N ULL, NULL, NULL);
    //log_error("ret:%d\n",ret);
    if (FD_ISSET(sockfd, &ready_sockfd))
    {
        return CONNECT_SUCCESS;
    }
    else
    {
        return CONNECTING;
    }
}
int send_request()
{
    //printf("begin send request \n");
    char *write_temp = request;
    // printf("%s \n ",request);
    int will_write = 0;
    will_write = request_len - have_send;
    int n = 0;
    //printf("request len %d \n ", request_len);
   // printf("have send %d \n", have_send);
    if (have_send < request_len)
    {
        write_temp += have_send;

        if ((n = write(sockfd, write_temp, will_write)) == -1)
        {
            have_send = 0;
            log_error("send error \n ");
            return SEND_REQUEST_ERROR;
        }
        else
        {
            have_send += n;
            //printf("have send %d \n", have_send);
        }
    }
    if (have_send == request_len)
    {
        have_send = 0;
        log_debug("send reaquest success \n");
        int flags;
        if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1)
        {
            log_error("flags ------------errror \n");
            return DOWNLOAD_ERROR;
        }
        else
        {
            flags = 0;
        }
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
        return SEND_REQUEST_SUCCESS;
    }
    else
    {
        return SENDING;
    }
}
int sw_start_download(char *download_url, sw_upgrade_progress_handle callback_handle,sw_download_handle download_handle, int over_time)
{

    char *urll = download_url;
    char host[100];
    char path[100];
    URL purl={0};
    state = INITIALIZE;
    overtime = over_time;
    timeout.tv_usec = 0;
    timeout.tv_sec = 0;
    port = 0;
    sockfd = 0;
    state = 0;
    laststate = -222;
    startms = 0;
    have_send = 0;
    write_in_buffer = 0;
    if_parse = 0;
    begin_write = NULL;
    truelen = 0;
    write_total_len = 0;
    count = 0;
    progress = 0;
    callback = callback_handle;
    g_download_handle = download_handle;
    if (download_url == NULL)
    {
        callback(-1, NULL);
        return -1;
    }
    //获取url path
    if (parse_url(host, path, urll) == -1)
    {
        printf("parse_url error\n");
        return URL_ERROR;
    }
    #if 0
    if (access(save_dir, F_OK) != 0)
    {
        callback(-1, NULL);
        return SAVE_DIR_NOT_EXIST;
    }
    //获取下载的文件名
    if (getDownloadFilename(path, filename) == -1)
    {
        callback(-1, NULL);
        return URL_ERROR;
    }
    strcpy(tempdir, save_dir);
    if (tempdir[strlen(tempdir) - 1] != '/')
    {

        strcat(tempdir, "/");
    }
    strcat(tempdir, filename);
    #endif
    //获取端口
    port = getHostPort(host);
   // purl = (PURL)malloc(sizeof(struct url));

    if (Init_URL(&purl, urll, NULL) == -1)
    {
        callback(-1, NULL);
        return URL_ERROR;
    }
    if ((http_request(request, &purl)) == -1)
    {
        callback(-1, NULL);
        return URL_ERROR;
    }
    request_len = strlen(request);
    struct hostent *hosth;
    if ((hosth = gethostbyname(host)) == NULL)
    {
        callback(-1, NULL);
        return GET_IP_ERROR;
    }
    #if 0
    rmfile(tempdir);
    fp = fopen(tempdir, "wb");
    #endif
    hostIP = inet_ntoa(*((struct in_addr *)hosth->h_addr));
    return 0;
}
int sw_on_time()
{
	int now = SWIOT_Common_Get_Ms();
    if (laststate != state)
    {
         startms = SWIOT_Common_Get_Ms();
        laststate = state;
    }
    switch (state)
    {
    case INITIALIZE:
        state = open_socket();
        break;
    case OPEN_SOCKET_ERROR:
        if (now - startms < overtime)
        {
            state = open_socket();
        }
        else
        {
            state = DOWNLOAD_ERROR;
        }
        break;
    case OPEN_SOCKET_SUCCESS:
        log_debug("open socket success  \n");
        state = connect_to_server();
        break;
    case CONNECTING:
    //log_error("CONNECTING\n");
        if (now - startms < overtime)
        {
            state = Select();
        }
        else
        {
            state = DOWNLOAD_ERROR;
        }
        break;
    case CONNECT_SUCCESS:
        log_debug("connect success \n");
        state = send_request();
        break;
    case ERROR_404:
        state = DOWNLOAD_ERROR;
        break;

    case SEND_REQUEST_ERROR:
        log_error("send request error \n");
        if (now - startms < overtime)
        {
            state = send_request();
        }
        else
        {
            state = DOWNLOAD_ERROR;
        }
        break;
    case SENDING:
        state = send_request();
        break;
    case SEND_REQUEST_SUCCESS:
        log_debug("send request success \n ");
        state = receive_response();
        break;
    case RECEIVE_ERROR:
        if (now - startms < overtime)
        {
            state = receive_response();
        }
        else
        {
            state = DOWNLOAD_ERROR;
        }
        break;

    case RECEIVE_OVER:
        state = write_in_file();
        break;
    case BUFFER_FULL:
        state = write_in_file();
        break;
    case WRITE_IN_FILE_AGAIN:
        state = write_in_file();
        break;
    case RECEIVING_RESPONSE:
        state = receive_response();
        break;
    case BUFFER_NULL:
        state = receive_response();
        break;
    case WRITE_ERROR:
        if (now - startms < overtime)
        {
            state = write_in_file();
        }
        else
        {
            state = DOWNLOAD_ERROR;
        }
        break;
    case WRITE_SUCCESS:

        state = DOWNLOAD_SUCCESS;
        break;
    case DOWNLOAD_SUCCESS:
        state = DOWNLOAD_SUCCESS;
        break;
    default:
        state = DOWNLOAD_ERROR;
        break;
    }
    if (state == DOWNLOAD_SUCCESS)
    {
        //fclose(fp);
        Close(sockfd);
        free(purl);
        callback(100, filename);
        log_info("download success-----------download success -------\n");
        // SW_LOG_DEBUG("download success-----------download success -------\n");

        return state;
    }
    else if (state == DOWNLOAD_ERROR)
    {
       // fclose(fp);
        log_error("download_error-------download error----\n");
        Close(sockfd);
        free(purl);
        //rmfile(tempdir);
        callback(-1, NULL);
        return state;
    }
    else
    {

        return state;
    }

}