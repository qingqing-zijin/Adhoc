/*
 * queue.h
 *
 *  Created on: 2015��2��4��
 *      Author: lsp
 */

#ifndef SRC_QUEUES_H_
#define SRC_QUEUES_H_
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>


#define LAN_SEND_CQ 		1024
#define RECV_CQ				1025
#define WLAN_SEND_CQ 		1026

//WJW :����MAC��Ϣ����id
#define MAC_QUEUE 			1023


//#define WLAN_ROUTE_CTRL		 	0x01
//#define WLAN_ROUTE_DATA 		0X02
/**
 *���߽��յ�WLAN_DATA����,����WLAN_ROUTE_DATA,WLAN_ROUTE_CTRL��������
 */
//#define WLAN_DATA				0x03
//#define LAN_DATA				0x04
//#define SELF_DATA				0x05

//WJW ����APP_DATA���ȼ�

/*
 * uartWirelessSend.c���߷����̴߳�������ʱ���������ȼ�
 * */
#define WLAN_ROUTE_DATA 		0X01
#define WLAN_ROUTE_CTRL		 	0x02

/*
 * Routing.c�����̴߳�������ʱ���������ȼ�
 * �����߳̽�������ʱ������Դ�����֣�
 * 1��ͨ�����߽��յ�����
 * 2��ͨ��LAN�ڽ���PC������
 * 3��LSA��RREQ������Ϣ
 * */
#define WLAN_DATA				0x01
#define LAN_DATA				0x02
#define SELF_DATA				0x03

#define MAX_MSG_LEN			2048

struct q_item
{
	long type;
	char data[MAX_MSG_LEN];
};


extern int create_queues();
extern int msgQ_snd(int msgqid, void *msg, int msgsize,long msgtype, int msgflg);
#endif /* SRC_QUEUES_H_ */
