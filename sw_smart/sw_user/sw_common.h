#ifndef _SW_COMMON_H_
#define _SW_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>


typedef struct _sw_url
{
	/** 
	 *      * @brief URL头 '// :'之前的一个词
	 *           */
	char head[32];	
	char user[32];
	char pswd[32];
	/** 
	 * 	 * @brief URL 域名信息(可能是IP)
	 * 	 	 */
	char hostname[256];
	/** 
	 * 	 * @brief has ipv4 IP
	 * 	 	 */
	bool has_ipv4;
	/** 
	 * 	 * @brief has ipv6 IP
	 * 	 	 */
	bool has_ipv6;
	/** 
	 * 	 * @brief URL指向的IP，网络字节序
	 * 	 	 */
	uint32_t ip;
	/** 
	 * 	 * @brief URL指向的IP，ipv6
	 * 	 	 */
	/** 
	struct in6_addr ipv6;
	 * 	 * @brief URL端口，网络字节序
	 * 	 	 */
	uint16_t port;
	/** 
	 * 	 * @brief URL path
	 * 	 	 */
	char path[128];		
	/** 
	 * 	 * @brief URL最后一个词
	 * 	 	 */
	char tail[128];
	/** 
	 * 	 * @brief URL最后一个词的后缀
	 * 	 	 */
	char suffix[32];
	/**
	 *      * @brief hostname[:port]
	 *           */
	char host[128];
}sw_url_t;

/** 
 * @brief 分析URL,
 * 
 * @param dst 指向分析后的结果
 * @param url 指向的源URL
 * 
 * @return 0表示分析成功,-1分析失败
 */
int sw_url_parse(sw_url_t* dst, const char* url);

/* 不区分大小写比较字符串的前n个字符 */
int xstrncasecmp(const char* s1, const char* s2, size_t n);

/* 不区分大小比较字符串 */
int xstrcasecmp(const char *s1, const char *s2);

/* 提取字段值 */
char *xstrgetval( char *buf, char *name, char *value, int valuelen );

/*判断输入的字符串是否是数字*/
int xstrisdigit(char* str);

/* 分析是不是一个有效的点分十进制IP地址,本函数不会判断是否时合法的IP地址类 */
bool sw_txtparser_is_address( char* buf );

/* 把数字字符串转化为16进制数组 */
int  txt2hex( const char* string, int length,uint8_t* binary,int binsize );

bool is_address(char* buf);



#endif
