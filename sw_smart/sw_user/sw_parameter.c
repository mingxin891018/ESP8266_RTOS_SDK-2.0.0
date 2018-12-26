#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "esp_spiffs.h"
#include "spiffs_test_params.h"
#include "sw_common.h"
#include "sw_debug_log.h"

#define SW_PARAM_FILE_NAME "param.txt"
#define SW_PARAM_TOTAL_LENGTH  2560
#define SW_PARAM_SIGNAL_LENGTH 63
#define SW_PARAM_TOTAL_NUMBER  20
#define SW_PARAM_LINE_LENGTH 132 

static char *param = NULL;

//初始化文件系统
static void spiffs_fs1_init(void)
{
    struct esp_spiffs_config config;

    config.phys_size = FS1_FLASH_SIZE;
    config.phys_addr = FS1_FLASH_ADDR;
    config.phys_erase_block = SECTOR_SIZE;
    config.log_block_size = LOG_BLOCK;
    config.log_page_size = LOG_PAGE;
    config.fd_buf_size = FD_BUF_SIZE * 2;
    config.cache_buf_size = CACHE_BUF_SIZE;

    esp_spiffs_init(&config);
}

void sw_printf_param()
{
    SW_LOG_DEBUG("param[0x%p]:\n%s", param, param);
}

static bool param_check(char *name)
{
	int name_length = strlen(name);
	if(name == NULL){
		SW_LOG_ERROR("param is null");
		return false;
	}

	if(name_length > SW_PARAM_SIGNAL_LENGTH || name_length <= 0){
		SW_LOG_ERROR("param langth error!,can not set or get");
		return false;
	}
	
	if(param == NULL){
		SW_LOG_ERROR("param system is not init,can not set or get");
		return false;
	}

	if(strstr(name, "=") || strstr(name, "\n")){
		SW_LOG_ERROR("param can not include \"=\" or \"\n\"");
		return false;
	}
	return true;
}

static char *find_string_from_param(char *str)
{
    char *p = param, *q = NULL;
    int ret = -1, tmp = 0, tmp_length  = 0, str_length = 0;

	if(param == NULL || str == NULL){
        SW_LOG_ERROR("find string error!!!");
        return NULL;
    }
	
	str_length = strlen(str);
    while(tmp  <= SW_PARAM_TOTAL_LENGTH - str_length){
        ret = xstrncasecmp(p + tmp, str, str_length);
        if(ret == 0){
			q = strstr(p+tmp, "=");
			tmp_length = q - (p + tmp);
			if(tmp == 0 || (*(p +tmp -1) == '\n' && str_length == tmp_length) )
				return p + tmp;
		}
        tmp++;
    }
    return NULL;
}

static bool create_empty_file(char *file_name,char *buf, size_t length)
{
    size_t total = length, ret = 0;
    int fd = -1;

    if(file_name == NULL || buf == NULL || length <= 0)
        return false;
	
	remove(file_name);
    fd = open(SW_PARAM_FILE_NAME, O_TRUNC | O_CREAT | O_RDWR, 0666);
    if(fd <= 3)
        return false;
    while(total > 0){
        ret = write(fd, buf + ret, total);
        total -= ret;
    }
    close(fd);
    fd = -1;
    return true;
}

static bool check_param_buf(char *m)
{
	int tmp = 0, tmp_1 = 0, tmp_2 = 0;
	char *p = NULL, *q = NULL;
	
	while( 1 ){
		p = strstr(m, "=");
		if(!p)
			break;
		q = strstr(p+1, "\n");
		if(!q)
			break;
		m = q + 1;
		tmp ++;
	}

	m = param;
	p = NULL;
	while(1){
		p = strstr(m, "=");
		if(!p) 
			break;
		m = p + 1;
		tmp_1 ++;
	}

	m = param;
	p = NULL;
	while(1){
		p = strstr(m, "\n");
		if(!p) 
			break;
		m = p + 1;
		tmp_2 ++;
	}
	
	SW_LOG_DEBUG("tmp=%d,tmp_1=%d,tmp_2=%d", tmp, tmp_1, tmp_2);
	if(tmp != 20 || tmp_1 != 20 || tmp_2 != 20)
		return false;
	else
		SW_LOG_DEBUG("check param file success!");
    return true;

}

static bool load_file_to_buf(int fd)
{
	int file_length = 0, ret = 0;

	if(param == NULL){
		SW_LOG_ERROR("param system int init!");
		return false;
	}
    file_length = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	
	SW_LOG_DEBUG("param file length=%d",file_length);
	if(file_length > SW_PARAM_TOTAL_LENGTH)
		return false;

	while(file_length > 0){
        ret = read(fd, param + ret, file_length);
        file_length -= ret;
    }
	
	return check_param_buf(param);
}

bool sw_parameter_init()
{
    char *param_buf = NULL;
    off_t file_length = 0;
	int fd = -1, i = 0, ret = 0, total = 0;

	if(param != NULL){
		SW_LOG_DEBUG("param system have already init!");
		return false;
	}

    spiffs_fs1_init();
    if(NULL == (param_buf = (char *)malloc(SW_PARAM_TOTAL_LENGTH))){
        SW_LOG_ERROR("init param failed");
        return false;
    }
	
    memset(param_buf, 0, SW_PARAM_TOTAL_LENGTH);
    for(i = 0; i < SW_PARAM_TOTAL_NUMBER; i++)
        total += snprintf(param_buf + total, SW_PARAM_TOTAL_LENGTH - total, "%s", "null=null\n");
    param = param_buf;
    
	i = 3;
    while(i--){
        fd = open(SW_PARAM_FILE_NAME, O_RDWR, 0666);
        if(fd > 3)
            break;
    }
    if(fd <= 3){
    	total = 0;
		memset(param_buf, 0, SW_PARAM_TOTAL_LENGTH);
    	for(i = 0; i < SW_PARAM_TOTAL_NUMBER; i++)
    	    total += snprintf(param_buf + total, SW_PARAM_TOTAL_LENGTH - total, "%s", "null=null\n");
        create_empty_file(SW_PARAM_FILE_NAME, param_buf, strlen(param_buf));
    	SW_LOG_ERROR("can not find param file!,create null param file\n");
		return true;
	}

    if( !load_file_to_buf(fd) ){
    	total = 0;
    	memset(param_buf, 0, SW_PARAM_TOTAL_LENGTH);
    	for(i = 0; i < SW_PARAM_TOTAL_NUMBER; i++)
        	total += snprintf(param_buf + total, SW_PARAM_TOTAL_LENGTH - total, "%s", "null=null\n");
        create_empty_file(SW_PARAM_FILE_NAME, param_buf, strlen(param_buf));
        SW_LOG_ERROR("check param file is error,create param file!!!\n");
    }
    close(fd);
	
	SW_LOG_DEBUG("init param ok");
    return true;

}

bool sw_parameter_set(char *name, char *value, int value_length)
{
    char *tmp_buf = NULL;
    char *tail = NULL;
    char *is_param_string = NULL, *is_null_string = NULL;
    char param_set[SW_PARAM_LINE_LENGTH] = {0};
    char old_param[SW_PARAM_LINE_LENGTH] = {0};
    int name_length = strlen(name);

    int tail_length = 0;

	if(!param_check(name))
		return false;
	int len = strlen(value);
	if(len > SW_PARAM_SIGNAL_LENGTH)
		return false;
    if(value == NULL || value_length <= 0 || value_length > SW_PARAM_SIGNAL_LENGTH){
        SW_LOG_ERROR("set param[%s] error,value is null", name);
		return false;
	}

	is_param_string = find_string_from_param(name);
    if( (NULL != is_param_string) && (NULL != xstrgetval(is_param_string, name, old_param, SW_PARAM_LINE_LENGTH)) ){
        tail = strstr(is_param_string,"\n") + 1;
        tail_length = strlen(tail);
		if(!strcmp(value, old_param) && strlen(value) == strlen(old_param)){
			SW_LOG_INFO("param \"%s\" is not changed,not neet set!",name);
			return true;
		}
	}
    else if( NULL != (is_null_string = find_string_from_param("null")) ){
        tail = is_null_string + strlen("null=null\n");
        tail_length = strlen(tail);
    }
    else{
		SW_LOG_DEBUG("param buff is full!,can not set");
        goto SET_ERROR;
	}
    if(NULL == (tmp_buf = (char *)malloc(tail_length+1)))
        goto SET_ERROR;
    
	memset(param_set, 0, sizeof(param_set));
    memset(tmp_buf, 0, tail_length+1);
	
    strncpy(tmp_buf, tail, tail_length);
    
	if(is_null_string)
		memset(is_null_string, 0, strlen(is_null_string));
	else if(is_param_string)
		memset(is_param_string, 0, strlen(is_param_string));
	else
		goto SET_ERROR;

    strncpy(param_set, name, name_length);
    strncpy(param_set + name_length, "=", 1);
    strncpy(param_set + name_length + 1, value, value_length);
    strncpy(param_set + name_length + 1 + value_length, "\n", 1);
    
	strncpy(param_set + name_length + 1 + value_length + 1, "\0", 1);

    if(*param == '\0')
        strncpy(param, param_set, strlen(param_set));
    else
        strcat(param, param_set);
    strcat(param, tmp_buf);

	SW_LOG_DEBUG("set param[%s] ok",name);
    return true;

SET_ERROR:
	if(tmp_buf){
        free(tmp_buf);
        tmp_buf = NULL;
    }
    return false;

}

bool sw_parameter_get(char *name, char *value, int value_length)
{
    char *is_param = NULL;
    int name_length = strlen(name);
   	
	if(value == NULL || value_length <= 0){
        SW_LOG_ERROR("param value is error!\n");
        return false;
    }
	memset(value, 0, value_length);

	if(value_length < SW_PARAM_SIGNAL_LENGTH){
        SW_LOG_ERROR("param value length is error!\n");
        return false;
	}
    
	if(!param_check(name))
		return false;
	
	if(NULL == (is_param = find_string_from_param(name)) || (NULL == xstrgetval(is_param, name, value, value_length))){
        SW_LOG_DEBUG("can not find param [%s]",name);
    	return false;
	}
    else
		SW_LOG_DEBUG("find param[%s] ok",name);
	
	return true;
}

bool sw_parameter_get_int(char *name, int *value)
{
	char buf[128] = {0};

	memset(buf, 0, sizeof(buf));
	if(!sw_parameter_get(name, buf, sizeof(buf)))
		return false;
	*value = atoi(buf);
	return true;
}

bool sw_parameter_set_int(char*name, int value)
{
	char buf[128] = {0};

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", value);
	if(!sw_parameter_set(name, buf, strlen(buf)))
		return false;
	return true;
}

bool sw_parameter_delete(char *name)
{
    int len = -1,tail_length = -1;
    char *is_param = NULL;
    char *tail = NULL;
    char *tmp_buf = NULL;

    if(!param_check(name))
		return false;

	if(NULL == (is_param = find_string_from_param(name)))
        return true;
    tail = strstr(is_param, "\n") +1;
    tail_length = strlen(tail);

    if(NULL == (tmp_buf = (char *)malloc(tail_length+1)))
        return false;
    
	memset(tmp_buf, 0, tail_length+1);
    strncpy(tmp_buf, tail, tail_length);
	memset(is_param, 0 ,strlen(is_param));
	
	if(strlen(param))
		strcat(param, tmp_buf);
	else
		strncpy(param, tmp_buf, tail_length);
	strcat(param, "null=null\n");
	
	SW_LOG_DEBUG("delete param[%s] success!",name);
	
	return true;
}

bool sw_parameter_save()
{
    int fd = -1, total = 0, ret = 0;
    if(param == NULL){
        SW_LOG_ERROR("param system init error,can not save param");
        return false;
    }
    if( (fd = open(SW_PARAM_FILE_NAME, O_TRUNC | O_CREAT | O_RDWR, 0666)) <= 3){
        SW_LOG_ERROR("open %s failed,can not save param!!!",SW_PARAM_FILE_NAME);
        return false;
    }
	
	if(!check_param_buf(param))
		return false;
    total = strlen(param);
    SW_LOG_DEBUG("param length=%d",total);
	while(total > 0){
        ret = write(fd, param + ret, total);
        total -= ret;
    }

	close(fd);
    fd = -1;
	
	SW_LOG_DEBUG("save param to file ok!");
	return true;
}

bool sw_parameter_clean()
{
	int total = 0, i = 0;
	if(param == NULL){
		SW_LOG_ERROR("param system is not init,can not clean");
		return false;
	}
	for(i = 0; i < SW_PARAM_TOTAL_NUMBER; i++)
        total += snprintf(param + total, SW_PARAM_TOTAL_LENGTH - total, "%s", "null=null\n");
	SW_LOG_ERROR("param clean success");
	return true;
}

void sw_param_free()
{
    if(param)
        free(param);
}



