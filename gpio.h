/*
 * gpio.h
 *
 *  Created on: May 13, 2018
 *      Author: root
 */

#ifndef GPIO_H_
#define GPIO_H_
#include "main.h"

typedef enum{
	TX_AUX =0,		//发结束触发
	RX_AUX,
	CFG_AUX
}AUX_Trig_t;

enum{
	GPIO_IN =0,
	GPIO_OUT =1
};

enum{
	GPIO_L =0,
	GPIO_H=1
};

enum{
	NONE= 0,
	RISING= 1,
	FALLING =2,
	BOTH_EDGE=3,
};

extern int e32_aux_gpio_input_fd;
extern int e43_aux_gpio_input_fd;

#define SYSFS_GPIO_EXPORT        "/sys/class/gpio/export"

/********************************************************
 *				E32中速模块
 *******************************************************/
//2018-07-18 E32 AUX输入控制引脚 : USB_OTG1_PWR_CORE9_GPIO1_IO4
#define M_E32_AUX_GPIO_PIN       "4"
#define M_E32_AUX_GPIO_PIN_NUM    4
#define M_E32_AUX_GPIO_DIR        "/sys/class/gpio/gpio4/direction"
#define M_E32_AUX_GPIO_VAL        	"/sys/class/gpio/gpio4/value"

//E32 M0/M1采用同一控制引脚
//E32 M0输出控制引脚:SNVS-TEMPER4,GPIO5_IO04

#define M_E32_M0_GPIO_PIN       "132"  //122(无线小模块) ////"110"			//"132"
#define M_E32_M0_GPIO_PIN_NUM   132   //122   // 110			//132
#define M_E32_M0_GPIO_DIR       "/sys/class/gpio/gpio132/direction"
#define M_E32_M0_GPIO_VAL       "/sys/class/gpio/gpio132/value"

/********************************************************
 *				E32低速模块
 *******************************************************/
//2018-07-18 E32 AUX输入控制引脚 : USB_OTG1_PWR_CORE9_GPIO1_IO4
#define L_E32_AUX_GPIO_PIN       "47"
#define L_E32_AUX_GPIO_PIN_NUM    47
#define L_E32_AUX_GPIO_DIR        "/sys/class/gpio/gpio47/direction"
#define L_E32_AUX_GPIO_VAL        "/sys/class/gpio/gpio47/value"

//E32 M0/M1采用同一控制引脚
//E32 M0输出控制引脚:SNVS-TEMPER4,GPIO5_IO04

#define L_E32_M0_GPIO_PIN       "46"  //122(无线小模块) ////"110"			//"132"
#define L_E32_M0_GPIO_PIN_NUM   46   //122   // 110			//132
#define L_E32_M0_GPIO_DIR       "/sys/class/gpio/gpio46/direction"
#define L_E32_M0_GPIO_VAL       "/sys/class/gpio/gpio46/value"

/********************************************************
 *				E43模块
 *******************************************************/

//E43 AUX输入控制引脚 : NAND_CE1_GPIO4_IO14
#define E43_AUX_GPIO_PIN       	"110"
#define E43_AUX_GPIO_PIN_NUM     110
#define E43_AUX_GPIO_DIR      	"/sys/class/gpio/gpio110/direction"
#define E43_AUX_GPIO_VAL       	"/sys/class/gpio/gpio110/value"


//E43 M1输出控制引脚:SNVS-TEMPER1,gpio5_io1
#define E43_M1_GPIO_PIN         "129"
#define E43_M1_GPIO_PIN_NUM      129
#define E43_M1_GPIO_DIR         "/sys/class/gpio/gpio129/direction"
#define E43_M1_GPIO_VAL       	"/sys/class/gpio/gpio129/value"



//E43 M0输出控制引脚:SNVS-TEMPER8, gpio5_io8
#define E43_M0_GPIO_PIN         "136"
#define E43_M0_GPIO_PIN_NUM      136
#define E43_M0_GPIO_DIR         "/sys/class/gpio/gpio136/direction"
#define E43_M0_GPIO_VAL       	"/sys/class/gpio/gpio136/value"



//E43 AUX输入控制引脚 : USB_OTG1_PWR, gpio1_io4
//#define E43_AUX_GPIO_PIN       	"4"
//#define E43_AUX_GPIO_PIN_NUM     4
//#define E43_AUX_GPIO_DIR      	"/sys/class/gpio/gpio4/direction"
//#define E43_AUX_GPIO_VAL       	"/sys/class/gpio/gpio4/value"



//E43 M1输出控制引脚:SNVS-TEMPER8,gpio5_io8
//#define E43_M1_GPIO_PIN         "136"
//#define E43_M1_GPIO_PIN_NUM      136
//#define E43_M1_GPIO_DIR         "/sys/class/gpio/gpio136/direction"
//#define E43_M1_GPIO_VAL       	"/sys/class/gpio/gpio136/value"

//E43 M0输出控制引脚:SNVS-TEMPER1, gpio5_io1
//#define E43_M0_GPIO_PIN         "129"
//#define E43_M0_GPIO_PIN_NUM      129
//#define E43_M0_GPIO_DIR         "/sys/class/gpio/gpio129/direction"
//#define E43_M0_GPIO_VAL       	"/sys/class/gpio/gpio129/value"


/********************************************************
 *				L01+模块
 *******************************************************/
//L01 IRQ引脚
#define L01_IRQ_PIN         	"122"
#define L01_IRQ_PIN_NUM     	122
#define L01_IRQ_PIN_DIR         "/sys/class/gpio/gpio122/direction"
#define L01_IRQ_PIN_VAL       	"/sys/class/gpio/gpio122/value"

//CE芯片片选引脚
#define L01_CE_PIN         		"121"
#define L01_CE_PIN_NUM     		121
#define L01_CE_PIN_DIR         "/sys/class/gpio/gpio121/direction"
#define L01_CE_PIN_VAL       	"/sys/class/gpio/gpio121/value"

//CS SPI2接口片选引脚---GPIO4_IO22
#define L01_CSN_PIN         	"118"
#define L01_CSN_PIN_NUM     	118
#define L01_CSN_PIN_DIR         "/sys/class/gpio/gpio118/direction"
#define L01_CSN_PIN_VAL       	"/sys/class/gpio/gpio118/value"


#define SYSFS_GPIO_OUT_ENBL  	"out"
#define SYSFS_GPIO_IN_ENBL   	"in"

#define SYSFS_GPIO_VAL_H         "1"
#define SYSFS_GPIO_VAL_L         "0"

extern int test_gipo();

int gpio_export(int pin);

int gpio_unexport(int pin);

//dir =1 , output;
//dir= 0,input;
int gpio_direction(int pin, int dir);

int gpio_write(int pin, int value);

int open_gpio_rw_fd(int pin);

int gpio_read(int pin);

int gpio_edge(int pin, int edge);

int gpio_edge_poll(int fd, AUX_Trig_t trig, int timeouts);

int gpio_high_wait(int pin);

/*
 * 查询AUX上升沿是否到来,上升沿到来，则返回1,否则返回0
 * */
int gpio_edge_poll_try(int fd);

#endif /* GPIO_H_ */
