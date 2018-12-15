/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include "esp_common.h"
#include "driver/key.h"

#define LONG_PRESS_TIME  3000 //ms

#define  GPIO_HIGH  1
#define  GPIO_LOW   0

os_timer_t key_noise_timer;//过滤超短干扰

volatile uint32 key_counter = 0;
volatile uint32 long_key = 0;

LOCAL void key_intr_handler(struct keys_param *keys);

/******************************************************************************
 * FunctionName : key_init_single
 * Description  : init single key's gpio and register function
 * Parameters   : uint8 gpio_id - which gpio to use
 *                uint32 gpio_name - gpio mux name
 *                uint32 gpio_func - gpio function
 *                key_function long_press - long press function, needed to install
 *                key_function short_press - short press function, needed to install
 * Returns      : single_key_param - single key parameter, needed by key init
*******************************************************************************/
struct single_key_param *
key_init_single(uint8 gpio_id, uint32 gpio_name, uint8 gpio_func, key_function long_press, key_function short_press)
{
    struct single_key_param *single_key = (struct single_key_param *)zalloc(sizeof(struct single_key_param));

    single_key->gpio_id = gpio_id;
    single_key->gpio_name = gpio_name;
    single_key->gpio_func = gpio_func;
    single_key->long_press = long_press;
    single_key->short_press = short_press;

    return single_key;
}

/******************************************************************************
 * FunctionName : key_init
 * Description  : init keys
 * Parameters   : key_param *keys - keys parameter, which inited by key_init_single
 * Returns      : none
*******************************************************************************/
void key_init(struct keys_param *keys)
{
    u32 i;
    GPIO_ConfigTypeDef *pGPIOConfig;

    pGPIOConfig = (GPIO_ConfigTypeDef *)zalloc(sizeof(GPIO_ConfigTypeDef));
    gpio_intr_handler_register(key_intr_handler, keys);

    for (i = 0; i < keys->key_num; i++) {
        keys->single_key[i]->key_level = 1;
        pGPIOConfig->GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
        pGPIOConfig->GPIO_Pullup = GPIO_PullUp_EN;
        pGPIOConfig->GPIO_Mode = GPIO_Mode_Input;
        pGPIOConfig->GPIO_Pin = (1 << keys->single_key[i]->gpio_id);//this is GPIO_Pin_13 for switch
        gpio_config(pGPIOConfig);
    }

    //enable gpio iterrupt
    _xt_isr_unmask(1 << ETS_GPIO_INUM);
}


/******************************************************************************
 * FunctionName : key_100ms_cb
 * Description  : 50ms timer callback to check it's a real key push
 * Parameters   : single_key_param *single_key - single key parameter
 * Returns      : none
*******************************************************************************/
LOCAL void key_100ms_cb(struct single_key_param *single_key)//一直等到释放
{
	key_counter++;
	
    if(GPIO_LOW == GPIO_INPUT_GET(GPIO_ID_PIN(13)))//zs
    {
		//printf(" low ,key_counter=%d\n",key_counter);

		if(key_counter >= 10*25) //3000 ms
		{
			//DDD(" Long press\n");
			
			if(single_key->long_press && long_key == 0)
	        {
				long_key = 0xAA;
	            single_key->long_press();
	        }	 
            
	        //等待释放 按键松开
	 	}
    }       
	else
	{
		//printf(" high ,key_counter=%d\n",key_counter);
		
		single_key->key_level = GPIO_HIGH;

		//key up
		os_timer_disarm(&single_key->key_100ms);
		
		long_key = 0;

		if(key_counter >= 8 && key_counter < 30) //80 ms  -- 300 ms
		{			
			//DDD(" short press\n");
			
			if(single_key->short_press)
	        {
	            single_key->short_press();
	        }
		}
		else
		{
			//DDD(" key noise or long key release \n");
		}
		
		key_counter = 0;		
		
		//下降沿  for next key pressed
		//ETS_GPIO_INTR_ENABLE();    
        //enable gpio iterrupt
        _xt_isr_unmask(1 << ETS_GPIO_INUM);
		gpio_pin_intr_state_set(GPIO_ID_PIN(single_key->gpio_id), GPIO_PIN_INTR_NEGEDGE);//继续等待下降沿
		
	}    
}

/******************************************************************************
 * FunctionName : key_intr_handler
 * Description  : key interrupt handler
 * Parameters   : key_param *keys - keys parameter, which inited by key_init_single
 * Returns      : none
*******************************************************************************/

LOCAL void key_intr_handler(struct keys_param *keys)
{
    uint8 i;
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

    for (i = 0; i < keys->key_num; i++) 
    {
        if (gpio_status & BIT(keys->single_key[i]->gpio_id)) 
        {
            //printf(" intr 1...\n");
            
        	key_counter=0;
            
            //disable this gpio pin interrupt
            gpio_pin_intr_state_set(GPIO_ID_PIN(keys->single_key[i]->gpio_id), GPIO_PIN_INTR_DISABLE);
            
            //clear interrupt status
            GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(keys->single_key[i]->gpio_id));

            if(keys->single_key[i]->key_level == GPIO_HIGH)//
            {                    
                os_timer_disarm(&keys->single_key[i]->key_100ms);
                os_timer_setfn(&keys->single_key[i]->key_100ms, (os_timer_func_t *)key_100ms_cb, keys->single_key[i]);
                os_timer_arm(&keys->single_key[i]->key_100ms, 10, 1);
            }     
            else
            {
			    //DDD(" intr 3...\n");
            }
        }
    }
}

