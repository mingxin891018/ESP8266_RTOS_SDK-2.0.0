#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define ACCEPT_ALL "*/*"
#define ACCEPT_HTML "text/html"
#define ACCEPT_JSON "application/json"
#define CONTENT_NORMAL "application/x-wwww-form-urlencode"
#define CONTENT_MULTI "multipart/form-data"
#define CRIL "\r\n"
#define SERV_PORT 80
#define http_utl_h

void num_to_string(char *str, int i)
{
    char *n = "%d";
    snprintf(str, sizeof(str), n, i);
}

struct post_data
{
    char postData[10 * 1024];
};

typedef struct post_data PostData, *PostData_pointer;

struct url
{
    char url[500];
    PostData postData;
};
typedef struct url URL, *PURL;
//全局变量cooki登陆操作后通过给你的cookie可得
char cookie[100];

//将url中的hOST和path部分分离开

//参数一：存储host的字符串
//参数二：存储path的字符串
//参数三：将被分解的URL

//返回值：成功返回1，失败返回0

int parse_url(char *host, char *path, char *url)
{
    int i = 0, j = 0;
    char *flag1, *flag2;
    char temp[200];
    strcpy(temp, url);
    if(strncmp(temp,"http://",7)!=0)
    {
        return -1;
    }
    if (url == NULL)
    {
        printf("url is null \n");
        return -1;
    }
    while (temp[i] != '\0')
    {
        if (temp[i] == '/')
        {
            j++;
        }
        i++;
        if (j == 3)
        {
            flag2 = &temp[i - 1];
            break;
        }
    }
    i = 0;
    j = 0;
    while (temp[i] != '\0')
    {
        if (temp[i] == '/')
        {
            j++;
        }
        i++;
        if (j == 2)
        {
            flag1 = &temp[i];
            break;
        }
    }
    if (flag2)
    {
        strcpy(path, flag2);
    }
    if (flag1)
    {
        if (flag2)
        {
            *flag2 = '\0';
        }
        strcpy(host, flag1);
        return 1;
    }
    else
    {
        return -1;
    }
}
/**
 * 将HOST和PATH部分组成一个完整的URL
 * 参数一：存储组织完成的URL
 * 参数二：存储host的字符串
 * 参数三：存储PATH的字符串
 * 返回值：成功返回1，失败返回0
 *
 */

int organize_url(char *url, char *host, char *path)
{
    char temp[200];
    if (host == NULL)
    {
        printf("host is NULL \n");
        return 0;
    }
    if (path == NULL)
    {
        printf("path=NULL\n");
        return 0;
    }
    strcpy(temp, "http://");
    strcat(temp, host);
    strcat(temp, path);
    strcpy(url, temp);
    return 1;
}
/**
 * 根据传入已经初始化的URL结构体，组织HTTP REQUEST
 * 参数一：存储经组织后的http request头部的字符串指针
 * 参数二：存储有URL和要POST的URL数据（不一定有）的URL结构体
 */
int http_request(char *request, PURL url)
{
    char temp[7000];
    char host[100];
    char path[200];
    char content_length[20];
    if (url == NULL)
    {
        printf("url is null\n");
        return -1;
    }

    if (strlen(url->postData.postData) > 5)
    {
        strcpy(temp, "POST");
    }
    else
    {
        strcpy(temp, "GET ");
    }
    if (parse_url(host, path, url->url))
    {

        if (strlen(path) != 0)
        {
            strcat(temp, path);
            strcat(temp, " ");
        }
    }
    else
    {
        printf("url not exit \n");
        return -1;
    }
    strcat(temp, "HTTP/1.1");
    strcat(temp, CRIL);
    strcat(temp, "Host: ");
    strcat(temp, host);
    strcat(temp, CRIL);

    if (strlen(cookie) > 10)
    {
        strcat(temp, "Cookie: ");
        strcat(temp, cookie);
        strcat(temp, CRIL);
    }
    // strcat(temp, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    // strcat(temp, "Accept: */*");
    strcat(temp,"Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, */*");
    strcat(temp, CRIL);
    strcat(temp,"User-Agent: GeneralDownloadApplication");
    strcat(temp,CRIL);
    strcat(temp, "Connection: close");
    strcat(temp, CRIL);
    strcat(temp, "Upgrade-Insecure-Reqests: 1");
    strcat(temp, CRIL);
    strcat(temp, "Accept-Encoding: gzip, deflate");
    strcat(temp, CRIL);
    strcat(temp, "Accept-Language: en-US,en;q=0.5");
    strcat(temp, CRIL);

    if (strlen(url->postData.postData) > 5) //当有数据POST给URL的时候，带上POST的数据
    {
        strcat(temp, "Content-Type: ");

        if (strstr(url->postData.postData, "------")) //检查是不是multipart date 数据
        {
            strcat(temp, CONTENT_MULTI); //注意：CONTENT_NORMAL和CONTENT_MULTI不是变量，是宏定义！！！！好吧，原本是小写的，现在我已经改了
            strcat(temp, "; boundary=");
            strcat(temp, "------WebKitFormBoundaryaoOeFzIPZu3yeBWJ\r\n");
        }
        else
        {
            strcat(temp, CONTENT_NORMAL);
            strcat(temp, CRIL);
        }

        strcat(temp, "Content-Length: ");
        num_to_string(content_length, strlen(url->postData.postData));
        strcat(temp, content_length);
        strcat(temp, CRIL);
        strcat(temp, CRIL);
        strcat(temp, url->postData.postData);
    }
    else
    {
        strcat(temp, CRIL);
    }
    strcpy(request, temp);
    return 1;
}
/**
 * URL结构体初始化函数
 * 参数一：指向要被初始化的URL结构体的指针
 * 参数二：完整的URL，如http://172.168.1.1/see
 * 参数三：要post的URL数据，可为NULL，当为NULL时，将初始化后的URL结构体给
 * http_request(char *request,PURL url)函数组织头部，会自动设置POST和GET方法
 * 
 */
int Init_URL(PURL Purl, char *url, char *postData)
{
    if (url != NULL)
    {
        strcpy(Purl->url, url);
    }
    else
    {
        return 0;
    }

    if (postData != NULL)
    {
        strcpy(Purl->postData.postData, postData);
    }
    else
    {
        strcpy(Purl->postData.postData, "");
    }

    return 1;
}
int rmfile(char *str)
{
    if (access(str, F_OK) != -1)
    {
        remove(str);
    }
    else
    {
    }
    return 0;
}
int str_to_int(const char *str)
{
    int temp = 0;
    const char *ptr = str;
    if (*str == '-' || *str == '+')
    {
        str++;
    }
    while (*str != 0)
    {
        if ((*str < '0') || (*str > '9'))
        {
            break;
        }
        temp = temp * 10 + (*str - '0');
        str++;
    }
    if (*ptr == '-')
    {
        temp = -temp;
    }
    return temp;
}
int getDownloadFilename(char *path, char *filename)
{
    if (path == NULL)
    {
        return -1;
    }
    char *p = NULL;
    char *temp = path;
    while ((p = strstr(temp, "/")) != NULL)
    {
        temp = p + 1;
    }
    if (strlen(temp) == 0)
    {
        return -1;
    }
    else
    {
        strcpy(filename, temp);
    }
    return 0;
}
int getContentLength(char *buf)
{
    //得到相应报文body的长度
    char *pos;
    char *pos2;
    char num[100];
    if ((pos = strstr(buf, "Content-Length")) != NULL)
    {
        if ((pos2 = strstr(pos, "\r\n")) != NULL)
        {
            pos = pos + 16;
            strncpy(num, pos, pos2 - pos);
        }
    }

    return str_to_int(num);
}
int getHostPort(char *host)
{
    char temp[100];
    int port;
    strcpy(temp, host);

    char *p = NULL;
    if ((p = strstr(temp, ":")) != NULL)
    {
        memset(host, 0, sizeof(host));
        port = str_to_int(p + 1);
        *p = '\0';
        strcpy(host, temp);
    }
    else
    {
        port = SERV_PORT;
    }
    return port;
}
