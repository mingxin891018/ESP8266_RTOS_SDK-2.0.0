/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


//wifi状态
typedef enum
{
	MODE_PROD_TEST_FAIL = 1,
	MODE_PROD_TEST_SUCC,
	MODE_SMART_CONFIG,
	MODE_SOFTAP_CONFIG,
	MODE_CONNECTED_ROUTER,
	MODE_DISCONNECTED_ROUTER,
	MODE_MAX,
}MODE_STATE;

//设备状态
typedef struct 
{
	MODE_STATE mode;
	bool  is_connected_server;//0 on 1 yes
	char  state;//0 start 1 ok  2 fail

	char  powerSwitch;//0 关 1开
	int   countDown;//0~24*60
	char  timerTaskStr[4*2*24+1];//197
	int  save_flag;
}smart_dev_state_t;

//设备信息
typedef struct
{
	char device_name[32];//MAC
	char device_secret[32];//deviceSecret即是mqtt password

	char mqtt_username[64];//mqtt user   mqtt id跟username保持一致
	char mqtt_host[32];
	int  mqtt_port;
	int  save_flag;
	int  channel_flag;//1 ZJDX  2 DXJT 3 SCDX (mofang)
}smart_dev_info_t;

//一个智能设备
typedef struct
{
	smart_dev_state_t dev_state;
	smart_dev_info_t  dev_para;
}smart_dev_t;


#endif

