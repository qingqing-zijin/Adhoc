/*
 * queues.c
 *
 *  Created on: 2015��2��4��
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

	//������Ϣ���У�ָ��KEY��msgflg
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

	//����ÿһ�����ж���һ��msqid_ds���������е�ǰ��״̬
	memset(&ds,0,sizeof(ds));

	//ͨ��msgctl������ָ������IPC_STAT,��ȡ��Ϣ���е����ݽṹmsqid_ds��������洢��bufָ���ĵ�ַ��
	ret = msgctl(queueid,IPC_STAT,&ds);

	//������Ϣ����buf����ֽ���
	ds.msg_qbytes = 16384 * 100;

	//IPC_SET:������Ϣ���е����ݽṹmsqid_ds�е�ipc_permԪ�ص�ֵ
	ret = msgctl(queueid,IPC_SET,&ds);

	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);
	//LOG_MSG(INFO_LEVEL,"create LAN SEND msg queue key = %d,queueid = %d\n",LAN_SEND_CQ,ret);

	while((temp_len= msgrcv(queueid ,&msgitem,MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old message inn LAN_SEND_Queue \n");
	};


	//����������Ϣ����
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
	ds.msg_qbytes =16384 * 100;				/*�����û���Ϣ*/
	ret = msgctl(queueid,IPC_SET,&ds);
	memset(&ds,0,sizeof(ds));
	ret = msgctl(queueid,IPC_STAT,&ds);

	//LOG_MSG(INFO_LEVEL,"create RECV msg queue key = %d,queueid = %d\n",RECV_CQ,ret);

	while((temp_len= msgrcv(queueid ,&msgitem,MAX_MSG_LEN,0,IPC_NOWAIT)) >0 ){
		LOG_MSG(INFO_LEVEL," discard old message in RECV_CQ_Queue\n");
	};

	//����WLAN_SEND��Ϣ����
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



	//����mac����Ϣ����
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
	 * WJW��
	 * ���ü�����ѯ���Խ����ŵ�ʱ����վδ�յ������ŵ�����ǰ��
	 * �����������á��û�ҵ�����ݱ��ĺ�·�ɿ��Ʊ��ģ�����ӵ����߷�����Ϣ���У��ȵ�������վ�����ŵ�������ʱ��
	 * ��ȡ�Ľ�����ʷ��Ϣ����ˣ�Ϊ�����������⣬�ﵽʵʱ�ԣ����뿪������ֻ�д�վ�յ�BC_REQ(1-> ���ڵ�)�����
	 * �ſ�ʼ���������Ϣ�����߷�����Ϣ���С�
	 * */

	if(g_BC_Req_rcvd== 0 && self_node_id != 1)
	{
		//δ�յ�һ�ν����ŵ�������ֱ�Ӷ����û����ݱ��ĺ�·�ɿ��Ʊ���
		LOG_MSG(INFO_LEVEL,"g_BC_Req_rcvd=0 , not execute msgsnd() to queuen \n");
		return 0;
	}

	titem.type = msgtype;
	memcpy(titem.data,msg,msgsize);
	/*
	msgrcv()���Դ���Ϣ�����ж�ȡ��Ϣ��
	msgsnd()��һ���µ���Ϣд����С�
	����ԭ��:
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


