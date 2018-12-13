#ifndef _SW_COMMON_H_
#define _SW_COMMON_H_

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
