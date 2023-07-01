/*
 * mac.c
 *
 *  Created on: Jun 25, 2018
 *      Author: root
 */
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <time.h>
#include "main.h"
#include "mac.h"
#include "aux_func.h"
#include "queues.h"
#include "radio.h"
#include "gpio.h"
#include "aux_func.h"
#include "linklist.h"
#include "Routing.h"
#include <errno.h>

#include <pthread.h>


List*	MAC_oneHopNb_List = NULL;		// 一跳邻居链表
List*	MAC_twoHopNb_List = NULL;		// 多跳邻居链表
List*	MAC_IERP_List = NULL;			// 域间路由链表

extern int 				snd_msgqid;
extern int 				M_e32_uart_fd;
extern unsigned short 	self_node_id;
extern int				msgQ_Wifi_Snd;

extern List*			TOKEN_RING_BC_REQ_Record_List;
extern List*			MAC_oneHopNb_List;
extern List*			MAC_twoHopNb_List;
extern List*			MAC_IERP_List;

struct q_item 			g_msgitem;

volatile int   			g_msgitem_len =0;

volatile u8   			g_BC_Req_rcvd =FALSE;

#define 				MAC_TIMEOUT_SIG 63
time_t 					wd_MAC;

static u8 		g_nb_node_i =0;
MAC_NB_State_t  nb_state_list[4];
/*
 * 鉴于MAC集中轮询算法的状态机变量值在mac.c和routing.c中定义的线程可能同时被修改，产生线程同步问题。
 * 因此必须考虑Linux线程同步与互斥。
 *
 *
 * */
//pthread_mutex_t 		mac_st_mutex;

//pthread_mutex_t 		mac_LinkList_mutex;

//TOKEN_RING_STATE_t 		global_token_ring_state = TR_ST_INIT;

TOKEN_RING_STATE_t 		mac_TR_state = TR_ST_INIT;
volatile u8 			g_mac_timeout = FALSE;

void Set_TR_State(TOKEN_RING_STATE_t  state)
{
	//pthread_mutex_lock(&mac_st_mutex);
	mac_TR_state = state;
	//pthread_mutex_unlock(&mac_st_mutex);
//	LOG_MSG(MAC_LEVEL,"------------------------------mac_TR_state=%d \n",mac_TR_state);

}

TOKEN_RING_STATE_t Get_TR_State()
{
	TOKEN_RING_STATE_t state;
	//pthread_mutex_lock(&mac_st_mutex);
	state = mac_TR_state ;
	//pthread_mutex_unlock(&mac_st_mutex);
	return state;
}

extern void snd_msgTo_radio(struct q_item* msgitem, int msg_len);

static unsigned short	BC_REQ_Seq=0;

static unsigned short	REQ_Seq=0;

int Del_Mac_Route_Tb_Node(u8 node_id)
{
	int	i = 1;
	MAC_ONE_HOP_NB_ITEM		*one_element_ptr;
	MAC_MULTI_HOP_NB_ITEM	*two_element_ptr;
	MAC_IERP_ITEM			*ierp_element_ptr;

	//global_token_ring_state = TR_ST_INIT;



//	pthread_mutex_lock(&mac_LinkList_mutex);

	while(NULL != (one_element_ptr = (MAC_ONE_HOP_NB_ITEM*)prg_list_access(MAC_oneHopNb_List, i)))
	{
		if(one_element_ptr->node_ID == node_id )
		{
			prg_list_remove(MAC_oneHopNb_List ,i);
			LOG_MSG(MAC_LEVEL,"-------------delete MAC_oneHopNb_List node%d\n",node_id);
			return 1;
		}
		i++;
	}

	i=0;
	while(NULL != (two_element_ptr = (MAC_MULTI_HOP_NB_ITEM*)prg_list_access(MAC_twoHopNb_List, i)))
	{
		if(two_element_ptr->node_ID == node_id)
		{
			prg_list_remove(MAC_twoHopNb_List ,i);
			LOG_MSG(MAC_LEVEL,"-------------delete MAC_twoHopNb_List node%d\n",node_id);
			return 1;
		}
		i++;
	}

	i=0;
	while(NULL != (ierp_element_ptr = (MAC_IERP_ITEM*)prg_list_access(MAC_IERP_List, i)))
	{
		if(ierp_element_ptr->dest_ID == node_id)
		{
			prg_list_remove(MAC_IERP_List ,i);
			LOG_MSG(MAC_LEVEL,"-------------delete MAC_IERP_ITEM node%d\n",node_id);
			return 1;
		}
		i++;
	}

//	pthread_mutex_unlock(&mac_LinkList_mutex);

	return 0;
}

int MAC_Search_BC_REQ_Record(List* rreq_list, u8 rreq_src_id, u8 rreq_dest_id, u16 rreq_seq)
{
	int				i = 1;
	MAC_RREQ_RECORD		*element_ptr;

	while(NULL != (element_ptr = (MAC_RREQ_RECORD*)prg_list_access(rreq_list, i)))
	{
		if(rreq_seq == 0)
		{
			if(element_ptr->rreq_src_ID == rreq_src_id && \
			element_ptr->rreq_dest_ID == rreq_dest_id)
			{
				return i;
			}
		}
		else
		{
			if(element_ptr->rreq_src_ID == rreq_src_id && \
			element_ptr->rreq_dest_ID == rreq_dest_id && \
			element_ptr->rreq_seq == rreq_seq)
			{
				return i;
			}
		}

		i++;
	}

	return -1;
}

/*
 * @FUN：
 * 		MAC_Route_Search
 * @Desc：
 * 		查找是否有src_ID-->dest_ID路径
 *		由参数@rcv_ID 返回下一跳节点ID
 *		由参数@rreq_sr_ID返回路径的源节点ID
 *		由参数@hop返回跳数
 *		由参数@trace_list返回路径节点ID数组
 *	@RETURN
 *		-1：失败无路由
 *		0 ：成功，有路由
 * */
int MAC_Route_Search(u8 dest_id,u8 *next_idptr,u8 *rreq_src_idptr, u8*hopptr, u8 *trace_list)
{
	int					rreq_seq = -1;
	int					i = 1;
	MAC_ONE_HOP_NB_ITEM		*element_ptr1;
	MAC_MULTI_HOP_NB_ITEM	*element_ptr2;
	MAC_IERP_ITEM			*element_ptr3;

	*hopptr = 0;

	// 1. 若目的为广播，则直接发送，下一跳也是广播
	if(dest_id == 0xff)
	{
		*next_idptr = 0xff;
		*rreq_src_idptr = 0xff;
		*hopptr = MAC_MAX_HOP-1;
		return 0;
	}
	// 2. 先找一跳邻居表中node_ID == dest_id 且双向可通的项，若存在，直接返回第一个结果
	i = 1;
	while(NULL != (element_ptr1 = (MAC_ONE_HOP_NB_ITEM*)prg_list_access(MAC_oneHopNb_List, i)))
	{
		if(element_ptr1->node_ID == dest_id)
		{
			*next_idptr = element_ptr1->node_ID;
			*rreq_src_idptr = 0;
			*hopptr = 1;
			break;
		}
		i++;
	}
	if (*hopptr > 0)
	{
		return 0;		// 域内路由
	}

	// 3. 再找2跳邻居表中node_ID == dest_id的项，若存在，直接返回第一个结果
	i = 1;
	while(NULL != (element_ptr2 = (MAC_MULTI_HOP_NB_ITEM*)prg_list_access(MAC_twoHopNb_List, i)))
	{
		if(element_ptr2->node_ID == dest_id )
		{
			*next_idptr = element_ptr2->relay_ID;
			*rreq_src_idptr = 0;
			*hopptr = element_ptr2->hop;
			break;
		}
		i++;
	}
	if (*hopptr > 0)
	{
		return 0;		// 域内路由
	}


	// 4. 最后找域间路由表中 dest_ID == dest_id的项，若存在，直接返回第一个结果
	i = 1;
	while(NULL != (element_ptr3 = (MAC_IERP_ITEM*)prg_list_access(MAC_IERP_List, i)))
	{
		if(element_ptr3->dest_ID == dest_id )
		{
			if(*hopptr <= element_ptr3->hop )		// 找最佳（hop最小的中选QOS最大）的路径
			{
				*next_idptr = element_ptr3->next_ID;
				*rreq_src_idptr = element_ptr3->rreq_src_ID;
				rreq_seq = element_ptr3->rreq_seq;
				*hopptr = element_ptr3->hop;
				memcpy((unsigned char*)trace_list, (unsigned char*)(element_ptr3->trace_list), MAC_MAX_HOP);
				break;
			}
		}
		i++;
	}
	if (*hopptr > 0)
	{
		return rreq_seq;		// 域间路由
	}


	return -1;
}


/* Get_Next_ID()  ----- 从trace_list中提取 id 的下一跳ID。没找到或输入出错，返回-1
 */
int MAC_Get_Next_ID(u8 *trace_list, u8 id)
{
	int	i;

	if (id == 0 || trace_list == NULL)
		return -1;

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		if(trace_list[i] == id)
		{
			break;
		}
	}
	if (i>=MAC_MAX_HOP-1)
		return -1;
	else
		return trace_list[i+1];
}


#define		MAC_SEQ_INC(x)		(x = ((unsigned short)(x)==0xffff ? 1:(x)+1))


/* 根据接收到的RREQ报文，更新RREQ记录表。
	RREQ记录表不能仅根据rreq_src_ID与seq区分，还要根据rreq_dest_ID,因为一个节点肯定会向多个目的寻路，而不同目的ID的寻路过程，仅比较seq不能确定新旧
	1. 根据src_ID和dest_ID查找RREQ_Record记录表。
	2. 若记录表内无此记录，则新建一块内存，赋值后插入RREQ链表末尾，并返回1。
	3. 若有记录，则按照下述规则更新该记录
			若报文中的的rreq_seq较新，则更新REQ_Record，并返回1。
			若rreq_seq相同，但新报文的hop较小，则更新REQ_Record，并返回1。
								hop相同或较大，并返回0。
			若报文中的的rreq_seq较旧，并返回0。

	返回值：0：未发生更新；1：发生更新

    注意更新hop,trace_list,path_QOS.加入本节点的影响
*/
int Update_TokenRing_BC_REQ_Tab(TokenRing_BC_Req_PK *bc_req_pkptr)
{
	int				record_pos;
	MAC_RREQ_RECORD	*record_ptr;
	int				cmp_res, i;
	u8				src_ID;
	u8				dest_ID;
	u16  			rreq_seq;
	u8				hop;
//	u8  			path_qos;

	// 1. 节点收到后，解析出src_ID,dest_ID,rreq_seq和hop;
	src_ID 		= bc_req_pkptr->src_ID;
//    send_ID 	= bc_req_pkptr->send_ID;
	dest_ID 	= bc_req_pkptr->dest_ID;
	rreq_seq 	= bc_req_pkptr->rreq_seq;
	hop 		= bc_req_pkptr->hop;


	// 2. 依据src_ID\dest_ID\rreq_seq搜索RREQ_RECORD表，找到第一个记录。
	/*
	 * 考虑不同节点开机时刻不一致的情况。
	 * 1》比如节点2先开机，这时
	 * */

	record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, src_ID,dest_ID, rreq_seq);	//根据RREQ请求的源节点id和目的id，查找RREQ表项
	record_ptr = (MAC_RREQ_RECORD *)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	if(record_pos == -1)
	{
		// 3. 若记录表内无此记录，则新建一块内存，赋值后插入RREQ链表末尾，并返回1。
//        LOG_MSG(MAC_LEVEL, "new RREQ record\n");
		record_ptr = (MAC_RREQ_RECORD*)malloc(sizeof(MAC_RREQ_RECORD));

		record_ptr->rreq_src_ID = src_ID;
		record_ptr->rreq_dest_ID = dest_ID;
		record_ptr->rreq_seq = rreq_seq;
//		record_ptr->path_QOS = path_qos;
		record_ptr->hop = hop+1;

		//将RREQ请求节点至本节点路径保存，并将本节点id信息追加至路径末尾
		for(i = 0; i< hop;i++)
		{
			record_ptr->trace_list[i] = bc_req_pkptr->trace_list[i];
		}

		//hop+1记录的是本节点ID
		record_ptr->trace_list[hop] = Node_Attr.ID;	 	// 末尾加上本地ID

		prg_list_insert(TOKEN_RING_BC_REQ_Record_List, record_ptr, LIST_POS_TAIL);

		return 1;
	}
	else
	{
		// 4. 若有记录，当报文内的rreq_seq较新，或rreq_seq相等但新报文的跳数较少，或rreq_seq和hop都相等但path_QOS 较大时，
        //              更新RREQ链表的第record_pos个元素,并返回1。
//        LOG_MSG(MAC_LEVEL,"pk/record = seq=[%d, %d], hop=[%d,%d]\n",rreq_seq,record_ptr->rreq_seq,hop,record_ptr->hop-1);
		cmp_res = Seq_Cmp(rreq_seq, record_ptr->rreq_seq);

		if(cmp_res != 0 || (cmp_res == 0 && hop < record_ptr->hop-1))
		{
			record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

			record_ptr->rreq_src_ID = src_ID;
			record_ptr->rreq_dest_ID = dest_ID;
			record_ptr->rreq_seq = rreq_seq;
			record_ptr->hop = hop+1;        // hop+1 保存

			for(i = 0; i< hop;i++)
			{
				record_ptr->trace_list[i] = bc_req_pkptr->trace_list[i];
			}

			record_ptr->trace_list[hop] = Node_Attr.ID;     // 末尾加上本地ID

			return 1;

		}
		else
		{
			// 5. 其它情况不处理，返回0
			return 0;
		}

	}
}

/* Get_Prev_ID()  ----- 从trace_list中提取 id 的上一跳ID。没找到或输入出错，返回0
 */
u8 MAC_Get_Prev_ID(u8 *trace_list, u8 id)
{
	int	i;

	if (id == 0 || trace_list == NULL)
		return 0;

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		if(trace_list[i] == id)
		{
			break;
		}
	}
	if (i>=MAC_MAX_HOP || i == 0)
		return 0;
	else
		return trace_list[i-1];
}

int Handle_rcv_token_ring_OK(TokenRing_OK_PK* OK_pkptr)
{
	u8	i=0;
	// 2. 根据报头格式，解析出源、目的IP地址 和 TTL，bc_seq等信息; 注意，报文内容为网络字节序
	u8 rcv_ID 		= OK_pkptr->rcv_ID;
	u8 src_ID 		= OK_pkptr->src_ID;
	u8 dest_ID 		= OK_pkptr->dest_ID;
	u16 rreq_seq 	= OK_pkptr->rreq_seq;
	u8	trace_list[MAC_MAX_HOP];

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		trace_list[i] = OK_pkptr->trace_list[i];
	}

	/*
	 * 以下为点对点传输，多跳中继情况下，通过trace_list[...]指导如何传输至下一个节点
	 * */
//    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_OK_PK from node%d to node%d, rcv_ID=%d \n",src_ID, dest_ID, rcv_ID);

	if(rcv_ID == (Node_Attr.ID & 0xff))		//中间节点是本节点否？
	{
		if(dest_ID == rcv_ID)		//目的节点是否为本节点否？
		{
			/*
			 *若是发给本节点的报文，则回复TokenRingREP报文。。。。
			 * */
		    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_OK_PK from node%d to node%d, rcv_ID=%d \n",src_ID, dest_ID, rcv_ID);

#ifdef DEF_GET_MSG_NUM
			memset((u8*)&g_msgitem, 0, sizeof(struct q_item));
			g_msgitem_len = msgrcv(snd_msgqid,&g_msgitem,MAX_MSG_LEN,-2,MSG_NOERROR |IPC_NOWAIT);
#endif
			snd_msgTo_radio(&g_msgitem, g_msgitem_len);

#ifndef NOT_USED_OK_ACK
			usleep(5000);
			/* 时间不能太大，否在主控站超时后主站会进入TR_INIT_ST,然后发送消息队列中消息。
				此时，若从站再发送OK_ACK_PKT，会导致信道碰撞冲突。
			*/
			/*
			 * 2018-07-24 若主控节点是我的一跳邻居
			 * */
//			if(Is_1hopNb(src_ID) ==0)
//			{
			TokenRing_OK_ACK_Send(src_ID,  dest_ID);
//			}
#endif
		}
		else if(rreq_seq == 0)
		{
			// 4.2 接收节点是自身，且为域内路由，则查IARP找下一跳
			rcv_ID = MAC_Get_Next_ID(trace_list, ((u8)Node_Attr.ID & 0xff));		//理论上，此处必然能够更新rcv_ID，否则，就是源端封装路由头时出错了
			if(rcv_ID >= 0)
			{
				// 4.2.1 有域内路由, 修改收发地址，重新计算CRC
				OK_pkptr->send_ID = self_node_id;
				OK_pkptr->rcv_ID  = rcv_ID;
				OK_pkptr->pk_crc  = getCRC((u8*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
				LOG_MSG(MAC_LEVEL,"--------------------------------Relay forward TR_OK_PK to node%d \n",rcv_ID);
				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
			}
			else
			{
				// 4.2.3 只有域间路由或无路由，直接丢弃报文
				LOG_MSG(MAC_LEVEL,"--------------------------------no IARP route to relay forward TR_OK_PK to dest_ID(%d) \n",dest_ID);
			}
//			routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
//			if(routing_state == 0)
//			{
//				// 4.2.1 有域内路由, 修改收发地址，重新计算CRC
//				OK_pkptr->send_ID = self_node_id;
//				OK_pkptr->rcv_ID  = rcv_ID;
//				OK_pkptr->pk_crc  = getCRC((u8*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
//			    LOG_MSG(MAC_LEVEL,"--------------------------------forward TR_OK_PK to node%d \n",rcv_ID);
//				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
//			}
//			else
//			{
//				// 4.2.3 只有域间路由或无路由，直接丢弃报文
//			    LOG_MSG(MAC_LEVEL,"--------------------------------no IARP route to forward TR_OK_PK to node%d \n",rcv_ID);
//			}
		}
		else
		{
			// 4.3 接收节点是自身，且为域间路由，则查报头内的trace_list找下一跳节点id
			rcv_ID = MAC_Get_Next_ID(trace_list, ((u8)Node_Attr.ID & 0xff));		//理论上，此处必然能够更新rcv_ID，否则，就是源端封装路由头时出错了

			if( rcv_ID >=0 )
			{
				//  是双向邻居, 或radius = 0 的情况下为单向邻居，则修改收发地址，重新计算CRC
				OK_pkptr->send_ID 	= self_node_id & 0xff;
				OK_pkptr->rcv_ID 	= rcv_ID;
				OK_pkptr->pk_crc 	= getCRC((unsigned char*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);

			    LOG_MSG(MAC_LEVEL,"--------------------------------Relay forward TR_OK_PK to node%d \n",rcv_ID);
				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);
			}
			else
			{
				// 域间也没有路由， 直接丢弃报文
			    LOG_MSG(MAC_LEVEL,"--------------------------------no IERP route to relay forward TR_OK_PK to dest_ID（%d） \n",dest_ID);
			}

			// 4.3.1 查一跳邻居表，判断到rcv_ID是否可达？可达则封装后发送，不可达，则逆向发rerr_pklen
//			i = Is_MacOneHopNb(rcv_ID);
//			if( i >0 )
//			{
//				//  是双向邻居, 或radius = 0 的情况下为单向邻居，则修改收发地址，重新计算CRC
//				OK_pkptr->send_ID 	= self_node_id & 0xff;
//				OK_pkptr->rcv_ID 	= rcv_ID;
//				OK_pkptr->pk_crc 	= getCRC((unsigned char*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
//
//			    LOG_MSG(MAC_LEVEL,"--------------------------------forward TR_OK_PK to node%d \n",rcv_ID);
//				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);
//			}
//			else
//			{
//				// 域间也没有路由， 直接丢弃报文
//			    LOG_MSG(MAC_LEVEL,"--------------------------------no IERP route to forward TR_OK_PK to node%d \n",rcv_ID);
//			}
		}
	}
	else
	{
		//4.5 接收ID不是自身，直接丢弃。
		return 0;
	}
	return 1;
}

/*
 * @FUN:
 * 		 TokenRing_OK_PK_Send
 * @Desc:
 * 		单播发送TokenRing_OK_PK报文至目的节点，允许占用信道发送消息;目的节点收到
 * 		报文后，立即发送消息队列中消息;
 * */
int TokenRing_OK_PK_Send(u8 dest_ID)
{
	u8  rcv_ID;
	u8	src_ID	=self_node_id;;
	u8	rreq_src_id;

	u8	hop;
	u8	trace_list[MAC_MAX_HOP];

	int	routing_state;
	int	i;

	TokenRing_OK_PK	tokenRing_OK_pk;
	TokenRing_OK_PK *tokenRingOk_pkptr = &tokenRing_OK_pk;
	memset(&tokenRing_OK_pk, 0, sizeof(TokenRing_OK_PK));

	if(dest_ID == self_node_id)
		return TRUE;

	/*调用MAC_Route_Search查找是否有src_ID-->dest_ID路径
	 *由参数@rcv_ID 返回下一跳节点ID
	 *由参数@rreq_sr_ID返回路径的源节点ID
	 *由参数@hop返回跳数
	 *由参数@trace_list返回路径节点ID数组
	 * */
	routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);

	if(routing_state >= 0)		// 有域内或域间路由
	{
		tokenRingOk_pkptr->pk_type 		= TOKEN_RING_OK_PK_TYPE;
		/*
		 * 返回next_rcv_ID,
		 * */
		tokenRingOk_pkptr->send_ID 		= src_ID;

		//下一跳节点ID
		tokenRingOk_pkptr->rcv_ID 		= rcv_ID;		//表示到达目的节点的中间节点或目的ID本身。

		//源节点和目的节点
		tokenRingOk_pkptr->src_ID		= src_ID;
		tokenRingOk_pkptr->dest_ID 		= dest_ID;
		tokenRingOk_pkptr->rreq_src_ID 	= rreq_src_id;
		/*
		 * rreq_seq为0时，表示域内1跳或2跳子网内有路由
		 * */
		tokenRingOk_pkptr->rreq_seq 	= routing_state;
		tokenRingOk_pkptr->hop 			= hop;	//子网内，1跳时hop=1; 2跳时hop=2;

		/*多跳情况下，携带中继节点路径列表
		 * */
		for(i =0; i< MAC_MAX_HOP; i++)
		{
			tokenRingOk_pkptr->trace_list[i] = trace_list[i];
		}

		tokenRingOk_pkptr->pk_crc = getCRC((unsigned char*)tokenRingOk_pkptr, sizeof(TokenRing_OK_PK)-2);

		/*
		 * 将状态置为TOKEN_RING_OK_SEND状态，等待TOKEN_RING_OK_ACK到来。。。。
		 * */
//		global_token_ring_state =TR_OK_SEND;
		LOG_MSG(MAC_LEVEL,"--------------------send TR_OK_PK from node%d to node%d \n",self_node_id, dest_ID);

		Set_TR_State(TR_OK_SEND);

		// 4. 发送TokenRing_Req_PK请求
		radio_pks_sendto(M_e32_uart_fd, (u8*)tokenRingOk_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
		// 5. LOG 打印
		return TRUE;
	}
	else
	{
		//没有达到目的节点的路径
		LOG_MSG(MAC_LEVEL,"--------------------no route to send TR_OK_PK from node%d to node%d \n",self_node_id, dest_ID);
		//global_token_ring_state =TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);

		return FALSE;
	}
}


/*
 * 主站收到RouteData,
 * */
int Handle_rcv_RouteData_Pkt(TokenRing_RX_RouteDATA_PK *TR_RX_RouteData_pkt)
{

	u8 srcID= TR_RX_RouteData_pkt->src_ID;

//	Set_TR_State(TR_ST_INIT);

	if(srcID == 1)
	{
		LOG_MSG(MAC_LEVEL, "--------------------rcvd TokenRing_RX_RouteDATA\n");

		if(srcID == Node_Attr.ID)
		{
			//取消第二阶段TR_OK_Send之后启动定时器
//			g_mac_timeout =FALSE;
			nb_state_list[g_nb_node_i].mac_timeouts_cnt =0;

			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);

			//启动接收TR_OK_ACK定时器
			wdStart(wd_MAC, OK_ACK_TIMEOUT_MS,TIMER_ONE_SHOT);
		}
	}
	return 0;
}

int Handle_rcv_OK_ACK(TokenRing_OK_ACK_PK *TR_ok_ack_pk)
{
	// 1. 解析rcv_ID，src_ID，dest_ID等
	u8	send_ID					=TR_ok_ack_pk->send_ID;
	u8	rcv_ID					=TR_ok_ack_pk->rcv_ID;
	u8	rreq_src_ID				=TR_ok_ack_pk->rreq_src_ID;
	u8	rreq_dest_ID			=TR_ok_ack_pk->rreq_dest_ID;
	u8	hop						=TR_ok_ack_pk->hop;			//hop值为跳数+1
	u8	trace_list[MAC_MAX_HOP]	={0};
	int	i;

	LOG_MSG(MAC_LEVEL, "--------------------rcvd TR_OK_ACK_PK(%d -> %d) from %d, rcv_ID= %d\n", rreq_dest_ID, rreq_src_ID,send_ID, rcv_ID);

	for(i =0; i< hop; i++)
	{
		trace_list[i]= TR_ok_ack_pk->trace_list[i];
	}
//	global_token_ring_state =TR_ST_INIT;
	Set_TR_State(TR_ST_INIT);

	if(rcv_ID == Node_Attr.ID)			//1.  由于RREP是单播，自己是接收者时才接收
	{
//		LOG_MSG(MAC_LEVEL, "--------------------rcvd TR_OK_ACK_PK(%d -> %d) from %d\n", rreq_dest_ID, rreq_src_ID,send_ID);

		if(rreq_src_ID == Node_Attr.ID)	//2. RREQ的源ID是本节点
		{
			//取消第二阶段定时器
//			g_mac_timeout =FALSE;
//			LOG_MSG(MAC_LEVEL, "--------------------wdStart 1\n");
			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);
//			LOG_MSG(MAC_LEVEL, "--------------------wdStart 2\n");

		}
		else
		{
			// 4. 如果自己不是源ID，则根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
			rcv_ID 					= MAC_Get_Prev_ID(trace_list, Node_Attr.ID & 0xff);
			TR_ok_ack_pk->rcv_ID 	= rcv_ID;
			TR_ok_ack_pk->send_ID 	= Node_Attr.ID & 0xff;
			TR_ok_ack_pk->pk_crc 	= getCRC((unsigned char*)TR_ok_ack_pk, sizeof(TokenRing_OK_ACK_PK)-2);
			radio_pks_sendto(M_e32_uart_fd, (u8*)TR_ok_ack_pk , sizeof(TokenRing_OK_ACK_PK), rcv_ID);
			LOG_MSG(MAC_LEVEL, "--------------------Relay forward TR_OK_ACK_PK(%d to %d ) to %d\n",rreq_dest_ID, rreq_src_ID, rcv_ID);
		}
	}
	else
	{
		LOG_MSG(MAC_LEVEL, "--------------------rcvd TR_OK_ACK_PK, not for me\n");
	}
	return 0;
}

void TokenRing_OK_ACK_Send(u8 rreq_src_id, u8 rreq_dest_id)
{
	TokenRing_OK_ACK_PK 	send_pk;
	MAC_RREQ_RECORD			*record_ptr;
	int						record_pos;
	int						i;

	/* 1. 从BC_REQ_RECORD表中查找src_ID---> dest_ID(本节点)的记录项，
	 * 	  提取路径id列表得到逆向路径,用于给RREP报文赋值
	 * 2、根据src_ID\dest_ID\rreq_seq查找表
	 */
	if (-1== (record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, rreq_src_id, rreq_dest_id, 0)))
	{
		//如果src_Id ---> dest_ID没有路径，则直接返回
		return;
	}

	record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	// 2. 封装报文
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type 	= TOKEN_RING_OK_ACK_PK_TYPE;
	send_pk.reserved 	= 0;
	send_pk.send_ID 	= self_node_id & 0xff;

	send_pk.rcv_ID 		= MAC_Get_Prev_ID(record_ptr->trace_list, (u8)(Node_Attr.ID & 0xff));
	send_pk.rreq_src_ID = rreq_src_id;
	send_pk.rreq_dest_ID= rreq_dest_id;
	send_pk.rreq_seq 	= record_ptr->rreq_seq;
	send_pk.hop			= record_ptr->hop;				//将跳数也返回给源节点

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		send_pk.trace_list[i] = record_ptr->trace_list[i] & 0xff;
	}

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(TokenRing_OK_ACK_PK)-2);

	// 4. LOG 打印
	LOG_MSG(MAC_LEVEL,"send TR_OK_ACK_PK(%d -> %d)to %d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID);

	radio_pks_sendto(M_e32_uart_fd, (u8*)&send_pk , sizeof(TokenRing_OK_ACK_PK), send_pk.rcv_ID);
	return ;
}


/** Handle_rcv_token_rep() -- 收到TokenRing_REP_PK后的处理。
	处理：
		1. rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发，中间节点不维护该条路径。
		2. 若自己为源节点，则不再转发，仅更新IERP。而且需要开启状态锁，在无线端的发送缓冲区内提取报文发送
*/
int Handle_rcv_token_rep(TokenRing_Rep_PK *TR_rep_pkptr)
{
	MAC_ONE_HOP_NB_ITEM		*new_1Hop_elem_ptr;
	MAC_MULTI_HOP_NB_ITEM	*new_2Hop_elem_ptr;
	MAC_IERP_ITEM			*ierp_ptr;

	int ret;

	u8	send_ID				=TR_rep_pkptr->send_ID;
	u8	rcv_ID				=TR_rep_pkptr->rcv_ID;
	u8	rreq_src_ID			=TR_rep_pkptr->rreq_src_ID;
	u8	rreq_dest_ID		=TR_rep_pkptr->rreq_dest_ID;
	u16	rreq_seq			=TR_rep_pkptr->rreq_seq;
	u8	hop					=TR_rep_pkptr->hop;			//hop值为跳数+1
	u8	dest_msgList_ack	=TR_rep_pkptr->msgList_ack;

	u8	trace_list[MAC_MAX_HOP]={0};
	int	i;

	Set_TR_State(TR_REP_RCV);

	// 2. 解析rcv_ID，src_ID，dest_ID等
	for(i =0; i< hop; i++)
	{
		trace_list[i]= TR_rep_pkptr->trace_list[i];
	}

	LOG_MSG(MAC_LEVEL, "rcvd TR_Rep_PK(%d -> %d) from %d, dest_msgList_ack = %d \n", rreq_dest_ID, rreq_src_ID,send_ID, dest_msgList_ack);

	if(rcv_ID == Node_Attr.ID)			//1.  由于RREP是单播，自己是接收者时才接收
	{
		if(rreq_src_ID == Node_Attr.ID)	//2. RREQ的源ID是本节点
		{
			 //取消第1阶段定时器
//			g_mac_timeout =FALSE;
			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);

//			if(dest_TokenRingOK_ack == TRUE)
//				return 0;
			// 3.1 如果自己是RREQ的源ID，更新IERP,将APP_BUFF内目的ID是dest_ID的报文提取出来，发给WiFiSndThread的App队列
			if(hop == 2)
			{
				// 无记录，直接添加
				if(Is_MacOneHopNb(send_ID)==FALSE)
				{
					new_1Hop_elem_ptr = (MAC_ONE_HOP_NB_ITEM*)malloc(sizeof(MAC_ONE_HOP_NB_ITEM));

					if(new_1Hop_elem_ptr != NULL)
					{
						new_1Hop_elem_ptr->node_ID = send_ID;
						prg_list_insert(MAC_oneHopNb_List, new_1Hop_elem_ptr, LIST_POS_TAIL);
						LOG_MSG(MAC_LEVEL, "MAC_oneHopNb_List add new node %d\n",send_ID);
					}
					else
					{
						LOG_MSG(MAC_LEVEL, "MAC_oneHopNb_List add new node %d failed\n",send_ID);
//						pthread_mutex_unlock(&mac_LinkList_mutex);
						return -1 ;
					}
				}
			}
			else if(hop == 3)
			{
				ret = Is_MacOneHopNb(trace_list[2]);

				if(ret == FALSE && Is_MacTwoHopNb(trace_list[2] , send_ID)==FALSE)
				{
					// 无记录，直接添加
					new_2Hop_elem_ptr = (MAC_MULTI_HOP_NB_ITEM*)malloc(sizeof(MAC_MULTI_HOP_NB_ITEM));
					if(new_2Hop_elem_ptr != NULL)
					{
						new_2Hop_elem_ptr->node_ID  =  trace_list[2] ;
						new_2Hop_elem_ptr->relay_ID =  send_ID;
						new_2Hop_elem_ptr->hop 		=  2;
						prg_list_insert(MAC_twoHopNb_List, new_2Hop_elem_ptr, LIST_POS_TAIL);
						LOG_MSG(MAC_LEVEL, "MAC_twoHopNb_List add new node %d\n",trace_list[2]);
					}
					else
					{
						LOG_MSG(MAC_LEVEL, "MAC_twoHopNb_List add new node %d failed\n",trace_list[2]);
//						pthread_mutex_unlock(&mac_LinkList_mutex);
						return -1 ;
					}
				}
			}
			else
			{
				if(Is_MacIERPHopNb(rreq_src_ID , rreq_dest_ID, trace_list)==FALSE)
				{
					ierp_ptr = (MAC_IERP_ITEM*)malloc(sizeof(MAC_IERP_ITEM));
					if(ierp_ptr != NULL)
					{
						ierp_ptr->rreq_src_ID = rreq_src_ID;	//源节点

						/*
						 * 广播BC_REQ_PK 序列号
						 * */
						ierp_ptr->rreq_seq = rreq_seq;
						ierp_ptr->dest_ID = rreq_dest_ID;		//目的节点
						ierp_ptr->next_ID = send_ID;			//中继节点
						ierp_ptr->hop = hop;
						for(i =0; i< hop; i++)
						{
							ierp_ptr->trace_list[i]= trace_list[i];
						}
						prg_list_insert(MAC_IERP_List, ierp_ptr,LIST_POS_TAIL);

						LOG_MSG(MAC_LEVEL, "MAC_IERP_List add new node %d\n",send_ID);
					}
					else
					{
						LOG_MSG(MAC_LEVEL, "MAC_IERP_List add new node %d failed\n",send_ID);
//						pthread_mutex_unlock(&mac_LinkList_mutex);
						return -1 ;
					}
				}
			}

			//若某节点消息队列有消息待发送，则发送TokenRing_OK_PK
			if(dest_msgList_ack== TRUE)
			{
				TokenRing_OK_PK_Send((u8)(rreq_dest_ID & 0xff));
//				g_mac_timeout =FALSE;
				//启动第二阶段定时器
				/*
				 * 收到REP_PKT后，发送OK_PKT,然后启动等待OK_ACK_PKT超时定时器
				 * */
//				wdStart(wd_MAC, SLICE_TIMEOUT_MS,TIMER_ONE_SHOT);
				wdStart(wd_MAC, ROUTE_AND_OK_ACK_TIMEOUT_MS, TIMER_ONE_SHOT);
			}
			else
			{
				//若某节点回复无消息，则状态回到TR_ST_INIT
//				global_token_ring_state = TR_ST_INIT;
				LOG_MSG(MAC_LEVEL, "--------------------Reply no MSG to send \n");
#ifdef NOT_USED_OK_ACK
				Set_TR_State(TR_ST_INIT);
#endif
				Set_TR_State(TR_ST_INIT);
			}
		}
		else
		{
			// 4. 如果自己不是源ID，则根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
			rcv_ID 					= MAC_Get_Prev_ID(trace_list, Node_Attr.ID & 0xff);
			TR_rep_pkptr->rcv_ID 	= rcv_ID;
			TR_rep_pkptr->send_ID 	= Node_Attr.ID & 0xff;
			TR_rep_pkptr->pk_crc 	= getCRC((unsigned char*)TR_rep_pkptr, sizeof(TokenRing_Rep_PK)-2);

			LOG_MSG(MAC_LEVEL, "--------------------Relay forward TR_Rep_PK(%d to %d ) to %d\n",rreq_dest_ID, rreq_src_ID, rcv_ID);

			radio_pks_sendto(M_e32_uart_fd, (u8*)TR_rep_pkptr , sizeof(TokenRing_Rep_PK), rcv_ID);
		}
	}
	else
	{
		LOG_MSG(MAC_LEVEL, "rcvd TR_Rep_PK(%d-> %d), not for me\n",send_ID, rcv_ID);
	}

//	pthread_mutex_unlock(&mac_LinkList_mutex);

	return 0;
}


/**
 *
 *  TokenRing_REP_PK_Send() -- 目的节点向RREQ的源节点发送RREP报文，逆向单播发送。从RREQ_Record表中找到该记录，封装RREP发送。
	输入：src_id为RREQ的源节点，这与通常的源地址定义不同，是为了简化操作，便于查找RREQ表
	rreq_seq为0时，代表字段无效

*/
void TokenRing_REP_PK_Send(u8 rreq_src_id, u8 rreq_dest_id, u16 rreq_seq)
{
	TokenRing_Rep_PK 	send_pk;
	MAC_RREQ_RECORD		*record_ptr;
	int					record_pos;
	int					i;
	struct msqid_ds 	ds;

	snd_msgqid = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(snd_msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"uartWireless_query_and_send_to_routine WLAN_SEND_CQ not found\n");
		exit(-1);
	}
	/* 1. 从BC_REQ_RECORD表中查找src_ID---> dest_ID(本节点)的记录项，
	 * 	  提取路径id列表得到逆向路径,用于给RREP报文赋值
	 *
	 */
	if (-1== (record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, rreq_src_id, rreq_dest_id, rreq_seq)))
	{
		//如果src_Id ---> dest_ID没有路径，则直接返回
        LOG_MSG(MAC_LEVEL, "--------------------------------TokenRing_REP_PK_Send no route(rreq_src_id[%d]-> rreq_dest_id[%d], seq=%d)\n",rreq_src_id, rreq_dest_id, rreq_seq );

		return;
	}

	record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	// 2. 封装报文
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type 	= TOKEN_RING_REP_PK_TYPE;
	send_pk.reserved 	= 0;

	send_pk.send_ID 	= self_node_id & 0xff;

	//获得路径列表中上一跳节点ID ,作为目的/中间接收节点。
	send_pk.rcv_ID 		= MAC_Get_Prev_ID(record_ptr->trace_list, (u8)(Node_Attr.ID & 0xff));

	send_pk.rreq_src_ID = rreq_src_id;
	send_pk.rreq_dest_ID= rreq_dest_id;

	/*
	 * 广播BC_REQ_PK 序列号
	 * */
	send_pk.rreq_seq 	= record_ptr->rreq_seq;

//	send_pk.path_QOS 	= record_ptr->path_QOS;
	send_pk.hop			= record_ptr->hop;				//将跳数也返回给源节点

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		send_pk.trace_list[i] = record_ptr->trace_list[i] & 0xff;
	}

	/*
	 * 判断消息队列中是否有消息待发送
	 * */
#ifndef DEF_GET_MSG_NUM

	memset((u8*)&g_msgitem, 0, sizeof(struct q_item));
	msgLen =0;
	msgLen = msgrcv(snd_msgqid,&g_msgitem,MAX_MSG_LEN,0,IPC_NOWAIT);
	g_msgitem_len =msgLen;

	if(errno== ENOMSG)
	{
		g_msgitem_len =0;
		//向中心控制节点回复ACK-无消息
		send_pk.msgList_ack	 = FALSE;
	}
	else
	{
		if(msgLen > 0 )
		{
			//向中心控制节点回复ACK-有消息
			send_pk.msgList_ack	 = TRUE;
		}
		else
		{
			//向中心控制节点回复ACK-无消息
			send_pk.msgList_ack	 = FALSE;
		}
	}

#else
	memset(&ds,0,sizeof(ds));
	msgctl(msgQ_Wifi_Snd,IPC_STAT,&ds);
	if(ds.msg_qnum > 0 )
	{
		//向中心控制节点回复ACK-有消息
		send_pk.msgList_ack	 = TRUE;
	}
	else
	{
		//向中心控制节点回复ACK-无消息
		send_pk.msgList_ack	 = FALSE;
	}
#endif

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);
	// 4. LOG 打印
	LOG_MSG(MAC_LEVEL,"send TR_Rep_PK(%d -> %d)to %d, msg_qnum=%d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID, ds.msg_qnum);

	Set_TR_State(TR_REP_SEND);

	//由于回复TokenRing_Rep_PK属于点对点传输过程，因此直接调用radio_pks_sendto()...
	radio_pks_sendto(M_e32_uart_fd, (u8*)&send_pk , sizeof(TokenRing_Rep_PK), send_pk.rcv_ID);


//	LOG_MSG(MAC_LEVEL,"send TR_Rep_PK(%d -> %d)to %d, g_msgitem_len=%d, errno=%d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID,g_msgitem_len, errno);
}

/*
 * 处理接收到TokenRing_BC_Req_PK数据包流程
 *
 * 由于主控节点在发送BC_REQ_PK报文后，并不希望再次收到BC_REQ_PK，
 * 即使在下一个时间片收到了来自其它节点转发的BC_REQ_PK，也做丢弃处理;
 * 主控节点发送BC_REQ_PK报文后，期待REP_PK的到来。
 * */

int Handle_rcv_tokenRing_bc_req(TokenRing_BC_Req_PK *bc_req_pkptr)
{
	u8	src_ID 	=bc_req_pkptr->src_ID;
	u8	dest_ID	=bc_req_pkptr->dest_ID;
	u8	send_ID =bc_req_pkptr->send_ID;
	u16 rreq_seq=bc_req_pkptr->rreq_seq;
	int	update_res;

#ifdef NOT_USED_OK_ACK
	Set_TR_State(TR_ST_INIT);
#endif

	if(send_ID != Node_Attr.ID && src_ID == Node_Attr.ID)
	{
		LOG_MSG(MAC_LEVEL, "--------------------------------discard myself send TR_BC_Req_PK(%d -> %d, seq=%d) from %d\n",src_ID, dest_ID, rreq_seq, send_ID);
		return 0;	/*自己发送的BC_REQ,被自己重复收到，则丢弃*/
	}
	LOG_MSG(MAC_LEVEL, "--------------------------------rcv TR_BC_Req_PK(%d -> %d, seq=%d) from %d\n",src_ID, dest_ID, rreq_seq, send_ID);

	//判断TokenRing_BC_Req_PK目的节点是否为本节点
    if(dest_ID == Node_Attr.ID)
    {
    	/*
    	 * 1> 由于TokenRing_BC_Req_PK是广播发送，因此需要通过序列号
    	 * 避免重复处理同一个数据包
    	 *
    	 * 2> 从TokenRing_BC_Req_PK报文中提取从源节点到中继/目的节点的路径列表，
    	 *
    	 * 3> 更新TOKEN_RING_BC_REQ_Record_List，判断是否接收到新的广播包，若是需要新建并插入新的记录项;
    	 * */
    	g_BC_Req_rcvd = TRUE;

		update_res = Update_TokenRing_BC_REQ_Tab(bc_req_pkptr);
		if(update_res == 1)
		{
			//接收到新的发送给本节点的TokenRing_BC_Req_PK报文，则回复REP_PK
			TokenRing_REP_PK_Send(src_ID, dest_ID, bc_req_pkptr->rreq_seq);
		}
		else
		{
			LOG_MSG(MAC_LEVEL, "not new TR_BC_Req_PK, discard it\n");
		}

    }//if(dest_ID == Node_Attr.ID)
    else
    {
        if(bc_req_pkptr->TTL >0)
        {
            update_res = Update_TokenRing_BC_REQ_Tab(bc_req_pkptr);
        	//达到只转发一次相同序列号的BC_REQ报文，
            if(update_res == 1)
            {
            	bc_req_pkptr->TTL--;
            	bc_req_pkptr->send_ID = Node_Attr.ID & 0xff;
            	bc_req_pkptr->trace_list[bc_req_pkptr->hop] = Node_Attr.ID & 0xff;
            	bc_req_pkptr->hop++;

            	bc_req_pkptr->pk_crc = getCRC((unsigned char*)bc_req_pkptr,  sizeof(TokenRing_BC_Req_PK)-2);
            	//radio_pks_sendto(M_e32_uart_fd, (u8*)bc_req_pkptr , sizeof(TokenRing_BC_Req_PK), 255);
				msgQ_snd(msgQ_Wifi_Snd, (u8*)bc_req_pkptr,  sizeof(TokenRing_BC_Req_PK) ,WLAN_ROUTE_CTRL, NO_WAIT);
            	// LOG 打印
                LOG_MSG(MAC_LEVEL, "--------------------------------add new TR_BC_Req_PK(seq=%d) to msgQ_Wifi_Snd\n",rreq_seq );
            }
            else
            {
                LOG_MSG(MAC_LEVEL, "--------------------------------not new, discard TR_BC_Req_PK\n");
            }
        }
        else
        {
            LOG_MSG(MAC_LEVEL, "--------------------------------not forward TR_BC_Req_PK due to Dead TTL (=%d) \n",bc_req_pkptr->TTL);
        }
    }
	return 0;
}

/*
 * 广播发送TokenRing_BC_Rep_PK报文
 * */
void TokenRing_BC_REQ_PK_Send(u8 dest_id )
{
	// 1. 封装报文
	TokenRing_BC_Req_PK send_pk;
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type = TOKEN_RING_BC_REQ_PK_TYPE;
	send_pk.TTL 	= MAC_MAX_HOP -1 ;

	send_pk.reserved1 =0;
	send_pk.send_ID = self_node_id & 0xff;

	//广播发送RREQ
	send_pk.reserved2 =0;
	send_pk.rcv_ID = 255;						//广播发送RREQ

	send_pk.src_ID = self_node_id & 0xff;

	//RREQ的目的节点id
	send_pk.dest_ID  = dest_id;					//RREQ的目的id
	send_pk.rreq_seq = MAC_SEQ_INC(BC_REQ_Seq); //生成rreq_seq序列号

//	send_pk.path_QOS = 16;
	send_pk.hop = 1;				//发送TokenRing_BC_Req_PK源节点将hop置为1

	send_pk.trace_list[0] = self_node_id & 0xff;		//

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(TokenRing_BC_Req_PK)-2);

	// 2. 更新本地RREQ表，加入本条记录。以识别邻居转发来的该报文

	/*
	 * 主控节点添加TOKEN_RING_BC_REQ_PK_TYPE，避免收到同一序号报文后重复转发
	 * */
	Update_TokenRing_BC_REQ_Tab(&send_pk);

//	global_token_ring_state =TR_BC_REQ_SEND;
	Set_TR_State(TR_BC_REQ_SEND);
    LOG_MSG(MAC_LEVEL,"--------------------send TR_BC_REQ_PK(Dest_ID=%d,seq=%d) from node%d \n",dest_id,BC_REQ_Seq,self_node_id);

	// 3. 发给WiFiSndThread的Route控制队列
	radio_pks_sendto(M_e32_uart_fd, (u8*)&send_pk , sizeof(TokenRing_BC_Req_PK), 255);

	g_mac_timeout= FALSE;
	wdStart(wd_MAC, BC_REP_TIMEOUT_MS, TIMER_ONE_SHOT);
}

int Is_MacOneHopNb(u8 id){
	int				i = 1;
	MAC_ONE_HOP_NB_ITEM	*element_ptr;
	while(NULL != (element_ptr = (MAC_ONE_HOP_NB_ITEM*)prg_list_access(MAC_oneHopNb_List, i)))
	{
		if(element_ptr->node_ID == id )
		{
			return 1;
		}
		i++;
	}
	return 0;
}

/*
 * 判断<node_ID, relay_ID>是否在2跳邻居列表中
 *
 * */
int Is_MacTwoHopNb(u8 node_ID,u8 relay_ID)
{
	int						i = 1;
	MAC_MULTI_HOP_NB_ITEM	*element_ptr;
	while(NULL != (element_ptr = (MAC_MULTI_HOP_NB_ITEM*)prg_list_access(MAC_twoHopNb_List, i)))
	{
		if(element_ptr->node_ID == node_ID &&  element_ptr->relay_ID == relay_ID)
		{
			return 1;
		}
		i++;
	}
	return 0;
}

int Is_MacIERPHopNb(u8 rreq_src_ID , u8 rreq_dest_ID,  u8* trace_list)
{
	int						i = 1;
	MAC_IERP_ITEM	*element_ptr;
	while(NULL != (element_ptr = (MAC_IERP_ITEM*)prg_list_access(MAC_IERP_List, i)))
	{
		if(element_ptr->rreq_src_ID == rreq_src_ID && \
				element_ptr->dest_ID == rreq_dest_ID && \
				element_ptr->trace_list[0] == trace_list[0] && \
				element_ptr->trace_list[1] == trace_list[1] && \
				element_ptr->trace_list[2] == trace_list[2] && \
				element_ptr->trace_list[3] == trace_list[3])
		{
			return 1;
		}
		i++;
	}
	return 0;
}

/*
 * @function
 * 		Handle_rcv_token_ring_req
 * @brief:
 * 		单播情况下接收到REQ_PK, 执行如下处理：
 * 		1》由于多跳中继场景，需考虑是否为发送给本节点的TokenRing_Req_PK; 若不是但为中继节点，则转发该报文;
 * 		2》若目的节点是本节点，则单播回复TokenRing_REP
 * */
int Handle_rcv_token_ring_req(TokenRing_Req_PK* req_pkptr)
{
	u8	i=0;
	// 2. 根据报头格式，解析出源、目的IP地址 和 TTL，bc_seq等信息; 注意，报文内容为网络字节序
	u8 rcv_ID 		= req_pkptr->rcv_ID;
	u8 src_ID 		= req_pkptr->src_ID;
	u8 dest_ID 		= req_pkptr->dest_ID;
	u16 rreq_seq 	= req_pkptr->rreq_seq;
	u8	trace_list[MAC_MAX_HOP];

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		trace_list[i] = req_pkptr->trace_list[i];
	}

	/*
	 * 以下为点对点传输，多跳中继情况下，通过trace_list[...]指导如何传输至下一个节点
	 * */
	Set_TR_State(TR_REQ_RCV);

	if(rcv_ID == (Node_Attr.ID & 0xff))		//中间节点是本节点否？
	{
	    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_REQ_PK(seq=%d) from src_ID(%d) to dest_ID(%d) \n",rreq_seq, src_ID, dest_ID);
    	g_BC_Req_rcvd = TRUE;

		if(dest_ID == rcv_ID)		//目的节点是否为本节点否？
		{
			//目的节点是本节点, 则回复TokenRingREP报文。。。。
			TokenRing_REP_PK_Send(src_ID,  dest_ID, 0);
		}
		else
		{
			rcv_ID = MAC_Get_Next_ID(trace_list, ((u8)Node_Attr.ID & 0xff));
			if(rcv_ID >= 0)
			{
				req_pkptr->send_ID 	= self_node_id & 0xff;
				req_pkptr->rcv_ID 	= rcv_ID;
				req_pkptr->pk_crc 	= getCRC((unsigned char*)req_pkptr, sizeof(TokenRing_Req_PK)-2);
				LOG_MSG(MAC_LEVEL,"--------------------------------forward TR_REQ to %d \n",rcv_ID);
				radio_pks_sendto(M_e32_uart_fd, (u8*)req_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);
			}
			else
			{
				LOG_MSG(MAC_LEVEL,"--------------------------------no route to forward TR_REQ to dest_ID(%d) \n",dest_ID);
			}
		}
	}//	if(rcv_ID == (Node_Attr.ID & 0xff))
	else
	{
		//4.5 接收ID不是自身，直接丢弃。
		return 0;
	}
	return 1;
}

/*@Function:
 * 		TokenRingReq_PK_Send
 *@Desc:
 * 		单播发送TokenRingReq, 该数据包在路由建立后，点对点方式发送至目的节点
 *@Param1:
 *		tokenRingReq_pkptr数据包指针
 *@Param2:
 *		dest_ID，目的接收节点
 * */
//int TokenRingReq_PK_Send( u8 dest_ID, u8 TokenRingOK_Pk)
int TokenRingReq_PK_Send( u8 dest_ID)
{
	u8  rcv_ID;
	u8	src_ID;
	u8	rreq_src_id;

	u8	hop;
	u8	trace_list[MAC_MAX_HOP];

	int	routing_state;
	int	i;

	src_ID = self_node_id;

	TokenRing_Req_PK	tokenRing_Req_pk;
	TokenRing_Req_PK 	*tokenRingReq_pkptr = &tokenRing_Req_pk;
	int req_pk_sz 		= sizeof(TokenRing_Req_PK);

	memset(&tokenRing_Req_pk, 0, req_pk_sz);

	if(dest_ID == self_node_id)
		return TRUE;

	/*调用MAC_Route_Search查找是否有src_ID-->dest_ID路径
	 *由参数@rcv_ID 返回下一跳节点ID
	 *由参数@rreq_sr_ID返回路径的源节点ID
	 *由参数@hop返回跳数
	 *由参数@trace_list返回路径节点ID数组
	 * */
	routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);

	if(routing_state >= 0)		// 有域内或域间路由
	{
		tokenRingReq_pkptr->pk_type 	= TOKEN_RING_REQ_PK_TYPE;
		/*
		 * 返回next_rcv_ID,
		 * */
		tokenRingReq_pkptr->send_ID 	= src_ID;

		//下一跳
		tokenRingReq_pkptr->rcv_ID 		= rcv_ID;		//表示到达目的节点的中间节点或目的ID本身。

		//源节点和目的节点
		tokenRingReq_pkptr->src_ID 		= src_ID;
		tokenRingReq_pkptr->dest_ID 	= dest_ID;
//		tokenRingReq_pkptr->rreq_src_ID = src_ID;
		/*
		 * rreq_seq为0时，表示域内1跳或2跳子网内有路由
		 * */
		tokenRingReq_pkptr->rreq_seq 	= MAC_SEQ_INC(REQ_Seq); //生成rreq_seq序列号;
//		tokenRingReq_pkptr->TTL 		= MAC_MAX_HOP-1;
//		tokenRingReq_pkptr->hop 		= hop;	//子网内，1跳时hop=1; 2跳时hop=2;

		/*
		 * 是否为TokenRingOK= TRUE 类型的TOKEN_RING_REQ_PK_TYPE报文
		 * */
//		tokenRingReq_pkptr->TokenRingOK_Pk_flag		=TokenRingOK_Pk;

		/*多跳情况下，携带中继路径列表
		 * */
		for(i =0; i< MAC_MAX_HOP; i++)
		{
			tokenRingReq_pkptr->trace_list[i] = trace_list[i];
		}

		tokenRingReq_pkptr->pk_crc = getCRC((unsigned char*)tokenRingReq_pkptr, sizeof(TokenRing_Req_PK)-2);

		// 5. LOG 打印
		LOG_MSG(MAC_LEVEL,"--------------------send TR_REQ_PK(seq=%d) from node%d to node%d \n",REQ_Seq, self_node_id, dest_ID);

		// 4. 发送TokenRing_Req_PK请求
		Set_TR_State(TR_REQ_SEND);
		radio_pks_sendto(M_e32_uart_fd, (u8*)tokenRingReq_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);

		//发送之后启动定时器
	    g_mac_timeout= FALSE;

	    /*
	     * 启动REP ＡＣＫ定时器
	     * */
		wdStart(wd_MAC, REP_TIMEOUT_MS, TIMER_ONE_SHOT);
		return TRUE;
	}
	else
	{
		//没有达到目的节点的路径
//		global_token_ring_state =TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);
	    g_mac_timeout= FALSE;
	    LOG_MSG(MAC_LEVEL,"--------------------no route to send TR_REQ_PK from node%d to node%d \n",self_node_id, dest_ID);
		return FALSE;
	}

}


 /*	多跳中继通信场景下集中控制轮询策略：
 *	节点1轮询节点2时：
 *	1) 首先判断节点2是否在1跳路由表内，如果在，则直接发送TokenRingReq(2);
 *	  等待MAX_TIMEOUT_MS时间接收处理节点2消息。
 *
 *	2）如果节点2不在1跳路由表内，则广播发送TokenRingBCReq(255, Seq, 2)， 此时结果场景有如下几种情况：
 *	  2.1) 刚开机，节点2在子网内,但尚未建立路由，此时收到TokenRingBCReq(255, Seq, 2)，
 *	  节点2发现是询问自己的令牌包，则回复TokenRingRep（2）,并携带有无消息待发送信息;
 *	  若有消息，则节点1会发送TokenRingOk(2),节点2收到TokenRingOk(2)指令后，则占用信道并发送1次消息;
 *
 *	  节点3/4若在子网内，此时收到TokenRingBCReq(255, Seq, 2)，节点3/4发现不是询问自己的广播令牌包，
 *	  则做转发处理，将TokenRingBCReq(255,Seq,2)添加到消息队列中;
 *	  转发的时机必须遵循节点1作为中心控制的策略;
 *
 *	  2.2）节点2不在子网内，MAX_TIMEOUT_MS超时后，节点1没有收到任何消息，则结束轮询节点2，开始轮询
 *	  节点3.
 *
 *节点1轮询节点3时：
 *	1) 首先判断节点3是否在1跳路由表内，如果在，则直接发送TokenRingReq(3);
 *	  等待MAX_TIMEOUT_MS时间接收处理节点3消息。
 *
 *	2）如果节点3不在1跳路由表内，则广播发送TokenRingBCReq(255, Seq, 3)， 此时结果场景有如下几种情况：
 *	  2.1) 刚开机，节点3在子网内,但尚未建立路由，此时收到TokenRingBCReq(255, Seq, 3)，
 *	  节点3发现是询问自己的令牌包，则回复TokenRingRep（3）,并携带有无消息待发送信息;
 *	  若有消息，则节点1会发送TokenRingOk(3),节点2收到TokenRingOk(3)指令后，则占用信道并发送1次消息;
 *
 *	  由于节点1询问节点2时，节点2尚未回复
 *
 *	  节点2/4若在子网内，此时收到TokenRingBCReq(255, Seq, 3)，节点2/4发现不是询问自己的广播令牌包，
 *	  则做转发处理，将TokenRingBCReq(255,Seq,3)添加到消息队列中;
 *	  转发的时机必须遵循节点1作为中心控制的策略;
 */

int mac_msgq_id=-1;
int wlan_snd_msgq_id=-1;

void Try_Get_Msg_Process()
{
	int				msgLen;
	unsigned char	msgBuff[MAC_MAX_MSG_LEN];	 //接收来自LanRcvThread和WiFiRcvThread的消息
	struct 			mac_q_item item;

	memset(&item,0,sizeof(item));
	//从MAC消息队列中按照先进先出原则获取消息
//	printf("--------Try_Get_MsgS_Process\n");
	msgLen = msgrcv(mac_msgq_id,&item,MAC_MAX_MSG_LEN,0,MSG_NOERROR | IPC_NOWAIT);//IPC_NOWAIT);//MSG_NOERROR);
	if(msgLen > 0)
	{
//		LOG_MSG(ERR_LEVEL,"-------------Try_Get_Msg_Process rcvd[0]= \n",msgBuff[0]);

		memcpy(msgBuff,item.data,msgLen);
		switch(msgBuff[0])
		{
			case TOKEN_RING_BC_REQ_PK_TYPE :			//处理TOKEN_RING_BC_REQ广播报文
			{
				if(getCRC(msgBuff, sizeof(TokenRing_BC_Req_PK)) !=0)
				{
					LOG_MSG(ERR_LEVEL,"-------------rcvd TokenRing_BC_Req_PK crc error \n");
					#ifdef NOT_USED_OK_ACK
						Set_TR_State(TR_ST_INIT);
					#endif
					break ;
				}
				Handle_rcv_tokenRing_bc_req((TokenRing_BC_Req_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_REQ_PK_TYPE:				//处理TOKEN_RING_REQ_PK_TYPE报文
			{
				if(getCRC(msgBuff, sizeof(TokenRing_Req_PK)) !=0)
				{
					#ifdef NOT_USED_OK_ACK
						Set_TR_State(TR_ST_INIT);
					#endif
					LOG_MSG(ERR_LEVEL,"--------------------------------rcvd TokenRing_Req_PK crc error \n");
					break ;
				}
				Handle_rcv_token_ring_req((TokenRing_Req_PK*)msgBuff);
				break ;
			}

			case  TOKEN_RING_REP_PK_TYPE:				//处理TOKEN_RING RREP消息
			{
				if(getCRC(msgBuff, sizeof(TokenRing_Rep_PK)) !=0)
				{
					#ifdef NOT_USED_OK_ACK
						Set_TR_State(TR_ST_INIT);
					#endif
					LOG_MSG(ERR_LEVEL,"--------------------------------rcvd TokenRing_Rep_PK crc error \n");
					break ;
				}
				/*
				 * 收到从站REP数据包后，根据从站的“答复：1-》有消息， 0-》无消息”，决定是否发送TR_OK_PK报文
				 * */
				Handle_rcv_token_rep((TokenRing_Rep_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_OK_PK_TYPE:					//处理TOKEN_RING OK消息
			{
				if(getCRC(msgBuff, sizeof(TokenRing_OK_PK)) !=0)
				{
					#ifdef NOT_USED_OK_ACK
						Set_TR_State(TR_ST_INIT);
					#endif
						LOG_MSG(ERR_LEVEL,"--------------------------------rcvd TokenRing_OK_PK crc error \n");
						break ;
				}
//					global_running_state = NORMAL_RUNNING;

				Handle_rcv_token_ring_OK((TokenRing_OK_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_RX_ROUTE_DATA_PK_TYPE:				//处理TOKEN_RING_OK_ACK_PK_TYPE消息
			{
				Handle_rcv_RouteData_Pkt((TokenRing_RX_RouteDATA_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_OK_ACK_PK_TYPE:				//处理TOKEN_RING_OK_ACK_PK_TYPE消息
			{
				if(getCRC(msgBuff, sizeof(TokenRing_OK_ACK_PK)) !=0)
				{
					#ifdef NOT_USED_OK_ACK
						Set_TR_State(TR_ST_INIT);
					#endif
					LOG_MSG(ERR_LEVEL,"--------------------------------rcvd TokenRing_OK_ACK_PK crc error \n");
					break ;
				}
				Handle_rcv_OK_ACK((TokenRing_OK_ACK_PK*)msgBuff);
				break ;
			}
			default:
			{
	            LOG_MSG(ERR_LEVEL, "Mac_Queue rcvd unkown_pk,msgLen = %d\n",msgLen);
				break;
			}
		}
	}

	else
	{
//        LOG_MSG(ERR_LEVEL, "Mac_Queue rcvd 0 pks,msgLen = %d\n",msgLen);
	}
}




#define MAX_TIMEOUT_CNT		(3U)

void Poll_Members_Control_Channel()
{
	u8 ret 			= FALSE;
	u8 node_i		=2;

	u8 timeouts_cnt =0;

	//首先判断某节点是否在子网内
	for(node_i =2; node_i<=4; node_i++)
	{
		g_nb_node_i =node_i;
//		printf("Poll_Members[%d]_Control_Channel\n", node_i);
		//发送TokenRing_REQ或TokenRing_BC_REQ
		ret = TokenRingReq_PK_Send(node_i);
		if(ret == TRUE)
		{
			 //STEP1> 启动第1阶段定时器, 考虑多跳时延
			do{
				 Try_Get_Msg_Process();
				 if(Get_TR_State() == TR_REQ_SEND && g_mac_timeout == FALSE)
				 {
					 //发送了REQ_PK之后，等待回复未超时
					 continue;
				 }
				 else if(Get_TR_State() == TR_REQ_SEND && g_mac_timeout == TRUE)
				 {
					 LOG_MSG(DBG_LEVEL,"----1rd\n");
					 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
					 //若超时，则需删除该目的节点在中心节点中MAC路径表信息
					 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
					 {
						Del_Mac_Route_Tb_Node(node_i);
						nb_state_list[node_i].mac_timeouts_cnt =0;
						LOG_MSG(INFO_LEVEL,"----1rd Del_Mac_Route_Tb_Node(%d)\n", node_i);
					 }

					//某节点超时未回复ACK，主站则将状态机复位至初始状态，准备询问下一个节点
//					LOG_MSG(MAC_LEVEL,"-------------------------------------------------------------Timeout rcvd OK_ACK_PK from node%d,timeouts_cnt=%d \n", node_i, timeouts_cnt);
					LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd REP_PK from node%d,Get_TR_State() =%d , timeouts_cnt=%d\n", node_i, Get_TR_State(),timeouts_cnt );

					Set_TR_State(TR_ST_INIT);
					g_mac_timeout =FALSE;
					break;
				 }
				 else
				 {
					 /*
					 * 若不超时，则主站收到REQ_PK的回复REP_PK,
					 * 然后主站会根据从站是否有消息待发送，发送OK_PK\等待用户占用信道\等待OK_ACK_PK
					 * */
					if(Get_TR_State() != TR_ST_INIT && g_mac_timeout== FALSE)
					{
						continue;
					}
					else if((Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND) && g_mac_timeout == TRUE)
					{
						LOG_MSG(DBG_LEVEL,"----2rd\n");

						//若不采用ACK，不能在第二阶段超时发生后，删除节点，
						//因为当前节点产生的业务包，可能不是发给主控节点的，而是别的节点收到了。
						//这样主控节点会在当前节点占用信道结束后，仍收不到业务包，发生超时。
						//所以，建议采用OK_ACK机制,从节点每次收到TR_OK_PK，占用信道结束后，回复TR_OK_ACK_PK
					#ifndef NOT_USED_OK_ACK
//						if(Get_TR_State() == TR_REQ_SEND)
//						if(Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND)
//						{
							 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
							 //若超时，则需删除该目的节点在中心节点中MAC路径表信息
							 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
							 {
								//若未收到node_i OK_ACK回复，则删除该邻居节点
								Del_Mac_Route_Tb_Node(node_i);
								nb_state_list[node_i].mac_timeouts_cnt =0;
								LOG_MSG(INFO_LEVEL,"----2rd Del_Mac_Route_Tb_Node(%d)\n", node_i);
							 }
							 LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd OK_ACK_PK from node%d,Get_TR_State() =%d , timeouts_cnt=%d\n", node_i, Get_TR_State(),timeouts_cnt );
//
//						}
					#endif
						g_mac_timeout =FALSE;
						Set_TR_State(TR_ST_INIT);
						break;
					}
					else
					{
						//该状态下，主控节点恢复到TR_ST_INIT，说明收到OK_ACK, 直接询问下一个节点。
//						printf("------------------round over1\n");
						Set_TR_State(TR_ST_INIT);
						nb_state_list[node_i].mac_timeouts_cnt=0;
						break;
					}
				 }
			}while(1);
		}
		else
		{
			//开启广播询路。。。。。
			TokenRing_BC_REQ_PK_Send(node_i);

			//开启第一阶段定时器
			do{
				Try_Get_Msg_Process();
				//等待1跳子网内节点（node_i）回复BC_REP_PK消息
				if(Get_TR_State() == TR_BC_REQ_SEND && g_mac_timeout == FALSE)
				{
					continue;
				}
				else if(Get_TR_State() == TR_BC_REQ_SEND && g_mac_timeout== TRUE )
				{
					//某节点超时未回复ACK，主站则将状态机复位至初始状态，准备询问下一个节点
					LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd TR_BC_REP_PK from node%d\n", node_i);

					Set_TR_State(TR_ST_INIT);
					g_mac_timeout =FALSE;
					break;
				}
				else
				{
					/*
					 * 发送BC_REQ后，收到REP_PK,
					 * 然后主站会根据从站是否有消息待发送，发送OK_PK\等待用户占用信道\等待OK_ACK_PK
					 * */
					//启动第二阶段定时器
					if(Get_TR_State() != TR_ST_INIT && g_mac_timeout == FALSE)
					{
						continue;
					}
					else if((Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND) &&g_mac_timeout == TRUE)
					{
						 LOG_MSG(DBG_LEVEL,"----2rd\n");
						 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
						 //若超时，则需删除该目的节点在中心节点中MAC路径表信息
						 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
						 {
							//若未收到node_i OK_ACK回复，则删除该邻居节点
							Del_Mac_Route_Tb_Node(node_i);
							nb_state_list[node_i].mac_timeouts_cnt =0;
							LOG_MSG(INFO_LEVEL,"----2rd Del_Mac_Route_Tb_Node(%d)\n", node_i);
						 }
						 LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd OK_ACK_PK from node%d,Get_TR_State() =%d , timeouts_cnt=%d\n", node_i, Get_TR_State(),timeouts_cnt );

						Set_TR_State(TR_ST_INIT);
						g_mac_timeout =FALSE;
						break;
					}
					else
					{
						//该状态下，主控节点恢复到TR_ST_INIT，说明收到OK_ACK, 直接询问下一个节点。
						Set_TR_State(TR_ST_INIT);
						g_mac_timeout =FALSE;
						break;
					}
				}
			}while(1);
		}
	}
	return ;
}

/*
 *@Function:
 *		Slaves_Mac_ThreadFun
 *@Desc:
 *		线程主函数,实现阻塞读取MAC消息队列
 * */

void* Slaves_Mac_ThreadFun(void *args)
{
	for(;;)
	{
		if(g_UPG_on == TRUE)
		{
			usleep(1000);
			continue;
		}
		 Try_Get_Msg_Process();
	}
}

/*
 * @Function:
 * 		Master_Mac_ThreadFunAll
 * @Desc:
 * 		主控节点MAC报文线程处理函数
 * */
void* Master_Mac_ThreadFunAll(void *args)
{
	int 			msglen;
	struct q_item 	msgitem;
	memset((u8*)nb_state_list, 0, sizeof(nb_state_list));
	for(;;)
	{
		/*Step1> 大船主控节点询问自身是否有消息待发送
		 * 需要考虑多跳中继场景下信道占用的开销
		 * 即下一跳节点收到主控节点的业务数据报文后，给予ＡＣＫ回复的时延。
		 * */

		if(g_UPG_on == TRUE)
		{
			usleep(1000);
			continue;
		}

		 while(Get_TR_State() != TR_ST_INIT );
		 {usleep(1000);}
		//取消mac定时器
		wdStart(wd_MAC, 0, TIMER_ONE_SHOT);

		 /*
		  * MSG_NOERROR: no error if message is too big
		  * */
		msglen = msgrcv(wlan_snd_msgq_id,&msgitem,512,-2,MSG_NOERROR | IPC_NOWAIT);//IPC_NOWAIT);

		if(msglen > 0)
		{
			snd_msgTo_radio(&msgitem, msglen);
		}
//		LOG_MSG(MAC_LEVEL,"wlan_snd_msgq_id msgrcv--2\n");
		/*Step2> 轮询其它节点是否有消息发送
		 * 需要考虑多跳中继场景下信道占用的开销，
		 * 即下一跳节点经过多跳收到主控节点的业务数据报文后，给予ＡＣＫ回复的时延。
		 * */
		Poll_Members_Control_Channel();
	}
}

/*
 * 集中式控制访问信道
 * */
void MAC_TimeOut_Handler(int sig, siginfo_t *si, void *uc)
{
	g_mac_timeout = TRUE;
}

/*
 * 创建mac线程
 * */
pthread_t create_mac_thread()
{
	pthread_t tid;


	int ret;
	prg_list_delete(MAC_oneHopNb_List);
	prg_list_delete(MAC_twoHopNb_List);
	prg_list_delete(MAC_IERP_List);

	if(NULL == (MAC_oneHopNb_List = prg_list_create()))
		return 0;
	if(NULL == (MAC_twoHopNb_List = prg_list_create()))
		return 0;
	if(NULL == (MAC_IERP_List = prg_list_create()))
		return 0;

	if(NULL == (TOKEN_RING_BC_REQ_Record_List = prg_list_create()))
	{
		LOG_MSG(MAC_LEVEL, "TOKEN_RING_BC_REQ_Record_List error!\n");
		return 0;
	}

	//获取Mac层消息队列，从消息队列中取MAC消息处理
	ret = msgget(MAC_QUEUE,IPC_EXCL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"Get MAC_QUEUE ERR\n");
		exit(-1);
	}else
	{
		mac_msgq_id = ret;
		LOG_MSG(MAC_LEVEL, "create_mac_thread----------mac_msgq_id =%d\n", mac_msgq_id);
	}


	//获取Route层消息队列，从消息队列中获取待发送的报文
	wlan_snd_msgq_id = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(wlan_snd_msgq_id < 0)
	{
		LOG_MSG(ERR_LEVEL,"Mac_ThreadFunAll WLAN_SEND_CQ not found\n");
		exit(-1);
	}

	if(self_node_id == 1)
	{
		ret = wdCreate(MAC_TIMEOUT_SIG, &wd_MAC, &MAC_TimeOut_Handler);
		if(ret== -1)
		{
			exit(-1);
		}
		/*
		 *注册mac层线程函数，处理通过无线接收到的MAC控制报文
		 * */
		ret = pthread_create(&tid,NULL,&Master_Mac_ThreadFunAll,NULL);
		if(ret < 0)
		{
			LOG_MSG(ERR_LEVEL,"create Mac_Thread\n");
			exit(-1);
		}

		//TODO 考虑节点0的情况
//		nb_state_list =(MAC_NB_State_t*) malloc(NET_MAX_NODES*sizeof(MAC_NB_State_t));
	}

	else

	{
		/*
		 *从节点注册mac层线程函数，处理通过无线接收到的MAC控制报文
		 * */
		ret = pthread_create(&tid,NULL,&Slaves_Mac_ThreadFun,NULL);
		if(ret < 0)
		{
			LOG_MSG(ERR_LEVEL,"create Mac_Thread\n");
			exit(-1);
		}
	}
	return tid;
}


