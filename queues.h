/*
 * queue.h
 *
 *  Created on: 2015年2月4日
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

//WJW :定义MAC消息队列id
#define MAC_QUEUE 			1023


//#define WLAN_ROUTE_CTRL		 	0x01
//#define WLAN_ROUTE_DATA 		0X02
/**
 *无线接收的WLAN_DATA类型,包含WLAN_ROUTE_DATA,WLAN_ROUTE_CTRL两种数据
 */
//#define WLAN_DATA				0x03
//#define LAN_DATA				0x04
//#define SELF_DATA				0x05

//WJW 提升APP_DATA优先级

/*
 * uartWirelessSend.c无线发送线程处理数据时分两级优先级
 * */
#define WLAN_ROUTE_DATA 		0X01
#define WLAN_ROUTE_CTRL		 	0x02

/*
 * Routing.c接收线程处理数据时分三级优先级
 * 接收线程接收数据时，数据源有三种：
 * 1》通过无线接收到数据
 * 2》通过LAN口接收PC端数据
 * 3》LSA和RREQ控制信息
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
