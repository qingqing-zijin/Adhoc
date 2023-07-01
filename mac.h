/*
 * mac.h
 *
 *  Created on: Jun 25, 2018
 *      Author: root
 */

#ifndef MAC_H_
#define MAC_H_

#include "radio.h"
#include <pthread.h>

/*
 * routing.c��mac.c MAC����״̬������������
 * */
extern pthread_mutex_t mac_st_mutex;

/*
 * routing.c��mac.c MAC����վ������վ·�ɱ�ά��������
 * */
extern pthread_mutex_t mac_LinkList_mutex;

extern volatile u8  g_BC_Req_rcvd ;

/*
 * �м�����
 * */
#define     MAC_MAX_HOP                        4


/*
 * ���ڵ��������͵����յ������ظ������ʱ�ӣ�������������£�
 * 1�������м�ָ�����ã���Թ�������Ŀ������м̴���Ϊ3�������Ϊ��=3��
 * 2��1���������͵����յ�������ã˵�ʱ�䣬���Ϊt2=150��
 * 3���û�ռ��1���ŵ����ʱ�䣬�뷢�͵����ݵ���󳤶�ֱ����أ�
 *    ��Թ�������Ŀ���û�1��ռ���ŵ���������ֽ���Ϊ600�ֽڣ����Ϊt3= 500ms��
 * 4���ڵ㷢��1�ν�����Ʊ������ʱ�俪�������Ϊt4 =80;
 * */

#define NET_MAX_NODES			(4U)
#define NET_MAX_RELAY_VAL		(NET_MAX_NODES-1)
/*
 *MAC����Ʊ��Ĵ������ʱ�俪��, t4
 * */
#define MAC_PK_MINI_MS			(80U)

#define MAC_MAX_MSG_LEN  		200

//#define NOT_USED_OK_ACK

#ifdef NOT_USED_OK_ACK
/*
 * û��TR_OK_ACKʱ��MAC����Ʊ�����������: TR_REQ_PK/TR_REP_PK/TR_OK_PK
 * */
#define MAC_CTL_PKS				(3)

#else
/*
 * ��TR_OK_ACKʱ��MAC����Ʊ�����������: TR_REQ_PK/TR_REP_PK/TR_OK_PK/TR_OK_ACK_PK
 * */
#define MAC_CTL_PKS				(4U)

#endif

/*
 * ROUTE���ݱ���ռ���ŵ�ʱ����һ�α��ĵ����ʱ�俪����
 * �����ݰ�����ֽ����������йأ� ������ֽ���Ϊ600�ֽڡ�19.2kbpsʱ��t3= 500ms��
 * */
#define ROUTE_PK_MAX_MS			(600U)

/*
 * һ��������TR_BC_PK�������յ�ACK��ʱʱ��
 * */
#define BC_REP_TIMEOUT_MS		(MAC_PK_MINI_MS*2)

/*
 *����������TR_REQ_PK��������������REP_ACK��ʱ����
 * */
#define REP_TIMEOUT_MS			(MAC_PK_MINI_MS*(NET_MAX_NODES-1)*2)

/*
 * ��վ�ӷ�����ѯ���ʼ�����յ��ӽڵ�ҵ���ĵ����ʱ�俪����
 * SLICE_TIMEOUT_MSӰ����վ��ѯ����վʱ�������ڡ����뱣�ϴ�վ1��ռ���ŵ����ͳɹ�������,���ܿ�����һ����ѯ��
 * (��ʱ���øò�������REP_TIMEOUT_MS��OK_ACK_TIMEOUT_MS�滻)
 * */
#define SLICE_TIMEOUT_MS		((NET_MAX_NODES-1)*(MAC_CTL_PKS*MAC_PK_MINI_MS)+ROUTE_PK_MAX_MS)

/*
 *TR_OK_PK���ͺ󣬵ȴ�Route���� + OK_ACK_PK;
 *���������ڶ�ڵ㻥�ཻ������ͨ�ų�����Route���ݲ�һ���Ƿ������ؽڵ�ģ���Ҫ����Route�����������
 * */
#define RX_ROUTE_DATA_TIMEOUT_MS (ROUTE_PK_MAX_MS)
/*
 *TR_OK_PK���ͺ󣬵ȴ�ACK��ʱ�����
 * */
#define ROUTE_AND_OK_ACK_TIMEOUT_MS	((MAC_PK_MINI_MS*(NET_MAX_NODES-1)*2) + RX_ROUTE_DATA_TIMEOUT_MS)	//����[2]��������OK_PKT�ͽ���OK_ACK_PKT����֮��

#define OK_ACK_TIMEOUT_MS			((MAC_PK_MINI_MS)*(NET_MAX_NODES))

/*
* radio	4.8kbpsʱ��ÿ��600�ֽڣ�ÿ500ms 300�ֽڣ�ÿ250ms 150�ֽڣ�ÿ100ms 60�ֽڣ� ÿ50ms 30�ֽ�
* radio	9.6kbpsʱ��ÿ��1200�ֽڣ�ÿ500ms 600�ֽڣ�ÿ250ms 300�ֽڣ�ÿ100ms 120�ֽڣ� ÿ50ms 60�ֽ�
* radio	19.2kbpsʱ��ÿ��2400�ֽڣ�ÿ500ms 1200�ֽڣ�ÿ250ms 600�ֽڣ�ÿ100ms 240�ֽڣ� ÿ50ms 120�ֽ�
*
* �ߣ�
* 4.8kbps  55B 200ms
* 9.6kbps  55B 135ms
* 19.2kbps 55B 80ms (������Ҫ80ms,�����Ƿ�������-��-���յ����ãˣ���Ҫ160ms)
* */


typedef	struct {
	u8  pk_type;
	u8	send_ID;		// ������ID���������м�ڵ��Դ�ڵ�
	u8	rcv_ID;			// ������ID���������м�ڵ��Ŀ�Ľڵ�
	u8	src_ID;			// Դ�ڵ�ID
	u8	dest_ID;		// Ŀ�Ľڵ�ID
	u8	rreq_src_ID;	// RREQԴ�ڵ�ID����rreq_seqһ��λһ��·�ɼ�¼
	u16 rreq_seq;		// ��Ϊ0�����ʾ����·�ɣ�������Ҳ��������·�ɱ��ʹ��ʱ�䣬�������ɾ����
									// ����0, ���ʾRREQ��ţ���trace_list����ȡ��һ������ɾ�����뱾�رȽϣ�����̫���� �����ڱ���·�ɼ�¼�е�rreq_seq��������·������ɵ�
	u8	hop;			// ����ʱ��Ч��Ϊ��src_ID��dest_ID��������

	u8	trace_list[MAC_MAX_HOP];	// ����ʱ��Ч��Ϊsrc_ID-->dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID���м�ڵ�������ҵ���һ���ڵ㣬�������ν��Դ·��
	u16 pk_crc;
}TokenRing_OK_PK;


typedef	struct {
	u8  pk_type;
	u8	send_ID;		// ������ID���������м�ڵ��Դ�ڵ�
	u8	rcv_ID;			// ������ID���������м�ڵ��Ŀ�Ľڵ�
	u8	src_ID;			// Դ�ڵ�ID
	u8	dest_ID;		// Ŀ�Ľڵ�ID
//	u8	rreq_src_ID;	// RREQԴ�ڵ�ID����rreq_seqһ��λһ��·�ɼ�¼
	u16 rreq_seq;		// ��Ϊ0�����ʾ����·�ɣ�������Ҳ��������·�ɱ��ʹ��ʱ�䣬�������ɾ����
									// ����0, ���ʾRREQ��ţ���trace_list����ȡ��һ������ɾ�����뱾�رȽϣ�����̫���� �����ڱ���·�ɼ�¼�е�rreq_seq��������·������ɵ�
//	u8	TTL;			// �㲥����ʱ�䣬���ƹ㲥��Χ��Դ����Ϊ6��ÿת��һ�εݼ�1����0ʱ���ٹ㲥ת��������6��
//	u8	hop;			// ����ʱ��Ч��Ϊ��src_ID��dest_ID��������

	u8	trace_list[MAC_MAX_HOP];	// ����ʱ��Ч��Ϊsrc_ID-->dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID���м�ڵ�������ҵ���һ���ڵ㣬�������ν��Դ·��
	u16 pk_crc;
}TokenRing_Req_PK;


enum{
	TokenRingOK_FALSE=0,
	TokenRingOK_TRUE=1
};


//typedef	struct
//{
//	u8 	pk_type;
//	u8 	send_id;
//	u8 	rcv_id;
//	u8  ack;
//	u16 pk_crc;
//}TokenRing_Rep_PK;

typedef struct{
	unsigned char	pk_type;		// �������ͣ�RREP���������ͣ�rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
	unsigned char	reserved;		// ����ֽڣ�2�ֽڶ���
	unsigned char	send_ID;		// ������ID���м�ڵ�ID��dest_ID
	unsigned char	rcv_ID;			// ������ID���м�ڵ�ID��src_ID; �յ�֮����trace_list������Ѱ����rcv_ID

	unsigned char	rreq_src_ID;	// ����RREQ��ԴID��Ҳ�ǻظ�RREP��Ŀ��ID
	unsigned char	rreq_dest_ID;	// ����RREQ��Ŀ��ID��Ҳ��RREP��ԴID
	unsigned short	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��·�ɽ�������ڱ�ʶ·�ɵ��¾ɡ�

//	unsigned char	path_QOS;		// ·���������Ƚϱ�����ԭֵ�뱾��node_qos��ѡ���С��һ���滻���ֶΣ�Դ�ڵ㷢��ʱ�ĳ�ֵΪ����node_qos���ݲ���
	unsigned char	hop;			// ��src_ID��dest_ID������
//	u8				tokenRingOk_ack;
	unsigned char	msgList_ack;	// ���ڵ��Ƿ���Ϣ�������Ƿ�����Ϣ������

	unsigned char	trace_list[MAC_MAX_HOP];	// ��src_ID��dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID

	unsigned short	pk_crc;			// ����RREP���ĵ�CRC

}TokenRing_Rep_PK;


typedef struct{
	unsigned char	pk_type;		// �������ͣ�RREP���������ͣ�rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
	unsigned char	reserved;		// ����ֽڣ�2�ֽڶ���
	unsigned char	send_ID;		// ������ID���м�ڵ�ID��dest_ID
	unsigned char	rcv_ID;			// ������ID���м�ڵ�ID��src_ID; �յ�֮����trace_list������Ѱ����rcv_ID

	unsigned char	rreq_src_ID;	// ����RREQ��ԴID��Ҳ�ǻظ�RREP��Ŀ��ID
	unsigned char	rreq_dest_ID;	// ����RREQ��Ŀ��ID��Ҳ��RREP��ԴID
	unsigned short	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��·�ɽ�������ڱ�ʶ·�ɵ��¾ɡ�
	unsigned char	hop;			// ��src_ID��dest_ID������
	unsigned char	trace_list[MAC_MAX_HOP];	// ��src_ID��dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID

	unsigned short	pk_crc;			// ����RREP���ĵ�CRC

}TokenRing_OK_ACK_PK;



typedef struct{
	u8	pk_type;		// �������ͣ�RREQ��ʼ���ǹ㲥���ͣ����ǽ����߻�����Լ�����mpr_list�ڣ��������Ƿ�ת��
	u8	TTL;			// �㲥����ʱ�䣬���ƹ㲥��Χ��Դ����ΪMAX_HOP-1��ÿת��һ�εݼ�1����0ʱ���ٹ㲥ת��

	u8	reserved1;
	u8	send_ID;		// ������ID���������м�ڵ��Դ�ڵ�

	u8	reserved2;
	u8	rcv_ID;			// ������ID���������м�ڵ��Ŀ�Ľڵ�

	u8	src_ID;			// ����RREQ��ԴID
	u8	dest_ID;		// RREQ��Ӧ��ڵ�ID����RREQ��Ŀ��ID
	u16	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��Ѱ·���̣���dest_ID�޹ء����һ��src_ID+rreq_seq,һ���ڵ�ֻ��Ӧһ�Σ����Ǻ�����path_cost��С

//	u8	path_QOS;		// ·���������Ƚϱ�����ԭֵ�뱾��node_qos��ѡ���С��һ���滻���ֶΣ�Դ�ڵ㷢��ʱ�ĳ�ֵΪ����node_qos���ݲ���
	u8	hop;			// ��src_ID�����ص�����������Դ�ڵ㷢��RREQʱ��hop��Ϊ��ֵ1���Ժ�ÿ��ת������1
	u8  trace_list[MAC_MAX_HOP];	// trace_list[0]Ϊsrc_ID, trace_list[hop-1]Ϊ���ͽڵ㣬������Ϊ���ͽڵ��MPR������ trace_list[hop]Ϊ����ID����������3ֵ�󷢳���

	unsigned short	pk_crc;					// ����RREQ���ĵ�CRC
}TokenRing_BC_Req_PK;


typedef struct
{
	u8	pk_type;
	u8	src_ID;
	u8	dst_ID;
}TokenRing_RX_RouteDATA_PK;

typedef struct
{
	u8	rreq_src_ID;			// ����RREQ��ԴID
	u8	rreq_dest_ID;		// RREQ��Ӧ��ڵ�ID����RREQ��Ŀ��ID
	u16	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��Ѱ·���̣���dest_ID�޹ء����һ��src_ID+rreq_seq,һ���ڵ�ֻ��Ӧһ�Σ����Ǻ�����path_cost��С

	u8	hop;			// ��src_ID�����ص�����������Դ�ڵ㷢��RREQʱ��hop��Ϊ��ֵ1���Ժ�ÿ��ת������1
	u8	trace_list[MAC_MAX_HOP];	// trace_list[0]Ϊsrc_ID, trace_list[hop-1]Ϊ���ͽڵ㣬������Ϊ���ͽڵ��MPR������ trace_list[hop]Ϊ����ID����������3ֵ�󷢳���
											//  ������Ϊdest_ID,���� trace_list[hop]Ϊ����ID��hop�� trace_list ���һ����ѡ·��
}MAC_RREQ_RECORD;



struct mac_q_item
{
	long type;
	char data[MAC_MAX_MSG_LEN];
};

typedef enum{
	TR_ST_INIT=0,
	TR_BC_REQ_SEND,
	TR_BC_REQ_RCV,
	TR_REQ_SEND,
	TR_REQ_RCV,
	TR_REP_SEND,
	TR_REP_RCV,
	TR_OK_SEND,
	TR_DATA_RCV,
	TR_OK_ACK_RCV,
	TOKEN_RING_FAILED,
}TOKEN_RING_STATE_t;

//typedef	struct
//{
//	u8 	pk_type;
//	u8 	send_id;
//	u8 	rcv_id;
//	u16 pk_crc;
//}TokenRing_OK_PK;
//

typedef struct
{
	u8	node_ID;			//�ھӽڵ�
	u8	mac_timeouts_cnt;		//�ھӽڵ�mac�㳬ʱδ�ظ���ʱ��
}MAC_NB_State_t;

typedef struct
{
	u8	node_ID;			// һ���ھ�ID
}MAC_ONE_HOP_NB_ITEM;

typedef	struct
{
	u8	node_ID;			// ��LSA���ĵ�nb_list����ȡ��ID���Ǳ��صĶ����ھ�
	u8	relay_ID;			// �м�ID��Ϊ���ص�˫��һ���ھӣ�Ҳ���յ���LSA���ĵķ�����
	u8	hop;				// �ӱ��ص���node_ID���������ض���2��������
}MAC_MULTI_HOP_NB_ITEM;


typedef struct{
	unsigned short	rreq_src_ID;	// �������ķ���RREQ��ԴID��Ҳ��RREP�����е�rreq_src_ID
	unsigned short	rreq_seq;		// RREQ��ţ���rreq_src_IDһ��Ψһʶ��һ��·�ɽ�������ڱ�ʶ·�ɵ���Դ���¾ɡ�

	unsigned short	dest_ID;		// ����һ����RREQ�е�Ŀ��ID���п�����������ȡ��ĳ���м�ڵ���ΪĿ��ID
	unsigned short	next_ID;		// ��һ��ID����ҪΪ�˷����ھӶ���ʱ��ɾ������Ŀ

	unsigned char	hop;			// ����dest_ID������
	unsigned short	trace_list[MAC_MAX_HOP];		// ����ID-->dest_ID��·���б�trace_list[0]Ϊ����ID, trace_list[hop]Ϊdest_ID��
}MAC_IERP_ITEM;




//Add by wujiwen, 2018/6/25, ע��ͬ��Routing.h\ Congitive.h�ж����LSA_PK_TYPE��...RERR_PK_TYPE��
#define		TOKEN_RING_REQ_PK_TYPE				0x17
#define		TOKEN_RING_REP_PK_TYPE				0x18
#define		TOKEN_RING_OK_PK_TYPE				0x19

#define		TOKEN_RING_BC_REQ_PK_TYPE			0x20

#define		TOKEN_RING_RX_ROUTE_DATA_PK_TYPE	0x21

#define		TOKEN_RING_OK_ACK_PK_TYPE			0x22

void Poll_Members_Control_Channel();

//extern TOKEN_RING_STATE 	global_token_ring_state;

int TokenRingReq_PK_Send( u8 dest_ID);

int Handle_rcv_token_ring_req(TokenRing_Req_PK* req_pkptr);

int Handle_rcv_tokenRing_bc_req(TokenRing_BC_Req_PK *bc_req_pkptr);

int Handle_rcv_token_rep(TokenRing_Rep_PK *tokenRing_rep_pkptr);

void TokenRing_OK_ACK_Send(u8 rreq_src_id, u8 rreq_dest_id);

int Update_TokenRing_BC_REQ_Tab(TokenRing_BC_Req_PK *bc_req_pkptr);
int Is_MacOneHopNb(u8 id);
int Is_MacTwoHopNb(u8 node_ID,u8 relay_ID);
int Is_MacIERPHopNb(u8 rreq_src_ID , u8 rreq_dest_ID,  u8* trace_list);

void Set_TR_State(TOKEN_RING_STATE_t  state);

TOKEN_RING_STATE_t Get_TR_State();

pthread_t create_mac_thread();


#define DEF_GET_MSG_NUM

#endif /* MAC_H_ */
