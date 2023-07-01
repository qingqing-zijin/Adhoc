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
 * routing.c和mac.c MAC控制状态机变量互斥锁
 * */
extern pthread_mutex_t mac_st_mutex;

/*
 * routing.c和mac.c MAC层主站到各从站路由表维护互斥锁
 * */
extern pthread_mutex_t mac_LinkList_mutex;

extern volatile u8  g_BC_Req_rcvd ;

/*
 * 中继跳数
 * */
#define     MAC_MAX_HOP                        4


/*
 * 关于单播请求发送到接收到单播回复的最大时延，其决定因素如下：
 * 1》多跳中继指标设置；针对哈工程项目，最大中继次数为3跳；标记为Ｎ=3；
 * 2》1跳内请求发送到接收到请求ＡＣＫ的时间，标记为t2=150；
 * 3》用户占用1次信道最大时间，与发送的数据的最大长度直接相关；
 *    针对哈工程项目，用户1次占用信道，最大发送字节数为600字节，标记为t3= 500ms；
 * 4》节点发送1次接入控制报文最大时间开销，标记为t4 =80;
 * */

#define NET_MAX_NODES			(4U)
#define NET_MAX_RELAY_VAL		(NET_MAX_NODES-1)
/*
 *MAC层控制报文传输最大时间开销, t4
 * */
#define MAC_PK_MINI_MS			(80U)

#define MAC_MAX_MSG_LEN  		200

//#define NOT_USED_OK_ACK

#ifdef NOT_USED_OK_ACK
/*
 * 没有TR_OK_ACK时，MAC层控制报文数量包括: TR_REQ_PK/TR_REP_PK/TR_OK_PK
 * */
#define MAC_CTL_PKS				(3)

#else
/*
 * 有TR_OK_ACK时，MAC层控制报文数量包括: TR_REQ_PK/TR_REP_PK/TR_OK_PK/TR_OK_ACK_PK
 * */
#define MAC_CTL_PKS				(4U)

#endif

/*
 * ROUTE数据报文占用信道时传输一次报文的最大时间开销。
 * 与数据包最大字节数和速率有关， 最大发送字节数为600字节、19.2kbps时，t3= 500ms；
 * */
#define ROUTE_PK_MAX_MS			(600U)

/*
 * 一跳子网内TR_BC_PK发送至收到ACK超时时间
 * */
#define BC_REP_TIMEOUT_MS		(MAC_PK_MINI_MS*2)

/*
 *多跳子网内TR_REQ_PK单播发送至请求REP_ACK超时门限
 * */
#define REP_TIMEOUT_MS			(MAC_PK_MINI_MS*(NET_MAX_NODES-1)*2)

/*
 * 主站从发送轮询命令开始，到收到从节点业务报文的最大时间开销。
 * SLICE_TIMEOUT_MS影响主站轮询各从站时间间隔周期。必须保障从站1次占用信道发送成功结束后,才能开启下一次轮询。
 * (暂时不用该参数，由REP_TIMEOUT_MS、OK_ACK_TIMEOUT_MS替换)
 * */
#define SLICE_TIMEOUT_MS		((NET_MAX_NODES-1)*(MAC_CTL_PKS*MAC_PK_MINI_MS)+ROUTE_PK_MAX_MS)

/*
 *TR_OK_PK发送后，等待Route数据 + OK_ACK_PK;
 *由于子网内多节点互相交换数据通信场景，Route数据不一定是发给主控节点的，但要考虑Route发送最大开销。
 * */
#define RX_ROUTE_DATA_TIMEOUT_MS (ROUTE_PK_MAX_MS)
/*
 *TR_OK_PK发送后，等待ACK超时最大门
 * */
#define ROUTE_AND_OK_ACK_TIMEOUT_MS	((MAC_PK_MINI_MS*(NET_MAX_NODES-1)*2) + RX_ROUTE_DATA_TIMEOUT_MS)	//乘以[2]即代表发送OK_PKT和接收OK_ACK_PKT开销之和

#define OK_ACK_TIMEOUT_MS			((MAC_PK_MINI_MS)*(NET_MAX_NODES))

/*
* radio	4.8kbps时，每秒600字节，每500ms 300字节，每250ms 150字节，每100ms 60字节， 每50ms 30字节
* radio	9.6kbps时，每秒1200字节，每500ms 600字节，每250ms 300字节，每100ms 120字节， 每50ms 60字节
* radio	19.2kbps时，每秒2400字节，每500ms 1200字节，每250ms 600字节，每100ms 240字节， 每50ms 120字节
*
* 高：
* 4.8kbps  55B 200ms
* 9.6kbps  55B 135ms
* 19.2kbps 55B 80ms (发送需要80ms,若考虑发送请求-到-接收到ＡＣＫ，需要160ms)
* */


typedef	struct {
	u8  pk_type;
	u8	send_ID;		// 发送者ID，可能是中间节点或源节点
	u8	rcv_ID;			// 接收者ID，可能是中间节点或目的节点
	u8	src_ID;			// 源节点ID
	u8	dest_ID;		// 目的节点ID
	u8	rreq_src_ID;	// RREQ源节点ID，与rreq_seq一起定位一条路由记录
	u16 rreq_seq;		// 若为0，则表示域内路由，不会查找并更新域间路由表的使用时间，等其过期删除。
									// 若不0, 则表示RREQ序号，从trace_list中提取下一跳。（删，不与本地比较，查找太慢） 若大于本地路由记录中的rreq_seq，则用新路径代替旧的
	u8	hop;			// 单播时有效，为从src_ID到dest_ID的跳数；

	u8	trace_list[MAC_MAX_HOP];	// 单播时有效，为src_ID-->dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID，中间节点从这里找到下一跳节点，这就是所谓的源路由
	u16 pk_crc;
}TokenRing_OK_PK;


typedef	struct {
	u8  pk_type;
	u8	send_ID;		// 发送者ID，可能是中间节点或源节点
	u8	rcv_ID;			// 接收者ID，可能是中间节点或目的节点
	u8	src_ID;			// 源节点ID
	u8	dest_ID;		// 目的节点ID
//	u8	rreq_src_ID;	// RREQ源节点ID，与rreq_seq一起定位一条路由记录
	u16 rreq_seq;		// 若为0，则表示域内路由，不会查找并更新域间路由表的使用时间，等其过期删除。
									// 若不0, 则表示RREQ序号，从trace_list中提取下一跳。（删，不与本地比较，查找太慢） 若大于本地路由记录中的rreq_seq，则用新路径代替旧的
//	u8	TTL;			// 广播生存时间，限制广播范围。源端置为6，每转发一次递减1，到0时不再广播转发，即发6次
//	u8	hop;			// 单播时有效，为从src_ID到dest_ID的跳数；

	u8	trace_list[MAC_MAX_HOP];	// 单播时有效，为src_ID-->dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID，中间节点从这里找到下一跳节点，这就是所谓的源路由
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
	unsigned char	pk_type;		// 报文类型，RREP，单播发送，rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
	unsigned char	reserved;		// 填充字节，2字节对齐
	unsigned char	send_ID;		// 发送者ID，中间节点ID或dest_ID
	unsigned char	rcv_ID;			// 接收者ID，中间节点ID或src_ID; 收到之后，在trace_list中逆向寻找新rcv_ID

	unsigned char	rreq_src_ID;	// 发起RREQ的源ID，也是回复RREP的目的ID
	unsigned char	rreq_dest_ID;	// 发送RREQ的目的ID，也是RREP的源ID
	unsigned short	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次路由结果，用于标识路由的新旧。

//	unsigned char	path_QOS;		// 路径质量，比较报文内原值与本地node_qos，选择较小的一个替换该字段，源节点发送时的初值为自身node_qos。暂不用
	unsigned char	hop;			// 从src_ID到dest_ID的跳数
//	u8				tokenRingOk_ack;
	unsigned char	msgList_ack;	// 本节点是否消息队列中是否有消息待发送

	unsigned char	trace_list[MAC_MAX_HOP];	// 从src_ID到dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID

	unsigned short	pk_crc;			// 整个RREP报文的CRC

}TokenRing_Rep_PK;


typedef struct{
	unsigned char	pk_type;		// 报文类型，RREP，单播发送，rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
	unsigned char	reserved;		// 填充字节，2字节对齐
	unsigned char	send_ID;		// 发送者ID，中间节点ID或dest_ID
	unsigned char	rcv_ID;			// 接收者ID，中间节点ID或src_ID; 收到之后，在trace_list中逆向寻找新rcv_ID

	unsigned char	rreq_src_ID;	// 发起RREQ的源ID，也是回复RREP的目的ID
	unsigned char	rreq_dest_ID;	// 发送RREQ的目的ID，也是RREP的源ID
	unsigned short	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次路由结果，用于标识路由的新旧。
	unsigned char	hop;			// 从src_ID到dest_ID的跳数
	unsigned char	trace_list[MAC_MAX_HOP];	// 从src_ID到dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID

	unsigned short	pk_crc;			// 整个RREP报文的CRC

}TokenRing_OK_ACK_PK;



typedef struct{
	u8	pk_type;		// 报文类型，RREQ，始终是广播发送，但是接收者会查验自己是在mpr_list内，来决定是否转发
	u8	TTL;			// 广播生存时间，限制广播范围。源端置为MAX_HOP-1，每转发一次递减1，到0时不再广播转发

	u8	reserved1;
	u8	send_ID;		// 发送者ID，可能是中间节点或源节点

	u8	reserved2;
	u8	rcv_ID;			// 接收者ID，可能是中间节点或目的节点

	u8	src_ID;			// 发起RREQ的源ID
	u8	dest_ID;		// RREQ的应答节点ID，即RREQ的目的ID
	u16	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次寻路过程，与dest_ID无关。针对一个src_ID+rreq_seq,一个节点只响应一次，除非后来的path_cost更小

//	u8	path_QOS;		// 路径质量，比较报文内原值与本地node_qos，选择较小的一个替换该字段，源节点发送时的初值为自身node_qos。暂不用
	u8	hop;			// 从src_ID到本地的跳数，所有源节点发送RREQ时，hop置为初值1，以后每次转发递增1
	u8  trace_list[MAC_MAX_HOP];	// trace_list[0]为src_ID, trace_list[hop-1]为发送节点，若本机为发送节点的MPR，则置 trace_list[hop]为本地ID，更新上述3值后发出；

	unsigned short	pk_crc;					// 整个RREQ报文的CRC
}TokenRing_BC_Req_PK;


typedef struct
{
	u8	pk_type;
	u8	src_ID;
	u8	dst_ID;
}TokenRing_RX_RouteDATA_PK;

typedef struct
{
	u8	rreq_src_ID;			// 发起RREQ的源ID
	u8	rreq_dest_ID;		// RREQ的应答节点ID，即RREQ的目的ID
	u16	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次寻路过程，与dest_ID无关。针对一个src_ID+rreq_seq,一个节点只响应一次，除非后来的path_cost更小

	u8	hop;			// 从src_ID到本地的跳数，所有源节点发送RREQ时，hop置为初值1，以后每次转发递增1
	u8	trace_list[MAC_MAX_HOP];	// trace_list[0]为src_ID, trace_list[hop-1]为发送节点，若本机为发送节点的MPR，则置 trace_list[hop]为本地ID，更新上述3值后发出；
											//  若本机为dest_ID,将置 trace_list[hop]为本地ID后，hop和 trace_list 组成一条备选路由
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
	u8	node_ID;			//邻居节点
	u8	mac_timeouts_cnt;		//邻居节点mac层超时未回复计时器
}MAC_NB_State_t;

typedef struct
{
	u8	node_ID;			// 一跳邻居ID
}MAC_ONE_HOP_NB_ITEM;

typedef	struct
{
	u8	node_ID;			// 从LSA报文的nb_list中提取的ID，是本地的多跳邻居
	u8	relay_ID;			// 中继ID，为本地的双向一跳邻居，也是收到的LSA报文的发送者
	u8	hop;				// 从本地到达node_ID的跳数，必定是2跳及以上
}MAC_MULTI_HOP_NB_ITEM;


typedef struct{
	unsigned short	rreq_src_ID;	// 监听到的发起RREQ的源ID，也是RREP报文中的rreq_src_ID
	unsigned short	rreq_seq;		// RREQ序号，与rreq_src_ID一起唯一识别一次路由结果，用于标识路由的来源与新旧。

	unsigned short	dest_ID;		// 并不一定是RREQ中的目的ID，有可能是旁听截取的某个中间节点作为目的ID
	unsigned short	next_ID;		// 下一跳ID，主要为了方便邻居断裂时，删除该条目

	unsigned char	hop;			// 到达dest_ID的跳数
	unsigned short	trace_list[MAC_MAX_HOP];		// 本机ID-->dest_ID的路径列表，trace_list[0]为本机ID, trace_list[hop]为dest_ID，
}MAC_IERP_ITEM;




//Add by wujiwen, 2018/6/25, 注意同步Routing.h\ Congitive.h中定义的LSA_PK_TYPE、...RERR_PK_TYPE等
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
