#ifndef		ROUTING_H
#define		ROUTING_H

#include	<stdio.h>
#include	<stdlib.h>
#include	<memory.h> 
#include	<math.h>
#include	<netinet/in.h>
#include	<signal.h>
//#include	<Winsock2.h>
//#pragma comment(lib,"ws2_32.lib");

#define		NO_WAIT		IPC_NOWAIT

/*----------------------------------------------------------------------
				     		IARP-FSR	
----------------------------------------------------------------------*/
//#define     PERIOD_LSA                  1000     	// LSA 发送周期 3秒，不按FSR中所描述的“大外径邻居节点通告周期长”的方法做，简单起见，统一按一个周期     

#define		MAX_NB_NUM					250			// 域内最大节点数，也就是N跳内邻居总数

/*----------------------------------------------------------------------
				     		IERP-DSR	
----------------------------------------------------------------------*/
/*  Timeouts  */
#define     IERP_PATH_TIMEOUT                   60000		// 域间路由条目超时时间，超过60秒未使用该路由条目，则删除
//#define     IERP_RREQ_TIMEOUT        			2000		// 域间路由请求超时时间，源节点发送RREQ后，1秒内未收到RREP，则本次寻路失败

#define     MAX_HOP                           	8		// 最大跳数，由于路径表达时包含src和dest，因此，实际跳数最大为MAX_HOP-1，即7跳。

#define		MAX_NODE_NUM						65536	// ID取值空间，最大为65536
/*----------------------------------------------------------------------
				     		Cluster
----------------------------------------------------------------------*/
/* 全部采用非交叠簇，因此，不会存在某一节点同时隶属于两个或两个以上簇头，但必须可以做网关*/
#define	  CLUSTER_NONE								0		// 无分簇，即工作在平面路由模式下
#define   CLUSTER_MEMBER                          	1		// 簇节点类型：普通簇成员，只与一个簇头相邻，一跳邻居中无簇外节点
#define   CLUSTER_GATEWAY                         	2		// 簇节点类型：网关成员，与多个簇头相邻，一跳邻居中有簇外节点
#define   CLUSTER_HEADER                         	3		// 簇节点类型：簇头，节点度最大、ID最小。

/*----------------------------------------------------------------------
				     		ZRP<->CLUSTER
----------------------------------------------------------------------*/
//#define     ZRP_TO_CLUSTER_NUM                   5		// ZRP过渡到Cluster的节点度门限
//#define     CLUSTER_TO_ZRP_NUM                   3		// Cluster过渡到ZRP的节点度门限

/*----------------------------------------------------------------------
				     		APP_BUFF		
----------------------------------------------------------------------*/
/* 需定义APP_BUFF缓冲池大小。在寻路期间，缓存待发送报文。只在源端缓存，中间节点无路径，则直接丢弃，因此，需要缓存ip报文 */
#define		BUFF_CACHE_TIMEOUT					60000		// 正在等待寻路的业务报文在APP_BUFF内的缓存时间，超过此时间，则删掉。仅缓存UDP报文
#define		MAX_APP_LIST_SIZE					1000	// BUFF内最多有1000个存储单元,即存1000包。溢出的报文不缓存
#define		BUFF_ITEM_SIZE						1600	// 每个存储单元1600字节（不小于最大业务报文长度）

typedef struct{
	
	unsigned long	record_time;	// 记录时间，即入队时间。每次遍历时比较该值与当前时间，若差值大于BUFF_CACHE_TIMEOUT，则删除
	unsigned short	dest_ID;		// 本报文的目的ID。方便遍历时确认该报文是否完成寻路，即路由表项是否建立
	unsigned short	app_len;		// 本报文的长度
	unsigned char	app_pk[BUFF_ITEM_SIZE];		// 包含路由头和源IP头的业务报文，路由头为无效值，寻路之后还要重新赋值
	}BUFF_ITEM;

/*----------------------------------------------------------------------
				     		DATA TYPE		
----------------------------------------------------------------------*/
#define		DATA_PK_TYPE				    0x00

#define		LSA_PK_TYPE			            0x10
#define		RREQ_PK_TYPE					0x11
#define		RREP_PK_TYPE					0x12
#define		RERR_PK_TYPE					0x13



/*----------------------------------------------------------------------
				     		Node Attribute	
----------------------------------------------------------------------*/
typedef	struct{
	unsigned short	ID;			// 设备ID = x.y，取值1~65534；唯一识别一个无线设备，iBox无线端IP地址为10.x.y.0，掩码255.0.0.0; 有线端IP为10.x.y.1，掩码255.255.255.0
	
	unsigned char	cluster_state;		//路由模式： 分簇状态，0为ZRP平面路由；1为普通簇成员，2为网关，3为簇头。
	unsigned char	cluster_size;		// 簇大小，及所有成员数目，仅分簇时有效，ZRP时置为0
	unsigned short	cluster_header;		// 簇头ID，仅分簇时有效，ZRP时置为0xffff
	
	unsigned char	node_qos;			// 节点质量，为本地发送速率,传输速率越小，对应的qos越小.在路由选择时，所有中间节点的最小node_qos，即为path_QOS。path_QOS越大越好。暂不用，置为1
	unsigned char	IARP_radius;		//域半径： 发送LSA_PK时，仅发送该半径以内的邻居节点。实际域半径为该值+1。比如该值=1时，LSA内携带自己和自己的一跳邻居，则实际上节点可以获得2跳邻居信息
	
	unsigned char	degree;				// 节点度：双向一跳邻居个数，方便分簇比较和LSA发送。
								
	unsigned long	rcvd_pk_num;		// 从无线端收到的业务报文的个数
	unsigned long	send_pk_num;		// 发给无线端的业务报文的个数
	unsigned long	rcvd_pk_size;		// 从无线端收到的业务报文的总字节数，app_len
	unsigned long	send_pk_size;		// 发给无线端的业务报文的总字节数， app_len

	
	}NODE_ATTR;			/* 节点属性，用于保存节点的以静态和动态属性*/


typedef struct{
	unsigned short	node_ID;			// 节点ID，即邻居节点ID
	unsigned char	path_QOS;			// 节点质量，LSA报文中send_ID到node_ID的链路质量。	
	unsigned char	mpr_distance;		// 组合字段，bit7：是否为LSA报文发送者的MPR节点，0不是，1是,只有一跳邻居才有可能是MPR；
                                        //           bit6: 是否为LSA报文发送者的双向邻居，0不是，1是。只有一跳邻居才有可能是1
										//           bit3~0:到LSA报文发送者的跳数，表示为几跳邻居

	}NB_INFO;			/* 邻居列表信息，用于填充报文内容,从邻居表中读取*/		
	
typedef struct{
	unsigned short	rreq_src_ID;			// 发起RREQ的源ID
	unsigned short	rreq_dest_ID;		// RREQ的应答节点ID，即RREQ的目的ID
	unsigned short	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次寻路过程，与dest_ID无关。针对一个src_ID+rreq_seq,一个节点只响应一次，除非后来的path_cost更小
				
	unsigned char	path_QOS;		// 路径质量，比较报文内原值与本地node_qos，选择较小的一个替换该字段，源节点发送时的初值为自身node_qos。暂不用
	unsigned char	hop;			// 从src_ID到本地的跳数，所有源节点发送RREQ时，hop置为初值1，以后每次转发递增1
	unsigned short	trace_list[MAX_HOP];	// trace_list[0]为src_ID, trace_list[hop-1]为发送节点，若本机为发送节点的MPR，则置 trace_list[hop]为本地ID，更新上述3值后发出；
											//  若本机为dest_ID,将置 trace_list[hop]为本地ID后，hop和 trace_list 组成一条备选路由	
	}RREQ_RECORD;		/* RREQ_PK_Rcvd() 中用到，防止重复广播。该表中对同一个（源，目的）节点对，只维护一条RREQ记录，并以seq区分新旧*/
	

/*----------------------------------------------------------------------
				     		PK structure	
----------------------------------------------------------------------*/

/* ************LSA 报文结构 ***************** */

/**  开机后，每PERIOD_LSA时间发送一次LSA_PK；若实测中冲突较多，可在一个周期内作随机退避，退避粒度设为1ms */
#define		LSA_PK_HEAD_LEN			10
#define		LSA_PK_LEN(x)			(((x)<<2)+12)
	
typedef	struct {
	unsigned char	pk_type;			// 报文类型，LSA，始终是广播发送，但是接收者只处理，不转发
	unsigned char	degree;				// 节点度，即双向可达的一跳邻居个数
	unsigned short	send_ID;			// 发送该LSA报文的ID
	
	unsigned char	cluster_state;		// 分簇状态，0为ZRP平面路由；1为普通簇成员，2为网关，3为簇头。
	unsigned char	cluster_size;		// 簇大小，及所有成员数目，仅分簇时有效，ZRP时置为0
	unsigned short	cluster_header;		// 簇头ID，仅分簇时有效，ZRP时置为0xffff
	unsigned char	node_qos;			// 节点质量，同NODE_ATTR，暂不用，置为1

	unsigned char	nb_num;				// 域内邻居个数,包含N跳邻居，根据设置处理
	NB_INFO			nb_list[MAX_NB_NUM];			// 域内邻居列表,最多有250个，占据1000字节
	
	unsigned short	pk_crc;				// LSA 报文的校验和。由于nb_list长度可变，0~1000字节，因此实际给pk_crc赋值时需在当前报文的末尾2字节填值
	}LSA_PK;

/* ************RREQ 报文结构 ***************** */
/**  状态合适，且没有域内路由，则开启域间寻路 */

#define		RREQ_PK_LEN			34

typedef struct{
	unsigned char	pk_type;		// 报文类型，RREQ，始终是广播发送，但是接收者会查验自己是在mpr_list内，来决定是否转发
	unsigned char	TTL;			// 广播生存时间，限制广播范围。源端置为MAX_HOP-1，每转发一次递减1，到0时不再广播转发
	
	unsigned short	send_ID;		// 发送者ID，可能是中间节点或源节点
	unsigned short	rcv_ID;			// 接收者ID，可能是中间节点或目的节点
	
	unsigned short	src_ID;			// 发起RREQ的源ID
	unsigned short	dest_ID;		// RREQ的应答节点ID，即RREQ的目的ID
	unsigned short	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次寻路过程，与dest_ID无关。针对一个src_ID+rreq_seq,一个节点只响应一次，除非后来的path_cost更小
				
	unsigned char	path_QOS;		// 路径质量，比较报文内原值与本地node_qos，选择较小的一个替换该字段，源节点发送时的初值为自身node_qos。暂不用
	unsigned char	hop;			// 从src_ID到本地的跳数，所有源节点发送RREQ时，hop置为初值1，以后每次转发递增1
	
	unsigned short	trace_list[MAX_HOP];	// trace_list[0]为src_ID, trace_list[hop-1]为发送节点，若本机为发送节点的MPR，则置 trace_list[hop]为本地ID，更新上述3值后发出；
											//  若本机为dest_ID,将置 trace_list[hop]为本地ID后，hop和 trace_list 组成一条备选路由

	unsigned short	pk_crc;					// 整个RREQ报文的CRC

	}RREQ_PK;
	


/* ************RREP 报文结构 ***************** */


/** 一般RREP由目的节点根据不同路径到达的RREQ，优选一条进行发送。从trace_list中反向提取rcv_ID字段*/

#define		RREP_PK_LEN		30	
typedef struct{
	unsigned char	pk_type;		// 报文类型，RREP，单播发送，rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
	unsigned char	reserved;		// 填充字节，2字节对齐
	
	unsigned short	send_ID;		// 发送者ID，中间节点ID或dest_ID
	unsigned short	rcv_ID;			// 接收者ID，中间节点ID或src_ID; 收到之后，在trace_list中逆向寻找新rcv_ID									
		
	unsigned short	rreq_src_ID;	// 发起RREQ的源ID，也是回复RREP的目的ID
	unsigned short	rreq_dest_ID;	// 发送RREQ的目的ID，也是RREP的源ID	
	unsigned short	rreq_seq;		// RREQ序号，与src_ID一起唯一识别一次路由结果，用于标识路由的新旧。

	unsigned char	path_QOS;		// 路径质量，比较报文内原值与本地node_qos，选择较小的一个替换该字段，源节点发送时的初值为自身node_qos。暂不用
	unsigned char	hop;			// 从src_ID到dest_ID的跳数
	
	unsigned short	trace_list[MAX_HOP];	// 从src_ID到dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID

	unsigned short	pk_crc;			// 整个RREP报文的CRC

	}RREP_PK;


/* ************RERR 报文结构 ***************** */

/** 仅在APP报文中trace_list提取出的下一跳ID不可达时，才逆向发送RERR
 *  若LSA连续丢失，不会触发RERR，否则就成了表驱动路由了。所以，中间节点不保存路由记录，也就无所谓删除操作*/

#define		RERR_PK_LEN			38

typedef struct{
	unsigned char	pk_type;		// 报文类型，RERR，单播发送，rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
	unsigned char	reserved;		// 填充，让结构体2字节对齐
	
	unsigned short	send_ID;		// 发送者ID，中间节点ID
	unsigned short	rcv_ID;			// 接收者ID，中间节点ID; 收到之后，在trace_list中逆向寻找新rcv_ID；src_ID收到后，若路由记录中的rreq_seq相同，则将其更新为最新的seq，新路由随下一条普通报文下发;若大于，则不理
			
	unsigned short	src_ID;			// 发现路径断裂，并发送RERR报文的节点
	unsigned short	dest_ID;		// RERR的目的节点，也就是业务报文的源节点
	
	unsigned short	rreq_src_ID;	// RREQ源节点ID，在源端与rreq_seq一起定位一条路由记录。从业务报头中提取	
	unsigned short	rreq_seq;		// RREQ序号，从业务报文中提取。源节点据此删除IERP路由。
	
	unsigned char	err_state;		// 错误状态，0：发现路径断裂（DSR中的下一跳不可达,LSA丢失时不再触发），且无法本地简单修复；1：发现断裂路径，但已经本地简单修复，新路径存在hop 和 trace_list中。
	unsigned char	hop;			// 从src_ID到dest_ID的跳数；【未修复之前，此值为旧值；若进行了本地修复，则此值变为新值，且trace_list也更新】
	unsigned short	trace_list[MAX_HOP];	// 从src_ID到dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID
	
	unsigned short	err_up_ID;		// 断裂路径的上游ID = src_ID，此处重复表达是为了意义明确
	unsigned short	err_down_ID;	// 断裂路径的下游ID
	unsigned short	err_repair_ID;	// 修复路径的ID，当错误状态为0时，该值为无效值0xffff
											
	unsigned short	pk_crc;			// 整个RERR报文的CRC

	}RERR_PK;


	
/* ************APP 报文结构 ***************** */
#define			ROUTE_HEAD_LEN		38
#define			MTU_LEN				1500

typedef struct{
	unsigned char	pk_type;		// 报文类型，可以LSA，RREQ，RREP，RERR，DATA。此处不作多播，若做，则再利用net区分多播
	unsigned char	reserved;		// 填充，让结构体2字节对齐
	
	unsigned short	send_ID;		// 发送者ID，可能是中间节点或源节点
	unsigned short	rcv_ID;			// 接收者ID，可能是中间节点或目的节点	
	
	unsigned short	src_ID;			// 源节点ID
	unsigned short	dest_ID;		// 目的节点ID
	
	unsigned short	rreq_src_ID;	// RREQ源节点ID，与rreq_seq一起定位一条路由记录
	unsigned short	rreq_seq;		// 若为0，则表示域内路由，不会查找并更新域间路由表的使用时间，等其过期删除。
									// 若不0, 则表示RREQ序号，从trace_list中提取下一跳。（删，不与本地比较，查找太慢） 若大于本地路由记录中的rreq_seq，则用新路径代替旧的
	
	unsigned char	TTL;			// 广播生存时间，限制广播范围。源端置为6，每转发一次递减1，到0时不再广播转发，即发6次			
	unsigned char	hop;			// 单播时有效，为从src_ID到dest_ID的跳数；	
	unsigned short	trace_list[MAX_HOP];	// 单播时有效，为src_ID-->dest_ID的路径列表，trace_list[0]为src_ID, trace_list[hop]为dest_ID，中间节点从这里找到下一跳节点，这就是所谓的源路由	
									
	unsigned short	bc_seq;			// 广播包序号，防止广播报文时的洪泛，使其仅朝"外圈"广播传递。即收到广播报文后，记录src和pk_seq，下次再收到小于记录的广播报文，不论TTL都不转发
	
	unsigned short	len;			// 后续IP报文的总长度
	unsigned short	head_crc;		// APP报头的CRC
	
	unsigned char	data[MTU_LEN];	//后续IP报文内容
	
	}APP_PK;



/*----------------------------------------------------------------------
				     		Table structure	
----------------------------------------------------------------------*/
	// 收到LSA报文后，首先在ONE_HOP_NB_TAB中更新或添加一条记录，主要是direction，可能由0变1，也可能由1变0
typedef struct{
	unsigned short	node_ID;			// 一跳邻居ID
		
	unsigned char	cluster_state;		// 分簇状态，0为ZRP平面路由；1为普通簇成员，2为网关，3为簇头。
	unsigned char	cluster_size;		// 簇大小，及所有成员数目，仅分簇时有效，ZRP时置为0
	unsigned short	cluster_header;		// 簇头ID，仅分簇时有效，ZRP时置为0xffff
		
	unsigned short	rcv_state;			// 接收统计，初值为0xff00; 每次发送LSA之前，更新所有邻居的rcv_qos，若距离上次接收时间差>PERIOD+50,直接左移1位，否则移位后+1。
	unsigned char	rcv_qos;			// 接收质量。 rcv_state统计‘1’的个数得到
	unsigned char	send_qos;			// 发送质量，由对端的rcv_state统计‘1’的个数得到。
	
	unsigned char	path_QOS;			// 链路质量。 每次发送和接收LSA时都更新，取值 = min{本地接收LSA连续度的统计，邻居接收LSA连续度的统计值}
	unsigned char	degree;				// 节点度，即双向可达的一跳邻居个数

	unsigned char	direction;			// 链路方向，0：只能收不能发，单向通 （该邻居LSA中的邻居列表中没有自己）；
										//       1：双向可通（该邻居LSA中的邻居列表中有自己）

	unsigned char	is_my_mpr;			// 1：node_ID是本节点的MPR；0：不是。通过本地计算得到
	unsigned char	is_your_mpr;		// 1: 本节点是node_ID的MPR；0：不是。通过解析LSA报文直接赋值
	
	unsigned long long	last_lsa_rcv_time;		// 最近接收该节点LSA的时间，超时后需要在ONE_HOP_NB_TAB和LSA_PK_Tab中删掉该邻居节点
	}ONE_HOP_NB_ITEM;
	
	// 之后，若该邻居为双向邻居，则根据LSA报文更新MULTI_HOP_NB_TAB；若是单向邻居（LSA报文中的邻居表中不包含自己了），则删除所有relay_ID是send_ID的记录;
	//       这样做，当一条最短路径内的relay_ID断裂，可能造成根其相关的所有2跳邻居暂时消失(最短路径的下一跳唯一)，但一个周期后会恢复(在更大hop条件下找到新路由)。
	//			[注]:直接截取LSA报文的nb_list内容时，会出现到达同一个目的ID，经历相同或不同的relay_id，历经相同或不同的跳数的不同条目。加入本表时，到达同一个dest_ID，只保留hop最小的项（仍可能有多个，但是relay_id不同）
	//				同时添加时，按序排列：先以hop排序2,3,4...；同一跳数条件下，node_id之间不排序，但相同node_id的条目要紧邻。由上两行描述可知，相同node_id的条目，hop必定相同
	//       【按此方式建立的多跳邻居表，与一跳邻居表一起，可作为域内路由表】
typedef	struct {
	unsigned short	node_ID;			// 从LSA报文的nb_list中提取的ID，是本地的多跳邻居
	unsigned short	relay_ID;			// 中继ID，为本地的双向一跳邻居，也是收到的LSA报文的发送者		
	unsigned char	path_QOS;			// 链路质量，收到LSA报文后，先计算本地到send_ID的QOS，再与报文内携带的nb_list[]->path_QOS比较，取较小值	
	unsigned char	hop;				// 从本地到达node_ID的跳数，必定是2跳及以上

	}MULTI_HOP_NB_ITEM;
	
	// 根据邻居的LSA，获得两跳邻居列表，并计算出一跳邻居中谁是我的MPR，然后在在LSA的邻居列表中通告出去。我的邻居收到后，就能知道其是不是我的MPR，当我广播发送时，只有我的MPR才转发。
	// 设计一种数据结构，提取出一跳内的双向邻居，及其双向邻居（自身的双向两跳邻居），然后计算具有最大两跳邻居并集的最小一跳邻居集，得到的一跳邻居集即为我的MPR列表。
	// 每次一跳或两跳邻居发生变化，都要重新计算MPR，因此，可以简单地在每次收到LSA时重新计算MPR，并更新ONE_HOP_NB_TAB

	// 之后，根据LSA报文列表更新域内路由表，dest_ID可能是1,2跳甚至3,4跳的邻居。
/*
typedef struct{
	unsigned short	dest_ID;		// 域内节点ID
	unsigned short	next_ID;		// 下一跳ID，即用作中继的一跳邻居节点
	unsigned char	path_QOS;		// 链路质量	
	unsigned char	hop;			// 本节点到达dest_ID的跳数	
	}IARP_ITEM;
*/	
	
	// IERP中记录的路由条目，源地址一定是自己，此条目不是直接将网络上监听到的RREP复制进来，而是截取一段trace_list构造出来的。
	// 如果本次操作涉及一跳邻居的删除（或双通转单通）操作，则需查找域间路由表，将next_ID为该邻居的条目全部删除。
	// 中间节点不会记录起始地址不是自己的IERP路由条目，因此，当收到LSA发现邻居断裂时，也不会向所谓的RREQ源节点发RERR，
	//        只会在下次发送业务报文时发觉链路断裂，并根据业务报头内的信息逆向发送RERR
typedef struct{
	unsigned short	rreq_src_ID;	// 监听到的发起RREQ的源ID，也是RREP报文中的rreq_src_ID
	unsigned short	rreq_seq;		// RREQ序号，与rreq_src_ID一起唯一识别一次路由结果，用于标识路由的来源与新旧。
	
	unsigned short	dest_ID;		// 并不一定是RREQ中的目的ID，有可能是旁听截取的某个中间节点作为目的ID
	unsigned short	next_ID;		// 下一跳ID，主要为了方便邻居断裂时，删除该条目
	
	unsigned char	path_QOS;		// 链路质量：直接寻路的源和目的端可以获知path_QOS,但中间节点无法准确获知。只能大概知道不会低于该值
									//       因此，域间寻路时不参考path_QOS，所以，被寻路的目的端作延迟等待，优选一条路径回复RREP就尤为重要
	unsigned char	hop;			// 到达dest_ID的跳数
	unsigned short	trace_list[MAX_HOP];		// 本机ID-->dest_ID的路径列表，trace_list[0]为本机ID, trace_list[hop]为dest_ID，
	
	unsigned long   last_use_time;	// 最近一次使用dest-->src或者src-->dest的时间。超时未使用时，需要删除该条路由
	
	}IERP_ITEM;


/* 所有seq变量均取值为1~65535,0保留，表示其它特殊含义*/
#define		SEQ_INC(x)		(x = ((unsigned short)(x)==0xffff ? 1:(x)+1))



extern void Route_Serv();

extern void APP_PK_Send(unsigned char *app_pkptr, unsigned short app_len);
extern int	APP_PK_Rcvd(unsigned char *app_pkptr, unsigned short app_len);
extern void LSA_PK_Send();
extern int LSA_PK_Rcvd(LSA_PK *lsa_pkptr, unsigned short lsa_pklen);
extern void RREQ_PK_Send(unsigned short dest_id );
extern int RREQ_PK_Rcvd(RREQ_PK *rreq_pkptr, unsigned short rreq_pklen);
extern void RREP_PK_Send(unsigned short rreq_src_id, unsigned short rreq_dest_id);
extern int RREP_PK_Rcvd(RREP_PK *rrep_pkptr, unsigned short rrep_pklen);
extern void RERR_PK_Send(unsigned short rcv_id, unsigned short dest_id, unsigned short err_down_ID, APP_PK *app_pkptr);
extern int RERR_PK_Rcvd(RERR_PK *rerr_pkptr, unsigned short rerr_pklen);
extern pthread_t create_route_thread();
int getQueuesIds();
int createWds();
void handleLanData(unsigned char *msgBuff,int msgLen);
void handleWLanData(unsigned char *msgBuff,int msgLen);
void handleSelfData(unsigned char *msgBuff,int msgLen);
void RREQ_TimeoutHandler(int sig,siginfo_t *si,void *uc);
void LSA_PK_SendHandler(int sig,siginfo_t *si,void *uc);
int Seq_Cmp(unsigned short x, unsigned short y);
extern NODE_ATTR			Node_Attr;		// 本地节点属性，包括ID，路由状态，收发统计等

extern unsigned char 		LSA_Flag;		// LSA 发送开关，为0关闭，为1打开

extern unsigned int    RADIUS;                 // 域半径：
extern unsigned int	   CLUSTER_TO_ZRP_NUM;		// Cluster过渡到ZRP的节点度门限  3
extern unsigned int	   ZRP_TO_CLUSTER_NUM;		// ZRP过渡到Cluster的节点度门限 5
extern unsigned int    RETRY_TIMES;             // LSA超时时间
extern unsigned int    PERIOD_LSA;              //LSA 发送周期
extern unsigned int    IERP_RREQ_TIMEOUT;       // RREQ 超时时间
#endif
