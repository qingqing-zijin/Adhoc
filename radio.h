/*
 * radio.h
 *
 *  Created on: May 4, 2018
 *      Author: root
 */

#ifndef RADIO_H_
#define RADIO_H_

#include "serial.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef enum RADIO_MOD
{
	DTU_MOD=0,		//M0M1=00模式，包括透明传输和定点传输模式
	ADDR_MOD=1,		//M0M1=00模式，包括透明传输和定点传输模式
	SLEEP_MOD,
	LOW_PWR_MOD,
	STANDBY_MOD=3,
	RSSI_MOD,
}RADIO_MOD_t;

typedef enum RADIO_ADDR
{
	ADDR0=0,
	ADDR1,
	ADDR2,
	ADDR3=3,
	ADDR4,
	ADDR5,
	ADDR6,
	ADDR7,
	ADDR255=255
}RADIO_ADDR_t;

typedef enum RADIO_SPEED
{
	SPD_NULL,
	SPD_E32_4K8=1,
	SPD_E32_9K6=2,
	SPD_E32_19K2,	//4K8,9K6,19K2均为E32速率档
	SPD_E32_FLAG,
	SPD_E01_250K,
	SPD_E01_1M,
	SPD_E01_2M,		//256K,1M,2M均为E01速率档
	SPD_E01_FLAG
}RADIO_SPEED_t;


typedef struct
{
	u8 	pk_seq;		 		//负载分组序列号
	u8 	pk_payload_len;		//单个负载分组大小
	u8 	rcv_id;				//接收者，目的id，或广播255地址
	u8 	reserved;			//接收者，目的id，或广播255地址
	u16 pay_total_bytes; 	//总负载数据大小
	u16 pk_head_crc;
}RADIO_APP_Head;

//用户业务报文最大字节数
#define USER_PKT_MAX_LEN				(650UL)

/********************************************************
 *				E32模块业务数传数据块参数
 *******************************************************/
#define E32_RADIO_PKT_MAX_BYTES			(58U)
#define E32_APP_PKT_PAY_LOAD_MAX 		(E32_RADIO_PKT_MAX_BYTES -sizeof(RADIO_APP_Head))		//每包实际负载的字节数

#define E32_RADIO_PKTS_MAX_SZ			(USER_PKT_MAX_LEN/E32_APP_PKT_PAY_LOAD_MAX)

#define E32_APP_PKT_ALIGNMENT_MASK 		(E32_APP_PKT_PAY_LOAD_MAX-1)

/********************************************************
 *				E01模块业务数传数据块参数
 *******************************************************/
#define E01_RADIO_PKT_MAX_BYTES			(32U)
#define E01_APP_PKT_PAY_LOAD_MAX 		(E01_RADIO_PKT_MAX_BYTES -sizeof(RADIO_APP_Head))		//每包实际负载的字节数
#define E01_RADIO_PKTS_MAX_SZ			(USER_PKT_MAX_LEN/E01_APP_PKT_PAY_LOAD_MAX)
#define E01_APP_PKT_ALIGNMENT_MASK 		(E01_APP_PKT_PAY_LOAD_MAX-1)

/*
 *E32空口数传业务报文帧结构，每个帧定长58字节，由报文头+数据报文内容（pkt_payload）组成。
 * */
typedef struct
{
	RADIO_APP_Head head;					//报文头
	u8 data[E32_APP_PKT_PAY_LOAD_MAX];		//负载字段
}RADIO_E32_APP_t;


/*
 * E01空口业务报文帧结构，每个帧定长32字节，由报文头+数据报文内容（pkt_payload）组成。
 * */
typedef struct
{
	RADIO_APP_Head head;
	u8 data[E01_APP_PKT_PAY_LOAD_MAX];
}RADIO_E01_APP_t;

#define UART_SPEEDTYPE_Pos              (3U)
#define UART_SPEEDTYPE_Msk              (0x7U << UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_115200           ((u8)0x07<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_57600            ((u8)0x06<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_38400            ((u8)0x05<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_19200           	((u8)0x04<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_9600       	    ((u8)0x03<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_4800            	((u8)0x02<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_2400             ((u8)0x01<<UART_SPEEDTYPE_Pos)
#define UART_SPEEDTYPE_1200        		((u8)0x00<<UART_SPEEDTYPE_Pos)

#define AIR_SPEEDTYPE_Pos               (0U)
#define AIR_SPEEDTYPE_Msk               (0x7U << AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_19K2_3           	((u8)0x07<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_19K2_1            ((u8)0x07<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_19K2_2            ((u8)0x06<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_19K2           	 ((u8)0x05<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_9K6            	((u8)0x04<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_4K8       	    ((u8)0x03<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_2K4             	((u8)0x02<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_1K2               ((u8)0x01<<AIR_SPEEDTYPE_Pos)
#define AIR_SPEEDTYPE_300        		((u8)0x00<<AIR_SPEEDTYPE_Pos)
//
#define UART_MOD_8N1_Pos               	(6U)
#define UART_MOD_8N1_Msk               	(0x3U << UART_MOD_8N1_Pos)
#define UART_MOD_8N1		       		((u8)0x00<<UART_MOD_8N1_Pos)

/********************  Bit definition for OPTION register  ********************/
#define DTU_ADDR_MOD_Pos               	(7U)
#define DTU_MOD_Msk               		(0x1U << DTU_ADDR_MOD_Pos)
#define IO_MOD_Pos               		(6U)
#define IO_MOD_Msk               		(0x1U << IO_MOD_Pos)
#define FEC_ON_Pos             			(2U)
#define FEC_ON_Msk              		(0x1U << FEC_ON_Pos)
#define TX_POWER_Pos               		(0U)
#define TX_POWER__Msk               	(0x3U << TX_POWER_Pos)

//定点发送使能
#define DTU_MOD_ON           			((u8)0x00<<DTU_ADDR_MOD_Pos)
#define ADDR_MOD_ON           			((u8)0x01<<DTU_ADDR_MOD_Pos)
//IO驱动方式，默认方式为1
#define TXD_AUX_OC_RX_PULLUP           	((u8)0x01<<IO_MOD_Pos)
#define TXD_AUX_OPEN_RX_OPEN           	((u8)0x00<<IO_MOD_Pos)
//FEC方式，，默认方式为1
#define FEC_ON           				((u8)0x01<<FEC_ON_Pos)
#define FEC_OFF           				((u8)0x00<<FEC_ON_Pos)

//发射功率选择
#define POWER_37dBm_ON       			((u8)0x00<<TX_POWER_Pos)
#define POWER_30dBm_ON       			((u8)0x00<<TX_POWER_Pos)
#define POWER_27dBm_ON        			((u8)0x01<<TX_POWER_Pos)
#define POWER_24dBm_ON        			((u8)0x02<<TX_POWER_Pos)
#define POWER_21dBm_ON        			((u8)0x03<<TX_POWER_Pos)

typedef enum RADIO_DEV
{
	E32_L=0,
	E32_M=1,
	E43,
	E01,
	DEV_NULL
}RADIO_DEV_t;

typedef enum RADIO_HEAD_TYPE
{
	C0_FLASH_PARAM=0xC0,
	C2_RAM_PARAM=0xC2,
}HEAD_TYPE_t;

/*
 * 1>针对E43，,其信道频率计算公式为：
 * 425M + CHAN*0.25; CHAN值的范围为：00～3FH，即0～63共64个信道频;
 * 对应0对应425MHz;
 * 64对应425+16=441MHz，为此63对应425+15.75= 440.75MHz。
 *
 * 2>针对E32业务无线模块,其信道频率计算公式为：
 *  410MHz+CHAN*1MHz, 默认CHAN值为17H，值的范围为：00～1FH，共32个信道 ，对应值为23（433MHz）
 *  频率范围为410～441MHz。
 *
 * 3>为了统一E43, E32的信道频率，其对应的信道计算公式为：
 * E43： 425+ 4n*0.25MHz, n范围为0～15;  n=0时，频率为425MHz; n=15时，频率为425+15=440MHz
 * E32： 410+ (m+15)*1MHz, m范围为0～15; m=0时，频率为425MHz; m=15时，频率为410+30 = 440MHz
 * */

//信道，起始频点为410MHz, 信道间隔1MHz ，信道编号从0x00～1F共32个。

typedef enum E32_CHANL {
	CHANL0 = 0,
	CHANL1,
	CHANL2,
	CHANL3,
	CHANL4,
	CHANL5,
	CHANL6,
	CHANL7,
	CHANL8,
	CHANL9,
	CHANL10,
	CHANL11,
	CHANL12,
	CHANL13,
	CHANL14,
	CHANL15,		//MAX
	CHANL16,
	CHANL17,
	CHANL18,
	CHANL19,
	CHANL20,
	CHANL21,
	CHANL22,
	CHANL23,
	CHANL24,
	CHANL25,
	CHANL26,
	CHANL27,
	CHANL28,
	CHANL29,
	CHANL30,
	CHANL31
}Radio_CHL_t;

typedef struct
{
	//中速业务电台参数
	u8 m_e32_freq;
	u8 cfg_m_e32_freq_en;
	u8 m_e32_spd;
	u8 cfg_m_e32_spd_en;

	//高速业务电台参数
	u8 e01_freq;
	u8 cfg_e01_freq_en;
	u8 e01_spd;
	u8 cfg_e01_spd_en;

	//校验位
	u16 crc;
}Radio_LinkParam;		//通过空口配置无线电台参数

#define CH_MAX_VAL  		CHANL15
#define CH_MAX_NUM			(CH_MAX_VAL+1)




int radio_pks_sendto(int fd, u8* sendBytes, int len, unsigned short addrto);

int radio_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len);

int L_E32_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len);

int radio_cfg_params(RADIO_DEV_t dev_t,HEAD_TYPE_t head_type,  int fd, int uartSpeed, int uart_mod, int link_speed,
		int dtu_mod_on, int io_mod, int fec_on, int pwr_type, Radio_CHL_t channl, unsigned short addr);

int radio_cfg_mode(RADIO_MOD_t modType, RADIO_DEV_t dev_t);

//int config_E32_radio_params(Radio_CHL_t ch_val, u8 speed_level, u8 pwr_level);
int config_E32_radio_params(RADIO_COM_t com, int aux_fd, Radio_CHL_t ch_val, u8 speed_level, u8 pwr_level);

int config_E43_radio_rssi_mode_test();

int E43_channl_rssi_congitive(Radio_CHL_t ch_i);

int radios_gpio_init();

//void cmd_set_radio_link_speed(char* speedStr);

int L_E32_pks_sendto(int uart_fd, int aux_fd,u8* sendBytes, int send_len, unsigned short addrto);

int M_E32_set_radio_params(Radio_CHL_t ch_i, u8 air_link_speed);
#endif /* RADIO_H_ */
