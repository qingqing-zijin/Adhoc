/*
 * auv.c
 *
 *  Created on: May 17, 2018
 *      Author: root
 */
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_UDP, INET_ADDRSTRLEN
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>        // macro ioctl is defined
#include "main.h"
#include "aux_func.h"
#include "queues.h"
#include <netpacket/packet.h> //struct sockaddr_ll
#include <ifaddrs.h>
#include "auv.h"
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include "Routing.h"
#include "linklist.h"

extern int 		self_node_id;
extern List*	oneHopNb_List;		// 一跳邻居链表，元素是ONE_HOP_NB_ITEM
extern List*	mHopNb_List;		// 多跳邻居链表，元素是MULTI_HOP_NB_ITEM
extern List*	IERP_List;			// 域间路由链表，元素是IERP_ITEM

int nsv_sock =-1;
struct sockaddr_in daddr;

void nsv_NodeLinks_TableReport()
{
	unsigned char 	msg[100]={0,};
	int				msglen = 0;
//	unsigned short	node_num = 0;
	int	i = 1;
	ONE_HOP_NB_ITEM		*element_ptr1;
	MULTI_HOP_NB_ITEM	*element_ptr2;
	IERP_ITEM			*element_ptr3;

	/*
	 * STEP1> 将本节点域内1跳邻居状态发送给USV，包括域内1跳节点的ID
	 * */
	//数据头
	msg[0] = '$';
	msg[1] = 'G';
	msg[2] = 'U';
	msg[3] = ',';

	//报文长度
	msg[4] = 0;// 发送前再修改
	msg[5] = ',';

	//指令源
	msg[6] = (self_node_id&0xff)+0x30;
	msg[7] = ',';

	//目的id
	msg[8] = '1';
	msg[9] = ',';

	msglen = 10;
	/*
	 * 一跳邻居列表，只报双向邻居id
	 * 数据格式： 2/3/4,代表节点2,3,4均为1跳节点
	 *
	 * 岸基处理时，先根据指令源确定节点id，然后根据提取1跳邻居id, 再根据角色为中继节点的1跳节点id，提取到达两跳节点id，
	 *
	 * */
	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{
		if(element_ptr1->direction == 1)	// 只报双向邻居
		{
			msg[msglen++] = (( element_ptr1->node_ID)&0xff) + 0x30;		// dest id
			msg[msglen++] = '/';

//			msg[msglen++] = 0x01;						// hop
//			msg[msglen++] = (element_ptr1->is_my_mpr<<7)+element_ptr1->path_QOS;			// mpr
		}
		i++;
	}
	msg[msglen++] = ';';
	/*
	 * 两跳邻居列表，只报目的id及其中继节点id，
	 * 数据格式：2-3/1-4, 表示本节点至3、4分别经过中继2和1
	 * */
	i= 1;
	while(NULL != (element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mHopNb_List, i)))
	{
		msg[msglen++] = (( element_ptr2->relay_ID)&0xff) + 0x30;		// next id
		msg[msglen++] = '-';
		msg[msglen++] = (( element_ptr2->node_ID)&0xff )+ 0x30;		// dest id
		msg[msglen++] = '/';
		i++;
//		node_num++;
	}

	msg[msglen++] = ';';
	/*
	 * 域间路由，则为3跳以上节点，报发送目的id及其中继节点id，
	 * 数据格式：2-3-1-4; 表示本节点节点为2,至节点4,分别经过中继3和1
	 * 2-1-4-5; 表示本节点节点为2至节点5,分别经过中继1和4
	 * */
	i = 1;
	while(NULL != (element_ptr3 = (IERP_ITEM*)prg_list_access(IERP_List, i)))
	{
//		msg[msglen++] = ((element_ptr3->trace_list[0])&0xff) + 0x30;		// next id
//		msg[msglen++] = '-';
//		msg[msglen++] = ((element_ptr3->trace_list[1])&0xff) + 0x30;		// dest id
//		msg[msglen++] = '-';
		msg[msglen++] = ((element_ptr3->trace_list[2])&0xff)+ 0x30;			// 2rd send id
		msg[msglen++] = '-';
		msg[msglen++] = ((element_ptr3->trace_list[3])&0xff)+ 0x30;			// 3rd send id
		msg[msglen++] = '/';
//		msg[msglen++] = element_ptr3->hop-1;				// hop
//		msg[msglen++] = element_ptr3->path_QOS;				// mpr
		i++;
//		node_num++;
	}
	msg[msglen++] = ',';			//
	msg[msglen++] = '*';			//结束符号	```

	msg[4] = (msglen)&0xff;

	if (sendto(nsv_sock, msg, msglen, 0, (struct sockaddr *) &daddr,sizeof(daddr)) <0)
	{
		LOG_MSG(ERR_LEVEL,"report err\n");
	}
}



/*
 * @function nsv_relay_auv_pkt
 * @desc.:
 *
 * */
int node_relay_auv_pkt(	unsigned char	*IP_pkptr)
{
	/*
	 * 获取UDP目的端口信息
	 * */
	struct udphdr udphdr;
	char 	*tranhead;
	int 	destport;
	int  	user_data_len =0;
	u8*		user_data = NULL;
	struct ip iphdr;

	//拷贝IP头
	memcpy(&iphdr, IP_pkptr, sizeof(iphdr));
	/*
	 * 由于通控与水声模块进行通信时，是通过有线RS232接口进行直接通信的。当大艇与水下航行器进行通信时，水面无人艇需要进行转发处理。开发实现
	 * $USV -> $AUV的转发策略是：
	 * 1> 大艇总控平台（192.0.1.3）将数据打包好，通过调用UDP接口sendto()函数发送数据包，
	 * 在sendto()函数中指定某个NSV节点IP地址+端口作为目的地址和目的端口（8015代表$USV -> $AUV的数据包）。
	 *
	 * 2> 小艇收到来自大艇的数据包时，在通控网关程序中判断是否为$UA数据包，如果是则表示接收到$UA数据包,转到3）处理。
	 *
	 * 3> NSV收到$UA数据包后，一方面调用水声模块通信接口（RS232 或其它）和协议解析/转换模块，转发给对应的AUV平台;
	 * 	  另一方面由Routing.c中APP_PK_Rcvd()msgQ_snd发送给NSV自主平台。
	 *
	 * $AUV-> $USV的策略是：
	 * 1> 参考ethrecv.c中NSV转发PC端口收到的$AU数据包。
	 * 2> 大艇从无线收到$AU数据包时，在APP_pk_Rcvd()发给PC终端。
	 * 3> 若$AU数据包经过某个节点转发时，属于IP层数据报文转发，无需在应用层处理。
	 *
	 * PC端发送“123”时，提示输出： head_len =5(对应IP头为20字节), total_bytes =31（IP头+ TCP（20字节）/UDP头(8字节) + APP_DATA）,   。
	 *	31 = 20(IP头) + 8（UDP头）+ 3（AppData）
	 * WJW：tranhead指向传输层UDP head的收地址
	 */
	tranhead = IP_pkptr + iphdr.ip_hl * 4; //to the udp or tcp head;
	memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
	destport = ntohs(udphdr.dest);

	user_data_len = ntohs(udphdr.len)-8;
	user_data = tranhead +8;

	char* dst_auv_id = find_n_comma(user_data, 4);

	if(user_data[0] == '$' &&  user_data[1] == 'U' &&  user_data[2] == 'A')
	{
		//转发$UA至对应的AUV平台; 转发NSV自主平台由Routing.c中msgQ_snd完成
		if(*(dst_auv_id+1) == '5')
		{
			//发送USV至AUV5水下航行器
			nsv_sendto_auv_or_usv(AUV5,user_data, user_data_len);
		}
		else if(*(dst_auv_id+1) == '6')
		{
			//发送USV至AUV6水下航行器
			nsv_sendto_auv_or_usv(AUV6, user_data, user_data_len);
		}
		else
		{
			LOG_MSG(ERR_LEVEL, "unkown pkt $UA to AUV%c,\n", *(dst_auv_id+1));
			return 0;
		}
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
 * nsv_network_init
 * 初始化NSV节点通信接口，创建套接字，为转发$AU, $UA数据包准备
 * */
int nsv_network_init()
{
	struct sockaddr_in BindAddrto;

	char nsv_dst_ip[16] ={0};

	int nsv_listen_port = 8031;

	/*
	 * 转发至USV或NSV
	 * */

	sprintf(nsv_dst_ip,"192.0.%d.3", self_node_id);


	bzero(&BindAddrto, sizeof(struct sockaddr_in));

//	设置监听本机IP地址+端口
	BindAddrto.sin_family = AF_INET;
	BindAddrto.sin_addr.s_addr = htonl(INADDR_ANY);
	BindAddrto.sin_port = htons(9000);

	daddr.sin_family = AF_INET;
	if(self_node_id ==1)
	{
		nsv_listen_port = 8011;
	}
	else if(self_node_id ==2)
	{
		nsv_listen_port = 8021;
	}
	else if(self_node_id ==3)
	{
		nsv_listen_port = 8031;
	}
	else if(self_node_id ==4)
	{
		nsv_listen_port = 8041;
	}

	daddr.sin_port = htons(nsv_listen_port);
	daddr.sin_addr.s_addr = inet_addr(nsv_dst_ip);

	if ((nsv_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		LOG_MSG(ERR_LEVEL,"lan create recv  socket ");
		exit(0);
	}

	if(bind(nsv_sock,(struct sockaddr *)&(BindAddrto), sizeof(struct sockaddr_in)) == -1)
	{
		LOG_MSG(ERR_LEVEL,"bind error\n");
		return -1;
	}
}

/*
 * @function nsv_sendto_selfModules
 * @nsv转发auv $AU命令至内部自主平台
 * */
int nsv_sendto_selfPlatfrom(node_type_t node_t,u8* msg , u32 msglen)
{
	struct sockaddr_in auv_daddr, nsv_daddr;
	char auv_dst_ip[16] ={0};
	char nsv_dst_ip[16] ={0};
	/*
	 * $UA,和$AU都点对点转发至NSV，由NSV内部广播
	 * */
	nsv_daddr.sin_family 		= AF_INET;
	sprintf(nsv_dst_ip,"192.0.%d.3", self_node_id);

	nsv_daddr.sin_addr.s_addr 	= inet_addr(nsv_dst_ip);
	nsv_daddr.sin_port 			= htons(8031);
	sendto(nsv_sock, msg, msglen, 0, (struct sockaddr *) &nsv_daddr,sizeof(nsv_daddr));

	LOG_MSG(INFO_LEVEL,"===========================================%s sendto nsv, nsv_sendto_selfPlatfrom \n", node_t== AUV5? ("AUV5") :(node_t== AUV6? "AUV6": "USV1"));

	return 0;
}

int nsv_sendto_auv_or_usv(node_type_t node_t,u8* msg , u32 msglen)
{
	struct sockaddr_in auv_daddr, nsv_daddr;
	char auv_dst_ip[16] ={0};
	char nsv_dst_ip[16] ={0};

	/*
	 * 转发至AUV
	 * */
	auv_daddr.sin_family 		= AF_INET;

	if(node_t == AUV5)
	{
		sprintf(auv_dst_ip,"192.0.%d.53", self_node_id);
		auv_daddr.sin_port 			= htons(8051);
		auv_daddr.sin_addr.s_addr 	= inet_addr(auv_dst_ip);
	}

	else if(node_t == AUV6)
	{
		sprintf(auv_dst_ip,"192.0.%d.63", self_node_id);
		auv_daddr.sin_port 			= htons(8061);
		auv_daddr.sin_addr.s_addr 	= inet_addr(auv_dst_ip);
	}
	else if(node_t == USV1)
	{
//		auv_daddr.sin_port 			= htons(8015);
//		auv_daddr.sin_addr.s_addr 	= inet_addr("192.0.1.3");
		return 1;		//大艇收到$AU时，直接由Routing.c中APP_PK_Rcvd()函数执行msgQ_snd完成发送给PC端。
	}
	else
	{
		LOG_MSG(ERR_LEVEL,"unkown node_type err\n");
		return -1;
	}

	if(msg == NULL || msglen ==0)
		return 0;

	if (sendto(nsv_sock, msg, msglen, 0, (struct sockaddr *) &auv_daddr,sizeof(auv_daddr)) <0)
	{
		LOG_MSG(ERR_LEVEL,"sendto err\n");
		return -1;
	}
	else
		LOG_MSG(INFO_LEVEL,"===========================================nsv_sendto_auv %s \n", node_t== AUV5? ("AUV5") :(node_t== AUV6? "AUV6": "USV1"));

	return 0;
}
