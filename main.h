#ifndef __MIAN_H__
#define __MIAN_H__

#include "radio.h"

#define ERROR 	-1
#define OK		0

#define LAN_INTERFACE	"eth0"
#define WLAN_INTERFACE  "wlan0"
#define WLAN_LISTEN_PORT  	20150

/*
 * USV接收监听端口
 * */
#define NVS_LISTEN_PORT  	8031
#define NSV_HOST_IP  		"192.0.3.3"

#define UART_Wireless_INTERFACE

enum{
	NET_NORMAL_RUNNING,
	NET_CONG_RUNNING,
};

enum{
	FREQ_CONFIG_INIT,
	FREQ_CONFIG_END,
};

enum {
	FALSE=0,
	TRUE
};

#define  USE_RADIO_AUX_PIN

extern RADIO_SPEED_t g_radio_speed;

extern int 	L_e32_uart_fd ;
extern int 	L_e32_aux_gpio_input_fd ;

extern int 	M_e32_uart_fd ;
extern int 	M_e32_aux_gpio_input_fd;

/*
 * 业务电台M_E32当前频率
 * */
extern Radio_CHL_t		g_m_e32_ch;

/*
 * 业务电M_E32当前速率
 * */
extern u8				g_m_e32_speed ;


extern u8     			g_UPG_on;

#endif



