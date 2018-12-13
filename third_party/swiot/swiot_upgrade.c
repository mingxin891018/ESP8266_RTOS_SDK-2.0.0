#include <esp_system.h>
#include <upgrade.h>
#include "swiot_upgrade.h"
#include "sw_download.h"
#include "swiot_common.h"
#include "swiot_sdk.h"
#include "freertos/task.h"
#include "fota_crc32.h"

#define MALLOC_SIZE 256

/**
* 升级状态
*/
typedef enum 
{
	/**
	* 	当处于此状态时，可以接收外界升级请求
	*/
	UPGRADE_NULL,
	/**
	*	在此状态下完成固件下载的工作 
	*/
	UPGRADE_START,
	/**
	*   固件下载已完成，升级收尾工作
	*/
}sw_upgrade_status_e;

typedef struct 
{
	sw_upgrade_status_e upgrade_status;	/*升级状态*/
	xTaskHandle*	ota_task_handle;
	int length;							/*下载长度*/
	char md5[64];						/*md5码*/
	char curversion[32];				/*当前升级的版本*/
	char strategy[32];					/*升级策略*/
}sw_upgrade_handle_t;



static sw_upgrade_handle_t g_upgrade_t;		/*升级模块句柄*/

void upgrade_ontimer(void *pvParameters)
{
	while(UPGRADE_START == g_upgrade_t.upgrade_status ) {
		sw_on_time();
	}
	vTaskDelete(NULL);
}

LOCAL void upgrade_recycle(void)
{
    system_upgrade_deinit();
    if (system_upgrade_flag_check() == UPGRADE_FLAG_FINISH) {
        system_upgrade_reboot(); // if need
    }

}

void sw_upgrade_progress_func(int progress, char* download_filename)
{
	int rc = 0;	
	int report_progress = 0;
	char upgrade_cmd[256] = {0};

	if (0 > progress){
		report_progress = progress;
		g_upgrade_t.upgrade_status = UPGRADE_NULL;
		system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
		log_error("upgrade error\n");
		goto UPGRADE_RECYCLE;
	} else if (100 == progress){
		g_upgrade_t.upgrade_status = UPGRADE_NULL;

		if (upgrade_crc_check(system_get_fw_start_sec(), g_upgrade_t.length) != true) {
                log_error("upgrade crc check failed !\n");
                report_progress = -1;
                system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
                goto UPGRADE_RECYCLE;
            }

		report_progress=100;
		system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
		goto UPGRADE_RECYCLE;
		
	} else
	{
		report_progress = progress;
		SWIOT_SDK_PostUpgress(report_progress,g_upgrade_t.md5,g_upgrade_t.curversion,g_upgrade_t.strategy);
	}
END:
	return;
UPGRADE_RECYCLE:
	SWIOT_SDK_PostUpgress(report_progress,g_upgrade_t.md5,g_upgrade_t.curversion,g_upgrade_t.strategy);
	upgrade_recycle();
	return;
}

int sw_download_func(char* buf,int buf_size,bool is_length)
{
	if(is_length)
		g_upgrade_t.length = buf_size;
	log_info("upgrade in buf_size:%d\n",buf_size);
	if (false == system_upgrade(buf,buf_size)) {
		g_upgrade_t.upgrade_status = UPGRADE_NULL;
		system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
		SWIOT_SDK_PostUpgress(-1,g_upgrade_t.md5,g_upgrade_t.curversion,g_upgrade_t.strategy);
		log_error("upgrade error!\n");
		return -1;
	}
	log_info("upgrade out\n");
	return 0;
}


/**
* @brief 升级
*
* @url 			升级地址
* @md5 			校验md5码 
* @curversion   当前升级的版本
* @strategy     当前升级的策略
* @return 		成功返回0，当已有升级任务时返回-1
*/
int sw_upgrade(const char* url,const char* md5,const char* curversion,const char* strategy)
{
	int ret = -1;

	char* trip_url;//[1024] = {0};
	char* download_url;//[1024] = {0};
	char bin1_path[64] = {0};
	char bin2_path[64] = {0};

	if (!url || !md5 || !curversion || !strategy)
		goto END;

	if (UPGRADE_NULL != g_upgrade_t.upgrade_status ) {
		log_error("upgrading , add url=%s md5=%s error\n",url,md5);
		goto END;
	}

	trip_url = (char*)malloc(MALLOC_SIZE);
	if (NULL == trip_url) {
		log_error("malloc error\n");
		goto END;
	}
	memset(trip_url,0,MALLOC_SIZE);

	download_url = (char*)malloc(MALLOC_SIZE);
	if (NULL == download_url) {
		log_error("malloc error\n");
		goto END;
	}
	memset(download_url,0,MALLOC_SIZE);

	url_trip_params((char*)url,trip_url,MALLOC_SIZE);
	log_debug("trip_url : %s\n",trip_url);

	if (NULL == url_get_param((char*)url,"bin1",bin1_path,sizeof(bin1_path))) {
		log_error("get params \"bin1\" error,url=%s\n",url);
		goto END;
	}
	log_debug("bin1_path : %s\n",bin1_path);

	if (NULL == url_get_param((char*)url,"bin2",bin2_path,sizeof(bin2_path))) {
		log_error("get params \"bin2\" error,url=%s\n",url);
		goto END;
	}
	log_debug("bin2_path : %s\n",bin2_path);

    if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
            snprintf(download_url,MALLOC_SIZE,"%s/%s",trip_url,bin2_path);
     } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
      		snprintf(download_url,MALLOC_SIZE,"%s/%s",trip_url,bin1_path);
    }

	if (0 != sw_start_download((char*)download_url,sw_upgrade_progress_func,sw_download_func,60*1000)) {
		log_error("start download error,url=%s\n",download_url);
		goto END;
	}

	g_upgrade_t.upgrade_status = UPGRADE_START;
    system_upgrade_flag_set(UPGRADE_FLAG_START);
    system_upgrade_init();
    memcpy(g_upgrade_t.md5,md5,sizeof(g_upgrade_t.md5));
	strncpy(g_upgrade_t.curversion,curversion,sizeof(g_upgrade_t.curversion));
	strncpy(g_upgrade_t.strategy,strategy,sizeof(g_upgrade_t.strategy));

	//create thread
	xTaskCreate(upgrade_ontimer, "upgrade_download",1024, NULL, 2, NULL);
	ret = 0;
END:
	if (trip_url)
		free(trip_url);
	if (download_url)
		free(download_url);
	return ret;
}