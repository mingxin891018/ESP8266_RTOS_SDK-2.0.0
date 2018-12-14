#ifndef _SW_PARAMETER_H_
#define _SW_PARAMETER_H_

#define PARAMETER_MAX_LEN 128

bool sw_parameter_init();
bool sw_parameter_set(char *name, char *value, unsigned int value_length);
bool sw_parameter_get(char *name, char *value, unsigned int value_length);
bool sw_parameter_get_int(char *name, int *value);
bool sw_parameter_set_int(char*name, int value);
bool sw_parameter_delete(char *name);
bool sw_parameter_clean();
bool sw_parameter_save();
void sw_printf_param();
void sw_param_free();

#endif
