#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "esp_spiffs.h"
#include "sw_debug_log.h"

#define BIG_END 		1
#define LITTLE_END 		0

int m_endtype = -1;


/* 不区分大小写比较字符串的前n个字符 */
int xstrncasecmp(const char* s1, const char* s2, size_t n)
{
	int i=0, ret=0;

	while( s1[i] != 0 && s2[i] != 0 && i < (int)n )
	{   
		ret = toupper(s1[i]) - toupper(s2[i]);
		if( ret != 0 ) 
			break;
		i++;
	}   
	if( i < (int)n )
		ret = toupper(s1[i]) - toupper(s2[i]);

	if( ret == 0 ) 
		return 0;
	else if( ret < 0 ) 
		return -1; 
	else
		return 1;

	return 0;
}

/* 不区分大小比较字符串 */
int xstrcasecmp(const char *s1, const char *s2)
{
	int i=0, ret=0;

	while( s1[i] != 0 && s2[i] != 0 )
	{
		ret = toupper(s1[i]) - toupper(s2[i]);
		if( ret != 0 )
			break;
		i++;
	}
	ret = toupper(s1[i]) - toupper(s2[i]);

	if( ret == 0 )
		return 0;
	else if( ret < 0 )
		return -1;
	else
		return 1;
}

/* 提取字段值 */
char *xstrgetval( char *buf, char *name, char *value, int valuelen )
{
	char *s, *e, *p;
	int len = strlen(buf );

	memset( value, 0, valuelen );

	/* 分析版本信息文件 */
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
		if( xstrncasecmp( s, name, strlen(name) ) == 0 )
		{
			/* 找到名称结束 */
			p = s;
			while( *p != ':' &&  *p != '=' &&  *p != '\t' && *p != '\r' && *p != '\n' && *p != '\0' )
				p++;
			if( *p == 0 || e <= p )
				goto NEXT_LINE;

			/* 找到值开始 */
			p++;
			while( *p == ':' ||  *p == '=' ||  *p == ',' ||  *p == '\t' || *p == '\r' || *p == '\n' )
				p++;
			strncpy( value, p, valuelen-1<e-p ? valuelen-1:e-p );/* 入口处已经清零value了,切e-p肯定>0也可使用memcpy或者memmove */
			return value;
		}
NEXT_LINE:
		p = e + 1;
	}
	return NULL;
}

/*
 *  @brief:判断输入的字符串是否是数字
 *   */
int xstrisdigit(char* str)
{
	if( str == NULL )
		return -1;
	for( ; *str!=0; str++)
	{
		if( isdigit(*str) == 0 )
			return -1;
	}
	return 0;
}

/* 分析是不是一个有效的点分十进制IP地址,本函数不会判断是否时合法的IP地址类 */
bool sw_txtparser_is_address( char* buf )
{
	/* 实际上在internet上的网络ip地址表达式标准:a,a.b,a.b.c,a.b.c.d（可8，10，16进制随机组合) */
	/* !inet_aton(dst->hostname, NULL)用此函数判断更为准确,那天如果有要就需改为此函数判断 */
	if ( buf == NULL )
		return false;
	int i, j, k;
	int sip;
	j = i = 0 ; 
	k = 0;
	while (k < 4)
	{   
		sip = 0;
		while (buf[i] != '.' && buf[i] != '\0')
		{   
			if ( 2 < (i-j) || buf[i] < '0' || '9' < buf[i] )
				return false;
			sip = sip * 10 + buf[i] - '0';
			i++;
		}   
		if ( j == i || sip > 255 )/* 没有数据.9..0或者大于255 */
			return false;
		i++;
		j = i;
		++k;
	}   
	return (k == 4 && buf[i-2] != '.' && buf[i-1] == '\0') ? true : false;
}

/* 提取等式的左右两边的字符串 */
char* equation_parse_as_line( char* equations,char** pleft,char** pright )
{
	char* bptr = equations;

	//去掉特殊字符和空格
	while( *bptr !='\0' && ( *bptr =='\r' || *bptr =='\n' || *bptr ==' ' ) )
		bptr++;

	//取得等式的左边
	*pleft = bptr;
	//找等于号
	while( *bptr !='\0' && *bptr !='\r' && *bptr !='\n' )
	{
		if( *bptr == '=' )
			break;
		bptr++;
	}
	//找到等于号,取等式右边
	if( *bptr == '=' )
	{
		bptr++;
		//去掉空格
		while( *bptr == ' ' )
			bptr ++;
		*pright = bptr;
	}
	else
		*pright = NULL;

	//找到下一个equatio的开始
	while ( *bptr != '\0' && *bptr !='\r' && *bptr !='\n' )
		bptr++;

	return bptr;
}

static unsigned char to_hex ( char *ptr )
{
	if ( isdigit( *ptr ) )
	{
		return ( *ptr - '0' );
	}
	return ( tolower( *ptr ) - 'a' + 10 );
}

/* 把数字字符串转化为16进制数组 */
int  txt2hex( const char* string, int length,uint8_t* binary,int binsize )
{
	char* iptr = NULL;
	int len;
	uint8_t* bptr = NULL;
	int ret = -1;

	iptr = ( char* )string;
	len=0;
	while ( isxdigit( *iptr ) && len<length )
	{
		iptr++;
		len++;
	}
	len >>= 1;
	if ( len == 0 )
	{
		return -1;
	}

	if( binsize < len )
	{
		return -1;
	}

	bptr = binary;

	ret = len;
	iptr =( char* )string;
	while ( len > 0 )
	{
		*bptr++ = ( to_hex( iptr ) << 4 ) | ( to_hex( iptr + 1 ) );
		iptr+=2;
		len--;
	}
	return ret;
}

uint16_t swap_int16(uint16_t value)
{
	if(m_endtype ==LITTLE_END)
		return value;

	return ((value &0x00FF)<<8)|
		((value&0xFF00)>>8);
}

uint32_t swap_int32(uint16_t value)
{
	if(m_endtype ==LITTLE_END)
		return value;
	return ((value & 0x000000FF) << 24) |
		((value & 0x0000FF00) << 8)|
		((value & 0x00FF0000) >> 8)|
		((value&0xFF000000) >> 24);
}

int sw_getendtype()
{
	union w
	{
		int a;
		char b;
	}c;
	c.a = 1;
	if (c.b == 1)
	{
		printf("xiaoduan\n");
		m_endtype = LITTLE_END;
	}
	else
	{
		printf("daduan\n");
		m_endtype= BIG_END;

	}
	return 0;
}

/* 分析是不是一个有效的点分十进制IP地址 */
bool is_address(char* buf)
{
	int i, j, k;
	int sip;
	j = i = 0 ;
	k = 0;
	//to do 
	//    //IPv6合法性如何判断需要研究，这里如果检查buf有:认为是ipv6的地址，而且暂且当其是合法的
	if( strchr(buf,':') != 0 )
		return true;
	while (k < 4)
	{
		sip = 0;
		while (buf[i] != '.' && buf[i] != '\0')
		{
			if ( 2 < (i-j) || buf[i] < '0' || '9' < buf[i] )
				return false;
			sip = sip * 10 + buf[i] - '0';
			i++;
		}
		if ( j == i || sip > 255 )/* 没有数据.9..0或者大于255 */
			return false;
		i++;
		j = i;
		++k;
	}
	return (k == 4 && buf[i-1] == '\0' && buf[i-2] != '.') ? true : false;
}

































