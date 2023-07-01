/*
 * Congitive.h
 *
 *  Created on: May 23, 2018
 *      Author: root
 */

#ifndef CONGITIVE_H_
#define CONGITIVE_H_
#include "main.h"
#include "radio.h"

#define NET_CONG_CMD		"NCC"

typedef	struct {
	u8 pk_type;
	u8 send_id;
	u8 cong_cmd[4];
	u16 pk_crc;
}CONG_CMD_PK;


//Add by wujiwen, 2018/5/30, ע��ͬ��Routing.h�ж����LSA_PK_TYPE��...RERR_PK_TYPE��
#define		CONG_START_PK_TYPE					0x14
#define		CONG_RET_PK_TYPE					0x15
#define		CONG_FREQ_RESULT_PK_TYPE			0x16
//�Զ�����������
#define MAX_NET_NODES		10

typedef struct
{
	u8 	pk_type;
	u8 	send_id;					//���ؽڵ�id
	u8 	recv_id;					//ȡֵΪ���з�����
	u8 	avg_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ���N��ɨ��rssiƽ��ֵ
//	u8  max_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ�N��ɨ��rssiֵ�����ֵ
//	u8  min_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ�N��ɨ��rssiֵ����Сֵ
	u16 pk_crc;
}Congitive_Result_t;

typedef struct
{
	u8 	sta;
	u8 	comm_avg_rssi_max;		//��¼ÿ���ڵ���ͬ�ŵ���N��ɨ��rssi�ľ��߽��
}Congitive_Descion_Result_t;


enum{
	CONG_ERR=0,
	CONG_OK
};

typedef struct
{
	u8 				   	node_sta;
	Congitive_Result_t 	cong_result;
}Net_Congitive_Result_t;


extern int			wd_LSA;
extern u8 			master_cong_id;
extern unsigned int PERIOD_LSA;
//extern u8 			global_freq_config_state;
extern int 			e32_uart_fd;

extern u8			global_e43_existed;

#define LOC_CONG_DELY			10			//ÿ���ŵ�ɨ��1000ms����10�����

//#define NET_CONG_DELY				(1500*MAX_NET_NODES)	//ÿ���ŵ�ɨ��1000ms����10�����
#define NET_CONG_DELY				(1300*CH_MAX_NUM)	//ÿ���ŵ�ɨ��1000ms����10�����

#define CONG_MAX_TIMES_PER_CHNL  	5

extern int global_running_state;

void 	NetCong_Module_Init();
void 	Cong_Local_Start();
void 	NetCong_Result_Receive_Handle(u8 *sub_node_cong_pkt);

u8 		NetCong_Desicion_Handle();
int  	NetCong_get_Wireless_SndQueueId();
int 	NetCong_Wireless_MsgQSnd(int msgqid, void *msg, int msgsize,long msgtype, int msgflg);

#endif /* CONGITIVE_H_ */
