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


List*	MAC_oneHopNb_List = NULL;		// һ���ھ�����
List*	MAC_twoHopNb_List = NULL;		// �����ھ�����
List*	MAC_IERP_List = NULL;			// ���·������

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
 * ����MAC������ѯ�㷨��״̬������ֵ��mac.c��routing.c�ж�����߳̿���ͬʱ���޸ģ������߳�ͬ�����⡣
 * ��˱��뿼��Linux�߳�ͬ���뻥�⡣
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
 * @FUN��
 * 		MAC_Route_Search
 * @Desc��
 * 		�����Ƿ���src_ID-->dest_ID·��
 *		�ɲ���@rcv_ID ������һ���ڵ�ID
 *		�ɲ���@rreq_sr_ID����·����Դ�ڵ�ID
 *		�ɲ���@hop��������
 *		�ɲ���@trace_list����·���ڵ�ID����
 *	@RETURN
 *		-1��ʧ����·��
 *		0 ���ɹ�����·��
 * */
int MAC_Route_Search(u8 dest_id,u8 *next_idptr,u8 *rreq_src_idptr, u8*hopptr, u8 *trace_list)
{
	int					rreq_seq = -1;
	int					i = 1;
	MAC_ONE_HOP_NB_ITEM		*element_ptr1;
	MAC_MULTI_HOP_NB_ITEM	*element_ptr2;
	MAC_IERP_ITEM			*element_ptr3;

	*hopptr = 0;

	// 1. ��Ŀ��Ϊ�㲥����ֱ�ӷ��ͣ���һ��Ҳ�ǹ㲥
	if(dest_id == 0xff)
	{
		*next_idptr = 0xff;
		*rreq_src_idptr = 0xff;
		*hopptr = MAC_MAX_HOP-1;
		return 0;
	}
	// 2. ����һ���ھӱ���node_ID == dest_id ��˫���ͨ��������ڣ�ֱ�ӷ��ص�һ�����
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
		return 0;		// ����·��
	}

	// 3. ����2���ھӱ���node_ID == dest_id��������ڣ�ֱ�ӷ��ص�һ�����
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
		return 0;		// ����·��
	}


	// 4. ��������·�ɱ��� dest_ID == dest_id��������ڣ�ֱ�ӷ��ص�һ�����
	i = 1;
	while(NULL != (element_ptr3 = (MAC_IERP_ITEM*)prg_list_access(MAC_IERP_List, i)))
	{
		if(element_ptr3->dest_ID == dest_id )
		{
			if(*hopptr <= element_ptr3->hop )		// ����ѣ�hop��С����ѡQOS��󣩵�·��
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
		return rreq_seq;		// ���·��
	}


	return -1;
}


/* Get_Next_ID()  ----- ��trace_list����ȡ id ����һ��ID��û�ҵ��������������-1
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


/* ���ݽ��յ���RREQ���ģ�����RREQ��¼��
	RREQ��¼���ܽ�����rreq_src_ID��seq���֣���Ҫ����rreq_dest_ID,��Ϊһ���ڵ�϶�������Ŀ��Ѱ·������ͬĿ��ID��Ѱ·���̣����Ƚ�seq����ȷ���¾�
	1. ����src_ID��dest_ID����RREQ_Record��¼��
	2. ����¼�����޴˼�¼�����½�һ���ڴ棬��ֵ�����RREQ����ĩβ��������1��
	3. ���м�¼����������������¸ü�¼
			�������еĵ�rreq_seq���£������REQ_Record��������1��
			��rreq_seq��ͬ�����±��ĵ�hop��С�������REQ_Record��������1��
								hop��ͬ��ϴ󣬲�����0��
			�������еĵ�rreq_seq�Ͼɣ�������0��

	����ֵ��0��δ�������£�1����������

    ע�����hop,trace_list,path_QOS.���뱾�ڵ��Ӱ��
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

	// 1. �ڵ��յ��󣬽�����src_ID,dest_ID,rreq_seq��hop;
	src_ID 		= bc_req_pkptr->src_ID;
//    send_ID 	= bc_req_pkptr->send_ID;
	dest_ID 	= bc_req_pkptr->dest_ID;
	rreq_seq 	= bc_req_pkptr->rreq_seq;
	hop 		= bc_req_pkptr->hop;


	// 2. ����src_ID\dest_ID\rreq_seq����RREQ_RECORD���ҵ���һ����¼��
	/*
	 * ���ǲ�ͬ�ڵ㿪��ʱ�̲�һ�µ������
	 * 1������ڵ�2�ȿ�������ʱ
	 * */

	record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, src_ID,dest_ID, rreq_seq);	//����RREQ�����Դ�ڵ�id��Ŀ��id������RREQ����
	record_ptr = (MAC_RREQ_RECORD *)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	if(record_pos == -1)
	{
		// 3. ����¼�����޴˼�¼�����½�һ���ڴ棬��ֵ�����RREQ����ĩβ��������1��
//        LOG_MSG(MAC_LEVEL, "new RREQ record\n");
		record_ptr = (MAC_RREQ_RECORD*)malloc(sizeof(MAC_RREQ_RECORD));

		record_ptr->rreq_src_ID = src_ID;
		record_ptr->rreq_dest_ID = dest_ID;
		record_ptr->rreq_seq = rreq_seq;
//		record_ptr->path_QOS = path_qos;
		record_ptr->hop = hop+1;

		//��RREQ����ڵ������ڵ�·�����棬�������ڵ�id��Ϣ׷����·��ĩβ
		for(i = 0; i< hop;i++)
		{
			record_ptr->trace_list[i] = bc_req_pkptr->trace_list[i];
		}

		//hop+1��¼���Ǳ��ڵ�ID
		record_ptr->trace_list[hop] = Node_Attr.ID;	 	// ĩβ���ϱ���ID

		prg_list_insert(TOKEN_RING_BC_REQ_Record_List, record_ptr, LIST_POS_TAIL);

		return 1;
	}
	else
	{
		// 4. ���м�¼���������ڵ�rreq_seq���£���rreq_seq��ȵ��±��ĵ��������٣���rreq_seq��hop����ȵ�path_QOS �ϴ�ʱ��
        //              ����RREQ����ĵ�record_pos��Ԫ��,������1��
//        LOG_MSG(MAC_LEVEL,"pk/record = seq=[%d, %d], hop=[%d,%d]\n",rreq_seq,record_ptr->rreq_seq,hop,record_ptr->hop-1);
		cmp_res = Seq_Cmp(rreq_seq, record_ptr->rreq_seq);

		if(cmp_res != 0 || (cmp_res == 0 && hop < record_ptr->hop-1))
		{
			record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

			record_ptr->rreq_src_ID = src_ID;
			record_ptr->rreq_dest_ID = dest_ID;
			record_ptr->rreq_seq = rreq_seq;
			record_ptr->hop = hop+1;        // hop+1 ����

			for(i = 0; i< hop;i++)
			{
				record_ptr->trace_list[i] = bc_req_pkptr->trace_list[i];
			}

			record_ptr->trace_list[hop] = Node_Attr.ID;     // ĩβ���ϱ���ID

			return 1;

		}
		else
		{
			// 5. �����������������0
			return 0;
		}

	}
}

/* Get_Prev_ID()  ----- ��trace_list����ȡ id ����һ��ID��û�ҵ��������������0
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
	// 2. ���ݱ�ͷ��ʽ��������Դ��Ŀ��IP��ַ �� TTL��bc_seq����Ϣ; ע�⣬��������Ϊ�����ֽ���
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
	 * ����Ϊ��Ե㴫�䣬�����м�����£�ͨ��trace_list[...]ָ����δ�������һ���ڵ�
	 * */
//    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_OK_PK from node%d to node%d, rcv_ID=%d \n",src_ID, dest_ID, rcv_ID);

	if(rcv_ID == (Node_Attr.ID & 0xff))		//�м�ڵ��Ǳ��ڵ��
	{
		if(dest_ID == rcv_ID)		//Ŀ�Ľڵ��Ƿ�Ϊ���ڵ��
		{
			/*
			 *���Ƿ������ڵ�ı��ģ���ظ�TokenRingREP���ġ�������
			 * */
		    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_OK_PK from node%d to node%d, rcv_ID=%d \n",src_ID, dest_ID, rcv_ID);

#ifdef DEF_GET_MSG_NUM
			memset((u8*)&g_msgitem, 0, sizeof(struct q_item));
			g_msgitem_len = msgrcv(snd_msgqid,&g_msgitem,MAX_MSG_LEN,-2,MSG_NOERROR |IPC_NOWAIT);
#endif
			snd_msgTo_radio(&g_msgitem, g_msgitem_len);

#ifndef NOT_USED_OK_ACK
			usleep(5000);
			/* ʱ�䲻��̫�󣬷�������վ��ʱ����վ�����TR_INIT_ST,Ȼ������Ϣ��������Ϣ��
				��ʱ������վ�ٷ���OK_ACK_PKT���ᵼ���ŵ���ײ��ͻ��
			*/
			/*
			 * 2018-07-24 �����ؽڵ����ҵ�һ���ھ�
			 * */
//			if(Is_1hopNb(src_ID) ==0)
//			{
			TokenRing_OK_ACK_Send(src_ID,  dest_ID);
//			}
#endif
		}
		else if(rreq_seq == 0)
		{
			// 4.2 ���սڵ���������Ϊ����·�ɣ����IARP����һ��
			rcv_ID = MAC_Get_Next_ID(trace_list, ((u8)Node_Attr.ID & 0xff));		//�����ϣ��˴���Ȼ�ܹ�����rcv_ID�����򣬾���Դ�˷�װ·��ͷʱ������
			if(rcv_ID >= 0)
			{
				// 4.2.1 ������·��, �޸��շ���ַ�����¼���CRC
				OK_pkptr->send_ID = self_node_id;
				OK_pkptr->rcv_ID  = rcv_ID;
				OK_pkptr->pk_crc  = getCRC((u8*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
				LOG_MSG(MAC_LEVEL,"--------------------------------Relay forward TR_OK_PK to node%d \n",rcv_ID);
				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
			}
			else
			{
				// 4.2.3 ֻ�����·�ɻ���·�ɣ�ֱ�Ӷ�������
				LOG_MSG(MAC_LEVEL,"--------------------------------no IARP route to relay forward TR_OK_PK to dest_ID(%d) \n",dest_ID);
			}
//			routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
//			if(routing_state == 0)
//			{
//				// 4.2.1 ������·��, �޸��շ���ַ�����¼���CRC
//				OK_pkptr->send_ID = self_node_id;
//				OK_pkptr->rcv_ID  = rcv_ID;
//				OK_pkptr->pk_crc  = getCRC((u8*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
//			    LOG_MSG(MAC_LEVEL,"--------------------------------forward TR_OK_PK to node%d \n",rcv_ID);
//				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
//			}
//			else
//			{
//				// 4.2.3 ֻ�����·�ɻ���·�ɣ�ֱ�Ӷ�������
//			    LOG_MSG(MAC_LEVEL,"--------------------------------no IARP route to forward TR_OK_PK to node%d \n",rcv_ID);
//			}
		}
		else
		{
			// 4.3 ���սڵ���������Ϊ���·�ɣ���鱨ͷ�ڵ�trace_list����һ���ڵ�id
			rcv_ID = MAC_Get_Next_ID(trace_list, ((u8)Node_Attr.ID & 0xff));		//�����ϣ��˴���Ȼ�ܹ�����rcv_ID�����򣬾���Դ�˷�װ·��ͷʱ������

			if( rcv_ID >=0 )
			{
				//  ��˫���ھ�, ��radius = 0 �������Ϊ�����ھӣ����޸��շ���ַ�����¼���CRC
				OK_pkptr->send_ID 	= self_node_id & 0xff;
				OK_pkptr->rcv_ID 	= rcv_ID;
				OK_pkptr->pk_crc 	= getCRC((unsigned char*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);

			    LOG_MSG(MAC_LEVEL,"--------------------------------Relay forward TR_OK_PK to node%d \n",rcv_ID);
				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);
			}
			else
			{
				// ���Ҳû��·�ɣ� ֱ�Ӷ�������
			    LOG_MSG(MAC_LEVEL,"--------------------------------no IERP route to relay forward TR_OK_PK to dest_ID��%d�� \n",dest_ID);
			}

			// 4.3.1 ��һ���ھӱ��жϵ�rcv_ID�Ƿ�ɴ�ɴ����װ���ͣ����ɴ������rerr_pklen
//			i = Is_MacOneHopNb(rcv_ID);
//			if( i >0 )
//			{
//				//  ��˫���ھ�, ��radius = 0 �������Ϊ�����ھӣ����޸��շ���ַ�����¼���CRC
//				OK_pkptr->send_ID 	= self_node_id & 0xff;
//				OK_pkptr->rcv_ID 	= rcv_ID;
//				OK_pkptr->pk_crc 	= getCRC((unsigned char*)OK_pkptr, sizeof(TokenRing_OK_PK)-2);
//
//			    LOG_MSG(MAC_LEVEL,"--------------------------------forward TR_OK_PK to node%d \n",rcv_ID);
//				radio_pks_sendto(M_e32_uart_fd, (u8*)OK_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);
//			}
//			else
//			{
//				// ���Ҳû��·�ɣ� ֱ�Ӷ�������
//			    LOG_MSG(MAC_LEVEL,"--------------------------------no IERP route to forward TR_OK_PK to node%d \n",rcv_ID);
//			}
		}
	}
	else
	{
		//4.5 ����ID��������ֱ�Ӷ�����
		return 0;
	}
	return 1;
}

/*
 * @FUN:
 * 		 TokenRing_OK_PK_Send
 * @Desc:
 * 		��������TokenRing_OK_PK������Ŀ�Ľڵ㣬����ռ���ŵ�������Ϣ;Ŀ�Ľڵ��յ�
 * 		���ĺ�����������Ϣ��������Ϣ;
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

	/*����MAC_Route_Search�����Ƿ���src_ID-->dest_ID·��
	 *�ɲ���@rcv_ID ������һ���ڵ�ID
	 *�ɲ���@rreq_sr_ID����·����Դ�ڵ�ID
	 *�ɲ���@hop��������
	 *�ɲ���@trace_list����·���ڵ�ID����
	 * */
	routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);

	if(routing_state >= 0)		// �����ڻ����·��
	{
		tokenRingOk_pkptr->pk_type 		= TOKEN_RING_OK_PK_TYPE;
		/*
		 * ����next_rcv_ID,
		 * */
		tokenRingOk_pkptr->send_ID 		= src_ID;

		//��һ���ڵ�ID
		tokenRingOk_pkptr->rcv_ID 		= rcv_ID;		//��ʾ����Ŀ�Ľڵ���м�ڵ��Ŀ��ID����

		//Դ�ڵ��Ŀ�Ľڵ�
		tokenRingOk_pkptr->src_ID		= src_ID;
		tokenRingOk_pkptr->dest_ID 		= dest_ID;
		tokenRingOk_pkptr->rreq_src_ID 	= rreq_src_id;
		/*
		 * rreq_seqΪ0ʱ����ʾ����1����2����������·��
		 * */
		tokenRingOk_pkptr->rreq_seq 	= routing_state;
		tokenRingOk_pkptr->hop 			= hop;	//�����ڣ�1��ʱhop=1; 2��ʱhop=2;

		/*��������£�Я���м̽ڵ�·���б�
		 * */
		for(i =0; i< MAC_MAX_HOP; i++)
		{
			tokenRingOk_pkptr->trace_list[i] = trace_list[i];
		}

		tokenRingOk_pkptr->pk_crc = getCRC((unsigned char*)tokenRingOk_pkptr, sizeof(TokenRing_OK_PK)-2);

		/*
		 * ��״̬��ΪTOKEN_RING_OK_SEND״̬���ȴ�TOKEN_RING_OK_ACK������������
		 * */
//		global_token_ring_state =TR_OK_SEND;
		LOG_MSG(MAC_LEVEL,"--------------------send TR_OK_PK from node%d to node%d \n",self_node_id, dest_ID);

		Set_TR_State(TR_OK_SEND);

		// 4. ����TokenRing_Req_PK����
		radio_pks_sendto(M_e32_uart_fd, (u8*)tokenRingOk_pkptr , sizeof(TokenRing_OK_PK), rcv_ID);
		// 5. LOG ��ӡ
		return TRUE;
	}
	else
	{
		//û�дﵽĿ�Ľڵ��·��
		LOG_MSG(MAC_LEVEL,"--------------------no route to send TR_OK_PK from node%d to node%d \n",self_node_id, dest_ID);
		//global_token_ring_state =TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);

		return FALSE;
	}
}


/*
 * ��վ�յ�RouteData,
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
			//ȡ���ڶ��׶�TR_OK_Send֮��������ʱ��
//			g_mac_timeout =FALSE;
			nb_state_list[g_nb_node_i].mac_timeouts_cnt =0;

			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);

			//��������TR_OK_ACK��ʱ��
			wdStart(wd_MAC, OK_ACK_TIMEOUT_MS,TIMER_ONE_SHOT);
		}
	}
	return 0;
}

int Handle_rcv_OK_ACK(TokenRing_OK_ACK_PK *TR_ok_ack_pk)
{
	// 1. ����rcv_ID��src_ID��dest_ID��
	u8	send_ID					=TR_ok_ack_pk->send_ID;
	u8	rcv_ID					=TR_ok_ack_pk->rcv_ID;
	u8	rreq_src_ID				=TR_ok_ack_pk->rreq_src_ID;
	u8	rreq_dest_ID			=TR_ok_ack_pk->rreq_dest_ID;
	u8	hop						=TR_ok_ack_pk->hop;			//hopֵΪ����+1
	u8	trace_list[MAC_MAX_HOP]	={0};
	int	i;

	LOG_MSG(MAC_LEVEL, "--------------------rcvd TR_OK_ACK_PK(%d -> %d) from %d, rcv_ID= %d\n", rreq_dest_ID, rreq_src_ID,send_ID, rcv_ID);

	for(i =0; i< hop; i++)
	{
		trace_list[i]= TR_ok_ack_pk->trace_list[i];
	}
//	global_token_ring_state =TR_ST_INIT;
	Set_TR_State(TR_ST_INIT);

	if(rcv_ID == Node_Attr.ID)			//1.  ����RREP�ǵ������Լ��ǽ�����ʱ�Ž���
	{
//		LOG_MSG(MAC_LEVEL, "--------------------rcvd TR_OK_ACK_PK(%d -> %d) from %d\n", rreq_dest_ID, rreq_src_ID,send_ID);

		if(rreq_src_ID == Node_Attr.ID)	//2. RREQ��ԴID�Ǳ��ڵ�
		{
			//ȡ���ڶ��׶ζ�ʱ��
//			g_mac_timeout =FALSE;
//			LOG_MSG(MAC_LEVEL, "--------------------wdStart 1\n");
			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);
//			LOG_MSG(MAC_LEVEL, "--------------------wdStart 2\n");

		}
		else
		{
			// 4. ����Լ�����ԴID�������trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
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

	/* 1. ��BC_REQ_RECORD���в���src_ID---> dest_ID(���ڵ�)�ļ�¼�
	 * 	  ��ȡ·��id�б�õ�����·��,���ڸ�RREP���ĸ�ֵ
	 * 2������src_ID\dest_ID\rreq_seq���ұ�
	 */
	if (-1== (record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, rreq_src_id, rreq_dest_id, 0)))
	{
		//���src_Id ---> dest_IDû��·������ֱ�ӷ���
		return;
	}

	record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	// 2. ��װ����
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type 	= TOKEN_RING_OK_ACK_PK_TYPE;
	send_pk.reserved 	= 0;
	send_pk.send_ID 	= self_node_id & 0xff;

	send_pk.rcv_ID 		= MAC_Get_Prev_ID(record_ptr->trace_list, (u8)(Node_Attr.ID & 0xff));
	send_pk.rreq_src_ID = rreq_src_id;
	send_pk.rreq_dest_ID= rreq_dest_id;
	send_pk.rreq_seq 	= record_ptr->rreq_seq;
	send_pk.hop			= record_ptr->hop;				//������Ҳ���ظ�Դ�ڵ�

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		send_pk.trace_list[i] = record_ptr->trace_list[i] & 0xff;
	}

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(TokenRing_OK_ACK_PK)-2);

	// 4. LOG ��ӡ
	LOG_MSG(MAC_LEVEL,"send TR_OK_ACK_PK(%d -> %d)to %d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID);

	radio_pks_sendto(M_e32_uart_fd, (u8*)&send_pk , sizeof(TokenRing_OK_ACK_PK), send_pk.rcv_ID);
	return ;
}


/** Handle_rcv_token_rep() -- �յ�TokenRing_REP_PK��Ĵ���
	����
		1. rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת�����м�ڵ㲻ά������·����
		2. ���Լ�ΪԴ�ڵ㣬����ת����������IERP��������Ҫ����״̬���������߶˵ķ��ͻ���������ȡ���ķ���
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
	u8	hop					=TR_rep_pkptr->hop;			//hopֵΪ����+1
	u8	dest_msgList_ack	=TR_rep_pkptr->msgList_ack;

	u8	trace_list[MAC_MAX_HOP]={0};
	int	i;

	Set_TR_State(TR_REP_RCV);

	// 2. ����rcv_ID��src_ID��dest_ID��
	for(i =0; i< hop; i++)
	{
		trace_list[i]= TR_rep_pkptr->trace_list[i];
	}

	LOG_MSG(MAC_LEVEL, "rcvd TR_Rep_PK(%d -> %d) from %d, dest_msgList_ack = %d \n", rreq_dest_ID, rreq_src_ID,send_ID, dest_msgList_ack);

	if(rcv_ID == Node_Attr.ID)			//1.  ����RREP�ǵ������Լ��ǽ�����ʱ�Ž���
	{
		if(rreq_src_ID == Node_Attr.ID)	//2. RREQ��ԴID�Ǳ��ڵ�
		{
			 //ȡ����1�׶ζ�ʱ��
//			g_mac_timeout =FALSE;
			wdStart(wd_MAC, 0,TIMER_ONE_SHOT);

//			if(dest_TokenRingOK_ack == TRUE)
//				return 0;
			// 3.1 ����Լ���RREQ��ԴID������IERP,��APP_BUFF��Ŀ��ID��dest_ID�ı�����ȡ����������WiFiSndThread��App����
			if(hop == 2)
			{
				// �޼�¼��ֱ�����
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
					// �޼�¼��ֱ�����
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
						ierp_ptr->rreq_src_ID = rreq_src_ID;	//Դ�ڵ�

						/*
						 * �㲥BC_REQ_PK ���к�
						 * */
						ierp_ptr->rreq_seq = rreq_seq;
						ierp_ptr->dest_ID = rreq_dest_ID;		//Ŀ�Ľڵ�
						ierp_ptr->next_ID = send_ID;			//�м̽ڵ�
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

			//��ĳ�ڵ���Ϣ��������Ϣ�����ͣ�����TokenRing_OK_PK
			if(dest_msgList_ack== TRUE)
			{
				TokenRing_OK_PK_Send((u8)(rreq_dest_ID & 0xff));
//				g_mac_timeout =FALSE;
				//�����ڶ��׶ζ�ʱ��
				/*
				 * �յ�REP_PKT�󣬷���OK_PKT,Ȼ�������ȴ�OK_ACK_PKT��ʱ��ʱ��
				 * */
//				wdStart(wd_MAC, SLICE_TIMEOUT_MS,TIMER_ONE_SHOT);
				wdStart(wd_MAC, ROUTE_AND_OK_ACK_TIMEOUT_MS, TIMER_ONE_SHOT);
			}
			else
			{
				//��ĳ�ڵ�ظ�����Ϣ����״̬�ص�TR_ST_INIT
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
			// 4. ����Լ�����ԴID�������trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
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
 *  TokenRing_REP_PK_Send() -- Ŀ�Ľڵ���RREQ��Դ�ڵ㷢��RREP���ģ����򵥲����͡���RREQ_Record�����ҵ��ü�¼����װRREP���͡�
	���룺src_idΪRREQ��Դ�ڵ㣬����ͨ����Դ��ַ���岻ͬ����Ϊ�˼򻯲��������ڲ���RREQ��
	rreq_seqΪ0ʱ�������ֶ���Ч

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
	/* 1. ��BC_REQ_RECORD���в���src_ID---> dest_ID(���ڵ�)�ļ�¼�
	 * 	  ��ȡ·��id�б�õ�����·��,���ڸ�RREP���ĸ�ֵ
	 *
	 */
	if (-1== (record_pos = MAC_Search_BC_REQ_Record(TOKEN_RING_BC_REQ_Record_List, rreq_src_id, rreq_dest_id, rreq_seq)))
	{
		//���src_Id ---> dest_IDû��·������ֱ�ӷ���
        LOG_MSG(MAC_LEVEL, "--------------------------------TokenRing_REP_PK_Send no route(rreq_src_id[%d]-> rreq_dest_id[%d], seq=%d)\n",rreq_src_id, rreq_dest_id, rreq_seq );

		return;
	}

	record_ptr = (MAC_RREQ_RECORD*)prg_list_access(TOKEN_RING_BC_REQ_Record_List, record_pos);

	// 2. ��װ����
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type 	= TOKEN_RING_REP_PK_TYPE;
	send_pk.reserved 	= 0;

	send_pk.send_ID 	= self_node_id & 0xff;

	//���·���б�����һ���ڵ�ID ,��ΪĿ��/�м���սڵ㡣
	send_pk.rcv_ID 		= MAC_Get_Prev_ID(record_ptr->trace_list, (u8)(Node_Attr.ID & 0xff));

	send_pk.rreq_src_ID = rreq_src_id;
	send_pk.rreq_dest_ID= rreq_dest_id;

	/*
	 * �㲥BC_REQ_PK ���к�
	 * */
	send_pk.rreq_seq 	= record_ptr->rreq_seq;

//	send_pk.path_QOS 	= record_ptr->path_QOS;
	send_pk.hop			= record_ptr->hop;				//������Ҳ���ظ�Դ�ڵ�

	for(i = 0; i< MAC_MAX_HOP; i++)
	{
		send_pk.trace_list[i] = record_ptr->trace_list[i] & 0xff;
	}

	/*
	 * �ж���Ϣ�������Ƿ�����Ϣ������
	 * */
#ifndef DEF_GET_MSG_NUM

	memset((u8*)&g_msgitem, 0, sizeof(struct q_item));
	msgLen =0;
	msgLen = msgrcv(snd_msgqid,&g_msgitem,MAX_MSG_LEN,0,IPC_NOWAIT);
	g_msgitem_len =msgLen;

	if(errno== ENOMSG)
	{
		g_msgitem_len =0;
		//�����Ŀ��ƽڵ�ظ�ACK-����Ϣ
		send_pk.msgList_ack	 = FALSE;
	}
	else
	{
		if(msgLen > 0 )
		{
			//�����Ŀ��ƽڵ�ظ�ACK-����Ϣ
			send_pk.msgList_ack	 = TRUE;
		}
		else
		{
			//�����Ŀ��ƽڵ�ظ�ACK-����Ϣ
			send_pk.msgList_ack	 = FALSE;
		}
	}

#else
	memset(&ds,0,sizeof(ds));
	msgctl(msgQ_Wifi_Snd,IPC_STAT,&ds);
	if(ds.msg_qnum > 0 )
	{
		//�����Ŀ��ƽڵ�ظ�ACK-����Ϣ
		send_pk.msgList_ack	 = TRUE;
	}
	else
	{
		//�����Ŀ��ƽڵ�ظ�ACK-����Ϣ
		send_pk.msgList_ack	 = FALSE;
	}
#endif

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);
	// 4. LOG ��ӡ
	LOG_MSG(MAC_LEVEL,"send TR_Rep_PK(%d -> %d)to %d, msg_qnum=%d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID, ds.msg_qnum);

	Set_TR_State(TR_REP_SEND);

	//���ڻظ�TokenRing_Rep_PK���ڵ�Ե㴫����̣����ֱ�ӵ���radio_pks_sendto()...
	radio_pks_sendto(M_e32_uart_fd, (u8*)&send_pk , sizeof(TokenRing_Rep_PK), send_pk.rcv_ID);


//	LOG_MSG(MAC_LEVEL,"send TR_Rep_PK(%d -> %d)to %d, g_msgitem_len=%d, errno=%d\n",rreq_dest_id,rreq_src_id, send_pk.rcv_ID,g_msgitem_len, errno);
}

/*
 * ������յ�TokenRing_BC_Req_PK���ݰ�����
 *
 * �������ؽڵ��ڷ���BC_REQ_PK���ĺ󣬲���ϣ���ٴ��յ�BC_REQ_PK��
 * ��ʹ����һ��ʱ��Ƭ�յ������������ڵ�ת����BC_REQ_PK��Ҳ����������;
 * ���ؽڵ㷢��BC_REQ_PK���ĺ��ڴ�REP_PK�ĵ�����
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
		return 0;	/*�Լ����͵�BC_REQ,���Լ��ظ��յ�������*/
	}
	LOG_MSG(MAC_LEVEL, "--------------------------------rcv TR_BC_Req_PK(%d -> %d, seq=%d) from %d\n",src_ID, dest_ID, rreq_seq, send_ID);

	//�ж�TokenRing_BC_Req_PKĿ�Ľڵ��Ƿ�Ϊ���ڵ�
    if(dest_ID == Node_Attr.ID)
    {
    	/*
    	 * 1> ����TokenRing_BC_Req_PK�ǹ㲥���ͣ������Ҫͨ�����к�
    	 * �����ظ�����ͬһ�����ݰ�
    	 *
    	 * 2> ��TokenRing_BC_Req_PK��������ȡ��Դ�ڵ㵽�м�/Ŀ�Ľڵ��·���б�
    	 *
    	 * 3> ����TOKEN_RING_BC_REQ_Record_List���ж��Ƿ���յ��µĹ㲥����������Ҫ�½��������µļ�¼��;
    	 * */
    	g_BC_Req_rcvd = TRUE;

		update_res = Update_TokenRing_BC_REQ_Tab(bc_req_pkptr);
		if(update_res == 1)
		{
			//���յ��µķ��͸����ڵ��TokenRing_BC_Req_PK���ģ���ظ�REP_PK
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
        	//�ﵽֻת��һ����ͬ���кŵ�BC_REQ���ģ�
            if(update_res == 1)
            {
            	bc_req_pkptr->TTL--;
            	bc_req_pkptr->send_ID = Node_Attr.ID & 0xff;
            	bc_req_pkptr->trace_list[bc_req_pkptr->hop] = Node_Attr.ID & 0xff;
            	bc_req_pkptr->hop++;

            	bc_req_pkptr->pk_crc = getCRC((unsigned char*)bc_req_pkptr,  sizeof(TokenRing_BC_Req_PK)-2);
            	//radio_pks_sendto(M_e32_uart_fd, (u8*)bc_req_pkptr , sizeof(TokenRing_BC_Req_PK), 255);
				msgQ_snd(msgQ_Wifi_Snd, (u8*)bc_req_pkptr,  sizeof(TokenRing_BC_Req_PK) ,WLAN_ROUTE_CTRL, NO_WAIT);
            	// LOG ��ӡ
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
 * �㲥����TokenRing_BC_Rep_PK����
 * */
void TokenRing_BC_REQ_PK_Send(u8 dest_id )
{
	// 1. ��װ����
	TokenRing_BC_Req_PK send_pk;
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));

	send_pk.pk_type = TOKEN_RING_BC_REQ_PK_TYPE;
	send_pk.TTL 	= MAC_MAX_HOP -1 ;

	send_pk.reserved1 =0;
	send_pk.send_ID = self_node_id & 0xff;

	//�㲥����RREQ
	send_pk.reserved2 =0;
	send_pk.rcv_ID = 255;						//�㲥����RREQ

	send_pk.src_ID = self_node_id & 0xff;

	//RREQ��Ŀ�Ľڵ�id
	send_pk.dest_ID  = dest_id;					//RREQ��Ŀ��id
	send_pk.rreq_seq = MAC_SEQ_INC(BC_REQ_Seq); //����rreq_seq���к�

//	send_pk.path_QOS = 16;
	send_pk.hop = 1;				//����TokenRing_BC_Req_PKԴ�ڵ㽫hop��Ϊ1

	send_pk.trace_list[0] = self_node_id & 0xff;		//

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(TokenRing_BC_Req_PK)-2);

	// 2. ���±���RREQ�����뱾����¼����ʶ���ھ�ת�����ĸñ���

	/*
	 * ���ؽڵ����TOKEN_RING_BC_REQ_PK_TYPE�������յ�ͬһ��ű��ĺ��ظ�ת��
	 * */
	Update_TokenRing_BC_REQ_Tab(&send_pk);

//	global_token_ring_state =TR_BC_REQ_SEND;
	Set_TR_State(TR_BC_REQ_SEND);
    LOG_MSG(MAC_LEVEL,"--------------------send TR_BC_REQ_PK(Dest_ID=%d,seq=%d) from node%d \n",dest_id,BC_REQ_Seq,self_node_id);

	// 3. ����WiFiSndThread��Route���ƶ���
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
 * �ж�<node_ID, relay_ID>�Ƿ���2���ھ��б���
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
 * 		��������½��յ�REQ_PK, ִ�����´���
 * 		1�����ڶ����м̳������迼���Ƿ�Ϊ���͸����ڵ��TokenRing_Req_PK; �����ǵ�Ϊ�м̽ڵ㣬��ת���ñ���;
 * 		2����Ŀ�Ľڵ��Ǳ��ڵ㣬�򵥲��ظ�TokenRing_REP
 * */
int Handle_rcv_token_ring_req(TokenRing_Req_PK* req_pkptr)
{
	u8	i=0;
	// 2. ���ݱ�ͷ��ʽ��������Դ��Ŀ��IP��ַ �� TTL��bc_seq����Ϣ; ע�⣬��������Ϊ�����ֽ���
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
	 * ����Ϊ��Ե㴫�䣬�����м�����£�ͨ��trace_list[...]ָ����δ�������һ���ڵ�
	 * */
	Set_TR_State(TR_REQ_RCV);

	if(rcv_ID == (Node_Attr.ID & 0xff))		//�м�ڵ��Ǳ��ڵ��
	{
	    LOG_MSG(MAC_LEVEL,"--------------------------------rcvd TR_REQ_PK(seq=%d) from src_ID(%d) to dest_ID(%d) \n",rreq_seq, src_ID, dest_ID);
    	g_BC_Req_rcvd = TRUE;

		if(dest_ID == rcv_ID)		//Ŀ�Ľڵ��Ƿ�Ϊ���ڵ��
		{
			//Ŀ�Ľڵ��Ǳ��ڵ�, ��ظ�TokenRingREP���ġ�������
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
		//4.5 ����ID��������ֱ�Ӷ�����
		return 0;
	}
	return 1;
}

/*@Function:
 * 		TokenRingReq_PK_Send
 *@Desc:
 * 		��������TokenRingReq, �����ݰ���·�ɽ����󣬵�Ե㷽ʽ������Ŀ�Ľڵ�
 *@Param1:
 *		tokenRingReq_pkptr���ݰ�ָ��
 *@Param2:
 *		dest_ID��Ŀ�Ľ��սڵ�
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

	/*����MAC_Route_Search�����Ƿ���src_ID-->dest_ID·��
	 *�ɲ���@rcv_ID ������һ���ڵ�ID
	 *�ɲ���@rreq_sr_ID����·����Դ�ڵ�ID
	 *�ɲ���@hop��������
	 *�ɲ���@trace_list����·���ڵ�ID����
	 * */
	routing_state = MAC_Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);

	if(routing_state >= 0)		// �����ڻ����·��
	{
		tokenRingReq_pkptr->pk_type 	= TOKEN_RING_REQ_PK_TYPE;
		/*
		 * ����next_rcv_ID,
		 * */
		tokenRingReq_pkptr->send_ID 	= src_ID;

		//��һ��
		tokenRingReq_pkptr->rcv_ID 		= rcv_ID;		//��ʾ����Ŀ�Ľڵ���м�ڵ��Ŀ��ID����

		//Դ�ڵ��Ŀ�Ľڵ�
		tokenRingReq_pkptr->src_ID 		= src_ID;
		tokenRingReq_pkptr->dest_ID 	= dest_ID;
//		tokenRingReq_pkptr->rreq_src_ID = src_ID;
		/*
		 * rreq_seqΪ0ʱ����ʾ����1����2����������·��
		 * */
		tokenRingReq_pkptr->rreq_seq 	= MAC_SEQ_INC(REQ_Seq); //����rreq_seq���к�;
//		tokenRingReq_pkptr->TTL 		= MAC_MAX_HOP-1;
//		tokenRingReq_pkptr->hop 		= hop;	//�����ڣ�1��ʱhop=1; 2��ʱhop=2;

		/*
		 * �Ƿ�ΪTokenRingOK= TRUE ���͵�TOKEN_RING_REQ_PK_TYPE����
		 * */
//		tokenRingReq_pkptr->TokenRingOK_Pk_flag		=TokenRingOK_Pk;

		/*��������£�Я���м�·���б�
		 * */
		for(i =0; i< MAC_MAX_HOP; i++)
		{
			tokenRingReq_pkptr->trace_list[i] = trace_list[i];
		}

		tokenRingReq_pkptr->pk_crc = getCRC((unsigned char*)tokenRingReq_pkptr, sizeof(TokenRing_Req_PK)-2);

		// 5. LOG ��ӡ
		LOG_MSG(MAC_LEVEL,"--------------------send TR_REQ_PK(seq=%d) from node%d to node%d \n",REQ_Seq, self_node_id, dest_ID);

		// 4. ����TokenRing_Req_PK����
		Set_TR_State(TR_REQ_SEND);
		radio_pks_sendto(M_e32_uart_fd, (u8*)tokenRingReq_pkptr , sizeof(TokenRing_Req_PK), rcv_ID);

		//����֮��������ʱ��
	    g_mac_timeout= FALSE;

	    /*
	     * ����REP ���ã˶�ʱ��
	     * */
		wdStart(wd_MAC, REP_TIMEOUT_MS, TIMER_ONE_SHOT);
		return TRUE;
	}
	else
	{
		//û�дﵽĿ�Ľڵ��·��
//		global_token_ring_state =TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);
	    g_mac_timeout= FALSE;
	    LOG_MSG(MAC_LEVEL,"--------------------no route to send TR_REQ_PK from node%d to node%d \n",self_node_id, dest_ID);
		return FALSE;
	}

}


 /*	�����м�ͨ�ų����¼��п�����ѯ���ԣ�
 *	�ڵ�1��ѯ�ڵ�2ʱ��
 *	1) �����жϽڵ�2�Ƿ���1��·�ɱ��ڣ�����ڣ���ֱ�ӷ���TokenRingReq(2);
 *	  �ȴ�MAX_TIMEOUT_MSʱ����մ���ڵ�2��Ϣ��
 *
 *	2������ڵ�2����1��·�ɱ��ڣ���㲥����TokenRingBCReq(255, Seq, 2)�� ��ʱ������������¼��������
 *	  2.1) �տ������ڵ�2��������,����δ����·�ɣ���ʱ�յ�TokenRingBCReq(255, Seq, 2)��
 *	  �ڵ�2������ѯ���Լ������ư�����ظ�TokenRingRep��2��,��Я��������Ϣ��������Ϣ;
 *	  ������Ϣ����ڵ�1�ᷢ��TokenRingOk(2),�ڵ�2�յ�TokenRingOk(2)ָ�����ռ���ŵ�������1����Ϣ;
 *
 *	  �ڵ�3/4���������ڣ���ʱ�յ�TokenRingBCReq(255, Seq, 2)���ڵ�3/4���ֲ���ѯ���Լ��Ĺ㲥���ư���
 *	  ����ת��������TokenRingBCReq(255,Seq,2)��ӵ���Ϣ������;
 *	  ת����ʱ��������ѭ�ڵ�1��Ϊ���Ŀ��ƵĲ���;
 *
 *	  2.2���ڵ�2���������ڣ�MAX_TIMEOUT_MS��ʱ�󣬽ڵ�1û���յ��κ���Ϣ���������ѯ�ڵ�2����ʼ��ѯ
 *	  �ڵ�3.
 *
 *�ڵ�1��ѯ�ڵ�3ʱ��
 *	1) �����жϽڵ�3�Ƿ���1��·�ɱ��ڣ�����ڣ���ֱ�ӷ���TokenRingReq(3);
 *	  �ȴ�MAX_TIMEOUT_MSʱ����մ���ڵ�3��Ϣ��
 *
 *	2������ڵ�3����1��·�ɱ��ڣ���㲥����TokenRingBCReq(255, Seq, 3)�� ��ʱ������������¼��������
 *	  2.1) �տ������ڵ�3��������,����δ����·�ɣ���ʱ�յ�TokenRingBCReq(255, Seq, 3)��
 *	  �ڵ�3������ѯ���Լ������ư�����ظ�TokenRingRep��3��,��Я��������Ϣ��������Ϣ;
 *	  ������Ϣ����ڵ�1�ᷢ��TokenRingOk(3),�ڵ�2�յ�TokenRingOk(3)ָ�����ռ���ŵ�������1����Ϣ;
 *
 *	  ���ڽڵ�1ѯ�ʽڵ�2ʱ���ڵ�2��δ�ظ�
 *
 *	  �ڵ�2/4���������ڣ���ʱ�յ�TokenRingBCReq(255, Seq, 3)���ڵ�2/4���ֲ���ѯ���Լ��Ĺ㲥���ư���
 *	  ����ת��������TokenRingBCReq(255,Seq,3)��ӵ���Ϣ������;
 *	  ת����ʱ��������ѭ�ڵ�1��Ϊ���Ŀ��ƵĲ���;
 */

int mac_msgq_id=-1;
int wlan_snd_msgq_id=-1;

void Try_Get_Msg_Process()
{
	int				msgLen;
	unsigned char	msgBuff[MAC_MAX_MSG_LEN];	 //��������LanRcvThread��WiFiRcvThread����Ϣ
	struct 			mac_q_item item;

	memset(&item,0,sizeof(item));
	//��MAC��Ϣ�����а����Ƚ��ȳ�ԭ���ȡ��Ϣ
//	printf("--------Try_Get_MsgS_Process\n");
	msgLen = msgrcv(mac_msgq_id,&item,MAC_MAX_MSG_LEN,0,MSG_NOERROR | IPC_NOWAIT);//IPC_NOWAIT);//MSG_NOERROR);
	if(msgLen > 0)
	{
//		LOG_MSG(ERR_LEVEL,"-------------Try_Get_Msg_Process rcvd[0]= \n",msgBuff[0]);

		memcpy(msgBuff,item.data,msgLen);
		switch(msgBuff[0])
		{
			case TOKEN_RING_BC_REQ_PK_TYPE :			//����TOKEN_RING_BC_REQ�㲥����
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

			case TOKEN_RING_REQ_PK_TYPE:				//����TOKEN_RING_REQ_PK_TYPE����
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

			case  TOKEN_RING_REP_PK_TYPE:				//����TOKEN_RING RREP��Ϣ
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
				 * �յ���վREP���ݰ��󣬸��ݴ�վ�ġ��𸴣�1-������Ϣ�� 0-������Ϣ���������Ƿ���TR_OK_PK����
				 * */
				Handle_rcv_token_rep((TokenRing_Rep_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_OK_PK_TYPE:					//����TOKEN_RING OK��Ϣ
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

			case TOKEN_RING_RX_ROUTE_DATA_PK_TYPE:				//����TOKEN_RING_OK_ACK_PK_TYPE��Ϣ
			{
				Handle_rcv_RouteData_Pkt((TokenRing_RX_RouteDATA_PK*)msgBuff);
				break ;
			}

			case TOKEN_RING_OK_ACK_PK_TYPE:				//����TOKEN_RING_OK_ACK_PK_TYPE��Ϣ
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

	//�����ж�ĳ�ڵ��Ƿ���������
	for(node_i =2; node_i<=4; node_i++)
	{
		g_nb_node_i =node_i;
//		printf("Poll_Members[%d]_Control_Channel\n", node_i);
		//����TokenRing_REQ��TokenRing_BC_REQ
		ret = TokenRingReq_PK_Send(node_i);
		if(ret == TRUE)
		{
			 //STEP1> ������1�׶ζ�ʱ��, ���Ƕ���ʱ��
			do{
				 Try_Get_Msg_Process();
				 if(Get_TR_State() == TR_REQ_SEND && g_mac_timeout == FALSE)
				 {
					 //������REQ_PK֮�󣬵ȴ��ظ�δ��ʱ
					 continue;
				 }
				 else if(Get_TR_State() == TR_REQ_SEND && g_mac_timeout == TRUE)
				 {
					 LOG_MSG(DBG_LEVEL,"----1rd\n");
					 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
					 //����ʱ������ɾ����Ŀ�Ľڵ������Ľڵ���MAC·������Ϣ
					 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
					 {
						Del_Mac_Route_Tb_Node(node_i);
						nb_state_list[node_i].mac_timeouts_cnt =0;
						LOG_MSG(INFO_LEVEL,"----1rd Del_Mac_Route_Tb_Node(%d)\n", node_i);
					 }

					//ĳ�ڵ㳬ʱδ�ظ�ACK����վ��״̬����λ����ʼ״̬��׼��ѯ����һ���ڵ�
//					LOG_MSG(MAC_LEVEL,"-------------------------------------------------------------Timeout rcvd OK_ACK_PK from node%d,timeouts_cnt=%d \n", node_i, timeouts_cnt);
					LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd REP_PK from node%d,Get_TR_State() =%d , timeouts_cnt=%d\n", node_i, Get_TR_State(),timeouts_cnt );

					Set_TR_State(TR_ST_INIT);
					g_mac_timeout =FALSE;
					break;
				 }
				 else
				 {
					 /*
					 * ������ʱ������վ�յ�REQ_PK�Ļظ�REP_PK,
					 * Ȼ����վ����ݴ�վ�Ƿ�����Ϣ�����ͣ�����OK_PK\�ȴ��û�ռ���ŵ�\�ȴ�OK_ACK_PK
					 * */
					if(Get_TR_State() != TR_ST_INIT && g_mac_timeout== FALSE)
					{
						continue;
					}
					else if((Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND) && g_mac_timeout == TRUE)
					{
						LOG_MSG(DBG_LEVEL,"----2rd\n");

						//��������ACK�������ڵڶ��׶γ�ʱ������ɾ���ڵ㣬
						//��Ϊ��ǰ�ڵ������ҵ��������ܲ��Ƿ������ؽڵ�ģ����Ǳ�Ľڵ��յ��ˡ�
						//�������ؽڵ���ڵ�ǰ�ڵ�ռ���ŵ����������ղ���ҵ�����������ʱ��
						//���ԣ��������OK_ACK����,�ӽڵ�ÿ���յ�TR_OK_PK��ռ���ŵ������󣬻ظ�TR_OK_ACK_PK
					#ifndef NOT_USED_OK_ACK
//						if(Get_TR_State() == TR_REQ_SEND)
//						if(Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND)
//						{
							 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
							 //����ʱ������ɾ����Ŀ�Ľڵ������Ľڵ���MAC·������Ϣ
							 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
							 {
								//��δ�յ�node_i OK_ACK�ظ�����ɾ�����ھӽڵ�
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
						//��״̬�£����ؽڵ�ָ���TR_ST_INIT��˵���յ�OK_ACK, ֱ��ѯ����һ���ڵ㡣
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
			//�����㲥ѯ·����������
			TokenRing_BC_REQ_PK_Send(node_i);

			//������һ�׶ζ�ʱ��
			do{
				Try_Get_Msg_Process();
				//�ȴ�1�������ڽڵ㣨node_i���ظ�BC_REP_PK��Ϣ
				if(Get_TR_State() == TR_BC_REQ_SEND && g_mac_timeout == FALSE)
				{
					continue;
				}
				else if(Get_TR_State() == TR_BC_REQ_SEND && g_mac_timeout== TRUE )
				{
					//ĳ�ڵ㳬ʱδ�ظ�ACK����վ��״̬����λ����ʼ״̬��׼��ѯ����һ���ڵ�
					LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Timeout rcvd TR_BC_REP_PK from node%d\n", node_i);

					Set_TR_State(TR_ST_INIT);
					g_mac_timeout =FALSE;
					break;
				}
				else
				{
					/*
					 * ����BC_REQ���յ�REP_PK,
					 * Ȼ����վ����ݴ�վ�Ƿ�����Ϣ�����ͣ�����OK_PK\�ȴ��û�ռ���ŵ�\�ȴ�OK_ACK_PK
					 * */
					//�����ڶ��׶ζ�ʱ��
					if(Get_TR_State() != TR_ST_INIT && g_mac_timeout == FALSE)
					{
						continue;
					}
					else if((Get_TR_State() == TR_REP_RCV || Get_TR_State() == TR_OK_SEND) &&g_mac_timeout == TRUE)
					{
						 LOG_MSG(DBG_LEVEL,"----2rd\n");
						 timeouts_cnt= ++nb_state_list[node_i].mac_timeouts_cnt;
						 //����ʱ������ɾ����Ŀ�Ľڵ������Ľڵ���MAC·������Ϣ
						 if(timeouts_cnt >= MAX_TIMEOUT_CNT)
						 {
							//��δ�յ�node_i OK_ACK�ظ�����ɾ�����ھӽڵ�
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
						//��״̬�£����ؽڵ�ָ���TR_ST_INIT��˵���յ�OK_ACK, ֱ��ѯ����һ���ڵ㡣
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
 *		�߳�������,ʵ��������ȡMAC��Ϣ����
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
 * 		���ؽڵ�MAC�����̴߳�����
 * */
void* Master_Mac_ThreadFunAll(void *args)
{
	int 			msglen;
	struct q_item 	msgitem;
	memset((u8*)nb_state_list, 0, sizeof(nb_state_list));
	for(;;)
	{
		/*Step1> �����ؽڵ�ѯ�������Ƿ�����Ϣ������
		 * ��Ҫ���Ƕ����м̳������ŵ�ռ�õĿ���
		 * ����һ���ڵ��յ����ؽڵ��ҵ�����ݱ��ĺ󣬸�����ã˻ظ���ʱ�ӡ�
		 * */

		if(g_UPG_on == TRUE)
		{
			usleep(1000);
			continue;
		}

		 while(Get_TR_State() != TR_ST_INIT );
		 {usleep(1000);}
		//ȡ��mac��ʱ��
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
		/*Step2> ��ѯ�����ڵ��Ƿ�����Ϣ����
		 * ��Ҫ���Ƕ����м̳������ŵ�ռ�õĿ�����
		 * ����һ���ڵ㾭�������յ����ؽڵ��ҵ�����ݱ��ĺ󣬸�����ã˻ظ���ʱ�ӡ�
		 * */
		Poll_Members_Control_Channel();
	}
}

/*
 * ����ʽ���Ʒ����ŵ�
 * */
void MAC_TimeOut_Handler(int sig, siginfo_t *si, void *uc)
{
	g_mac_timeout = TRUE;
}

/*
 * ����mac�߳�
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

	//��ȡMac����Ϣ���У�����Ϣ������ȡMAC��Ϣ����
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


	//��ȡRoute����Ϣ���У�����Ϣ�����л�ȡ�����͵ı���
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
		 *ע��mac���̺߳���������ͨ�����߽��յ���MAC���Ʊ���
		 * */
		ret = pthread_create(&tid,NULL,&Master_Mac_ThreadFunAll,NULL);
		if(ret < 0)
		{
			LOG_MSG(ERR_LEVEL,"create Mac_Thread\n");
			exit(-1);
		}

		//TODO ���ǽڵ�0�����
//		nb_state_list =(MAC_NB_State_t*) malloc(NET_MAX_NODES*sizeof(MAC_NB_State_t));
	}

	else

	{
		/*
		 *�ӽڵ�ע��mac���̺߳���������ͨ�����߽��յ���MAC���Ʊ���
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


