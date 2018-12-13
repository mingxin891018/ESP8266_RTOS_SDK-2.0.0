#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/netif.h"
#include "esp_sta.h"

#define	DEMO_AP_SSID "smart-test"
#define	DEMO_AP_PASSWORD "zhao12345678"

static struct station_config config = {0};

void set_ap_info_to_connect(void)
{
	memset(&config, 0, sizeof(struct station_config));
	strncpy((char *)config.ssid ,(const char *)DEMO_AP_SSID, sizeof(DEMO_AP_SSID));
	strncpy((char *)config.password, (const char *)DEMO_AP_PASSWORD, sizeof(DEMO_AP_PASSWORD));
	
	wifi_station_set_config(&config);
	wifi_station_connect();
}


