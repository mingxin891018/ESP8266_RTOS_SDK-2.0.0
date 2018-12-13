#ifndef _DEBUGLOG_H
#define _DEBUGLOG_H

#define	LOG_ERROR 	 	1
#define	LOG_WARN 	 	2
#define	LOG_INFO 	 	3
#define	LOG_DEBUG 	 	4
#define	LOG_NONE 	 	5

#ifndef LOG_LEVEL 
#define LOG_LEVEL       LOG_ERROR
#endif

#define SWDEBUG 		printf


//#define SW_LOG_INFO(format, args...) (printf (  "[I/%s %s %d] " format, __FILE__, __func__ , __LINE__, ##args))
   
//#define SW_LOG_DEBUG(format, args...) (printf(  "[D/%s %s %d] " format, __FILE__, __func__ , __LINE__, ##args))
    
//#define SW_LOG_WARN(format, args...) (printf (  "[W/%s %s %d] " format, __FILE__, __func__ , __LINE__, ##args))

//#define SW_LOG_ERROR(format, args...) (printf(  "[E/%s %s %d] " format, __FILE__, __func__, __LINE__, ##args))


#define SW_LOG_ERROR(format,...)           do {  \
	if(LOG_LEVEL <= LOG_ERROR) {  \
		SWDEBUG("[E/%s %s %d] "format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
	}  \
} while(0);  
#define SW_LOG_WARN(format, ...)           do {  \
	if(LOG_LEVEL <= LOG_WARN) {  \
		SWDEBUG("[W/%s %s %d] "format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
	}  \
} while(0);

#define SW_LOG_INFO(format, ...)           do {  \
	if(LOG_LEVEL <= LOG_INFO) {  \
		SWDEBUG("[I/%s %s %d] "format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
	}  \
} while(0);

#define SW_LOG_DEBUG(format, ...)          do {  \
	if(LOG_LEVEL <= LOG_DEBUG) {  \
		SWDEBUG("[D/%s %s %d] "format,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);  \
	}  \
} while(0);







#endif
