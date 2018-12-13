/**
* @date 	: 2018/6/19 19:13
* @author	: zhoutengbo
*/

#ifndef __SWIOT_DEVICE_H__
#define __SWIOT_DEVICE_H__

#define ARR_LEN(ary) (sizeof(ary))/(sizeof(ary[0]))

/* The fomat of topic */
#define TOPIC_COMMON_FMT           "/sys/%s/%s/thing/%s/%s"

/* The fomat of topic */
#define TOPIC_COMMON_EXT_FMT       "/sys/%s/%s/thing/%s/%s/%s"

/* The fomat of topic */
#define TOPIC_COMMON_EXT_EXT_FMT   "/sys/%s/%s/thing/%s"

/* The fomat of topic*/
#define TOPIC_COMMON_EXT_EXT_EXT_FMT "/sys/%s/%s/%s/%s/%s"

/**
* @brief topic fmt struct
*/
typedef struct 
{
	char topic[80];
	char* fmt;			/*topic fmt*/
	char* agr1;			
	char* agr2;
	char* agr3;
}swiot_topic_fmt_t,*swiot_topic_fmt_pt;

/*subscribe*/
#define TOPIC_SUBSCRIBE_TYPE 	(1)

/*unsubscribe*/
#define TOPIC_UNSUBSCRIBE_TYPE  (2)

/*topic length*/
#define TOPIC_FILTER_LEN        (64)

/*reply data length*/
#define TOPIC_REPLY_DATA_LEN    (32)

/*reply list length*/
#define TOPIC_REPLY_LIST_LEN    (20)

/*success reply code*/
#define REPLY_CODE_OK			(200)

/*no upgrade*/
#define REPLY_CHECK_SAME		(800)

/*wait for reply code*/
#define REPLY_CODE_WAIT         (0)

/*init reply code*/
#define REPLY_CODE_INIT         (-1)

/*max wait for reply times*/
#define MAX_WAIT_REPLY_TIMES    (10)
/**
* @brief wait for reply
*/
typedef struct 
{
	int  id;								/*message id*/
	int  reply_code;						/*the return code*/
	char data[TOPIC_REPLY_DATA_LEN];		/*the return data*/
}swiot_wait_reply_t,*swiot_wait_reply_pt;

/**
* @brief mqtt连接参数
*/
typedef struct 
{
	/*服务器端口*/	
	int   port;	
	/*服务器地址*/
	const char* host;
	/*客户端id*/
	const char* client_id;
	/*用户名*/
	const char* username;
	/*密码*/
	const char* password;

	/**
	*如果此字段为空，则表示普通的tcp连接，否则为ssl连接
	*/
	const char* pub_key;

	/*读缓存*/
	char*  pread_buf;
	/*读缓存大小*/
	int    pread_bufsize;
	/*写缓存*/
	char*  pwrite_buf;
	/*写缓存大小*/
	int    pwrite_bufsize;
	/*超时时间*/
	unsigned int command_timeout_ms;
}swiot_device_param_t,*swiot_device_param_pt;

/**
* @brief 交互类型
*/
typedef enum 
{
	/**
	* 物模型中的set方法调用，下同
	*/
	SWIOT_SERVICE_SET = 0,
	SWIOT_SERVICE_GET,
	/**
	* 当请求类型为SWIOT_SERVICE_COM时，扩展字段exstr将指明请求的服务id号
	*/
	SWIOT_SERVICE_COM, 

	/**
	* 禁用设备
	*/
	SWIOT_CONTROL_DISABLE,
	/**
	* 启用设备
	*/
	SWIOT_CONTROL_ENABLE,

	/**
	* 删除设备
	*/
	SWIOT_CONTROL_DELETE,

	/**
	* 设备升级
	*/
	SWIOT_CONTROL_UPGRADE,

	/**
	* 固件验证
	*/
	SWIOT_CONTROL_VALIDATE

}swiot_interact_type_e;

/**
* @brief 系统将解析所有形如/sys/${productkey}/${devicename}/+的topic,并回调此函数
*
* @handle 			句柄
* @topic  			topic去除/sys/${productkey}/${devicename}后余下的部分
* @message_id  	 	消息id
* @paramstr       	负载json中的params字段所包含的值,字符串
* @exstr            扩展字段
*
* @return NULL
*/
typedef void (*system_request_callback)( swiot_interact_type_e type,
					int message_id,
					char* paramstr,
					char* exstr);


/**
* 	@brief 建立直连设备mqtt通道
*
* 	@param 建立连接所需的参数
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Construct( swiot_device_param_pt param );

/**
*   @brief 	销毁直连设备mqtt通道
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Destroy( void );

/**
*   @brief 	设备登陆
* 
*	@secret 登陆密钥
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Login( const char* secret );

/**
*	@brief 	设备登出
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Logout( void );

/**
*	@brief 		定时器
*
*	@timeout 	超时时间
*
*	@return 0 成功,-1 失败
*/
int SWIOT_Device_Yeild( int timeout );

/**
* 	@brief 注册topic处理回调函数
*				平台下发的系统级topic将由此回调函数处理
* 
*   @callback 回调函数地址
*   
*   @return   0 成功,-1 失败
*/
int SWIOT_Device_System_Register( system_request_callback callback );

/**
*   @brief 回复系统请求
*
*   @type  			请求/回复类型
*   @message_id		消息id
*   @code           回复码
*   @paramstr       回复消息体中的param字段值
*   @exstr          扩展字段
*/
int SWIOT_Device_System_Response( swiot_interact_type_e type,
								  int message_id,
								  int code,
								  const char* paramstr,
								  const char* exstr);

/**
*  @brief 属性上报
*
*  @property        负载属性信息的json体
*  @return          0 成功,-1 失败
*/
int SWIOT_Device_Property_Post( const char* property );

/**
*  @brief 事件上报
* 
*  @event          负载事件信息的json体
*  @event_id       事件id
*
*  @return         0 成功,-1 失败
*/
int SWIOT_Device_Event_Post( const char* event,
							 const char* event_id );

/**
*  @brief 设备信息上报
*
*  @version        	 设备版本号
*  @mac            	 设备mac地址
*  @manufacturer  	 设备厂商
*  @hardware_version 硬件厂商
*
*  @return           0 成功，-1失败
*/
int SWIOT_Device_Check(const char* version,
        			   const char* mac,
        			   const char* manufacturer,
        			   const char* hardware_version);

/**
*  @brief 升级进度上报
* 
*  @firmware    	  当前升级版本的md5号
*  @curversion        当前升级的md5号
*  @step              升级进度
*  @desc              描述字段
*  @strategy          升级策略
*/
int SWIOT_Device_Upgrade_Progress( const char* firmware,
        						   const char* curversion,
        						   const char* step,
        						   const char* desc,
        						   const char* strategy);



#endif //__SWIOT_DEVICE_H__