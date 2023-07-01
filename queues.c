/*
 * queues.c
 *
 *  Created on: 2015年2月4日
 *      Author: lsp
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include "queues.h"
#include "aux_func.h"
#include "mac.h"

//extern unsigned char	g_BC_Req_rcvd ;
extern unsigned short	self_node_id ;
int create_queues()
{
	int ret;
	struct msqid_ds ds;
	int queueid;

	struct q_item msgitem;
	struct mac_q_item mac_msgitem;
	int temp_len =0;

	//创建消息队列，指定KEY和msgflg
	ret = msgget(LAN_SEND_CQ,IPC_EXCL);
	
	if(ret < 0)
	{
		ret = msgget(LAN_SEND_CQ,IPC_CREAT);
		if(ret < 0 )
		{
			LOG_MSG(ERR_LEVEL,"create LAN SEND msg queue");
			return ret;
		}
	}
	queueid = ret;

	//对于每一个队列都有一个msqid_ds来描述队列当前的状态
	memset(&ds,0,sizeof(ds));

	//通过msgctl函数，指定参数IPC_STAT,读取消息队列的数据结构msqid_ds，并将其存储在buf指定的地址中
	ret = msgctl(queueid,IPC_STAT,&ds);

	//设置消息队列buf最大字节数
	ds.msg_qbytes = 16384 * 100;

	//IPC_SET:设置消息队列的数据结构msqid_ds中的ipc_perm元素的值
	ret = msgctl(queueid,IPC_SET,&ds);

	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	//LOG_MSG(INFO_LEVEL,"create LAN SEND msg queue key = %d,queueid = %d\n",LAN_SEND_CQ,ret);

	while((temp_len= msgrcv(queueid ,&msgitem,MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old message inn LAN_SEND_Queue \n");
	};


	//创建接收消息队列
	ret = msgget(RECV_CQ,IPC_EXCL);
	if(ret < 0)
	{
		ret = msgget(RECV_CQ,IPC_CREAT);
		if(ret < 0 )
		{
			LOG_MSG(ERR_LEVEL,"create RECV msg queue" );
			return ret;
		}		
	}
	queueid = ret;
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	ds.msg_qbytes =16384 * 100;				/*由于用户消息*/
	ret = msgctl(queueid,IPC_SET,&ds);
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);

	//LOG_MSG(INFO_LEVEL,"create RECV msg queue key = %d,queueid = %d\n",RECV_CQ,ret);

	while((temp_len= msgrcv(queueid ,&msgitem,MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old message in RECV_CQ_Queue\n");
	};

	//创建WLAN_SEND消息队列
	ret = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(ret < 0)
	{
		ret = msgget(WLAN_SEND_CQ,IPC_CREAT);
		if(ret < 0 )
		{
			LOG_MSG(ERR_LEVEL,"create WLAN SEND DATA msg queue ");
			return ret;
		}
	}
	queueid = ret;

	LOG_MSG(INFO_LEVEL,"create WLan_Send_CQ id = %d\n",queueid);

	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	ds.msg_qbytes = 16384 * 100;
	ret = msgctl(queueid,IPC_SET,&ds);
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	//LOG_MSG(INFO_LEVEL,"create WLAN SEND  msg queue key = %d,queueid = %d\n",WLAN_SEND_CQ,ret);

	while((temp_len= msgrcv(queueid ,&msgitem,MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old messages in WLAN_SEND_Queue\n");
	};



	//创建mac层消息队列
	ret = msgget(MAC_QUEUE,IPC_EXCL);
	if(ret < 0)
	{
		ret = msgget(MAC_QUEUE,IPC_CREAT);
		if(ret < 0 )
		{
			LOG_MSG(ERR_LEVEL,"create MAC_QUEUE\n");
//			return ret;
			exit(0);
		}
	}
	queueid = ret;
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	ds.msg_qbytes = 16384 * 100;
	ret = msgctl(queueid,IPC_SET,&ds);
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	while((temp_len= msgrcv(queueid ,&mac_msgitem,MAC_MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old messages in MAC_QUEUE\n");
	};

	return ret;
}

/*
 instert msg into the queue whose id is msgqid.
input:
 msgqid  	the id of the queue
 msg		message to be send
 msgsize	len of the message
 msgtype	the type of the message
 msgflg		send flag such as IPC_NOWAIT
return: 
	-1 if fails
	0  if success
*/
int msgQ_snd(int msgqid, void *msg, int msgsize,long msgtype, int msgflg)
{
	int ret;
	struct q_item titem;

	/*
	 * WJW：
	 * 采用集中轮询策略接入信道时，从站未收到接入信道请求前，
	 * 将产生“无用”用户业务数据报文和路由控制报文，并添加到无线发送消息队列，等到真正主站接入信道请求到来时，
	 * 获取的将是历史消息。因此，为避免这种问题，达到实时性，加入开关量，只有从站收到BC_REQ(1-> 本节点)请求后，
	 * 才开始允许添加消息至无线发送消息队列。
	 * */

	if(g_BC_Req_rcvd== 0 && self_node_id != 1)
	{
		//未收到一次接入信道请求，则直接丢弃用户数据报文和路由控制报文
		LOG_MSG(INFO_LEVEL,"g_BC_Req_rcvd=0 , not execute msgsnd() to queuen \n");
		return 0;
	}

	titem.type = msgtype;
	memcpy(titem.data,msg,msgsize);
	/*
	msgrcv()可以从消息队列中读取消息，
	msgsnd()将一个新的消息写入队列。
	函数原型:
		int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
		ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
	*/
	ret = msgsnd(msgqid,&titem,msgsize,msgflg);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"msgQ_send");
		return ret;
	}
	//LOG_MSG(INFO_LEVEL,"send msg into queue id = %d\n",msgqid);
	return ret;
}


