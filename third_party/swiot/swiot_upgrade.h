/**
* @date:2018/8/14
*/
#ifndef __SWIOT_UPGRADE_H__
#define __SWIOT_UPGRADE_H__

/**
* @brief 升级
*
* @url 			升级地址
* @md5 			校验md5码 
* @curversion   当前升级的版本
* @strategy     当前升级的策略
* @return 		成功返回1，当已有升级任务时返回0
*/
int sw_upgrade(const char* url,const char* md5,const char* curversion,const char* strategy);

#endif //__SWIOT_UPGRADE_H__