#ifndef __SW_DOWNLOAD_H__
#define __SW_DOWNLOAD_H__

#include <stdio.h>
#include <string.h>
#include "http_url.h"
typedef void (*sw_upgrade_progress_handle)(int progress, char *download_filename);
typedef int (*sw_download_handle)(char* buf,int buf_size,bool is_length);
int sw_start_download(char *download_url, sw_upgrade_progress_handle callback_handle,sw_download_handle download_handle, int over_time);
int sw_on_time();
void sw_stop_download();

#endif 