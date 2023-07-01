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
extern List*	oneHopNb_List;		// һ���ھ�����Ԫ����ONE_HOP_NB_ITEM
extern List*	mHopNb_List;		// �����ھ�����Ԫ����MULTI_HOP_NB_ITEM
extern List*	IERP_List;			// ���·������Ԫ����IERP_ITEM

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
	 * STEP1> �����ڵ�����1���ھ�״̬���͸�USV����������1���ڵ��ID
	 * */
	//����ͷ
	msg[0] = '$';
	msg[1] = 'G';
	msg[2] = 'U';
	msg[3] = ',';

	//���ĳ���
	msg[4] = 0;// ����ǰ���޸�
	msg[5] = ',';

	//ָ��Դ
	msg[6] = (self_node_id&0xff)+0x30;
	msg[7] = ',';

	//Ŀ��id
	msg[8] = '1';
	msg[9] = ',';

	msglen = 10;
	/*
	 * һ���ھ��б�ֻ��˫���ھ�id
	 * ���ݸ�ʽ�� 2/3/4,����ڵ�2,3,4��Ϊ1���ڵ�
	 *
	 * ��������ʱ���ȸ���ָ��Դȷ���ڵ�id��Ȼ�������ȡ1���ھ�id, �ٸ��ݽ�ɫΪ�м̽ڵ��1���ڵ�id����ȡ���������ڵ�id��
	 *
	 * */
	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{
		if(element_ptr1->direction == 1)	// ֻ��˫���ھ�
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
	 * �����ھ��б�ֻ��Ŀ��id�����м̽ڵ�id��
	 * ���ݸ�ʽ��2-3/1-4, ��ʾ���ڵ���3��4�ֱ𾭹��м�2��1
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
	 * ���·�ɣ���Ϊ3�����Ͻڵ㣬������Ŀ��id�����м̽ڵ�id��
	 * ���ݸ�ʽ��2-3-1-4; ��ʾ���ڵ�ڵ�Ϊ2,���ڵ�4,�ֱ𾭹��м�3��1
	 * 2-1-4-5; ��ʾ���ڵ�ڵ�Ϊ2���ڵ�5,�ֱ𾭹��м�1��4
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
	msg[msglen++] = '*';			//��������	```

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
	 * ��ȡUDPĿ�Ķ˿���Ϣ
	 * */
	struct udphdr udphdr;
	char 	*tranhead;
	int 	destport;
	int  	user_data_len =0;
	u8*		user_data = NULL;
	struct ip iphdr;

	//����IPͷ
	memcpy(&iphdr, IP_pkptr, sizeof(iphdr));
	/*
	 * ����ͨ����ˮ��ģ�����ͨ��ʱ����ͨ������RS232�ӿڽ���ֱ��ͨ�ŵġ�����ͧ��ˮ�º���������ͨ��ʱ��ˮ������ͧ��Ҫ����ת����������ʵ��
	 * $USV -> $AUV��ת�������ǣ�
	 * 1> ��ͧ�ܿ�ƽ̨��192.0.1.3�������ݴ���ã�ͨ������UDP�ӿ�sendto()�����������ݰ���
	 * ��sendto()������ָ��ĳ��NSV�ڵ�IP��ַ+�˿���ΪĿ�ĵ�ַ��Ŀ�Ķ˿ڣ�8015����$USV -> $AUV�����ݰ�����
	 *
	 * 2> Сͧ�յ����Դ�ͧ�����ݰ�ʱ����ͨ�����س������ж��Ƿ�Ϊ$UA���ݰ�����������ʾ���յ�$UA���ݰ�,ת��3������
	 *
	 * 3> NSV�յ�$UA���ݰ���һ�������ˮ��ģ��ͨ�Žӿڣ�RS232 ����������Э�����/ת��ģ�飬ת������Ӧ��AUVƽ̨;
	 * 	  ��һ������Routing.c��APP_PK_Rcvd()msgQ_snd���͸�NSV����ƽ̨��
	 *
	 * $AUV-> $USV�Ĳ����ǣ�
	 * 1> �ο�ethrecv.c��NSVת��PC�˿��յ���$AU���ݰ���
	 * 2> ��ͧ�������յ�$AU���ݰ�ʱ����APP_pk_Rcvd()����PC�նˡ�
	 * 3> ��$AU���ݰ�����ĳ���ڵ�ת��ʱ������IP�����ݱ���ת����������Ӧ�ò㴦��
	 *
	 * PC�˷��͡�123��ʱ����ʾ����� head_len =5(��ӦIPͷΪ20�ֽ�), total_bytes =31��IPͷ+ TCP��20�ֽڣ�/UDPͷ(8�ֽ�) + APP_DATA��,   ��
	 *	31 = 20(IPͷ) + 8��UDPͷ��+ 3��AppData��
	 * WJW��tranheadָ�����UDP head���յ�ַ
	 */
	tranhead = IP_pkptr + iphdr.ip_hl * 4; //to the udp or tcp head;
	memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
	destport = ntohs(udphdr.dest);

	user_data_len = ntohs(udphdr.len)-8;
	user_data = tranhead +8;

	char* dst_auv_id = find_n_comma(user_data, 4);

	if(user_data[0] == '$' &&  user_data[1] == 'U' &&  user_data[2] == 'A')
	{
		//ת��$UA����Ӧ��AUVƽ̨; ת��NSV����ƽ̨��Routing.c��msgQ_snd���
		if(*(dst_auv_id+1) == '5')
		{
			//����USV��AUV5ˮ�º�����
			nsv_sendto_auv_or_usv(AUV5,user_data, user_data_len);
		}
		else if(*(dst_auv_id+1) == '6')
		{
			//����USV��AUV6ˮ�º�����
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
 * ��ʼ��NSV�ڵ�ͨ�Žӿڣ������׽��֣�Ϊת��$AU, $UA���ݰ�׼��
 * */
int nsv_network_init()
{
	struct sockaddr_in BindAddrto;

	char nsv_dst_ip[16] ={0};

	int nsv_listen_port = 8031;

	/*
	 * ת����USV��NSV
	 * */

	sprintf(nsv_dst_ip,"192.0.%d.3", self_node_id);


	bzero(&BindAddrto, sizeof(struct sockaddr_in));

//	���ü�������IP��ַ+�˿�
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
 * @nsvת��auv $AU�������ڲ�����ƽ̨
 * */
int nsv_sendto_selfPlatfrom(node_type_t node_t,u8* msg , u32 msglen)
{
	struct sockaddr_in auv_daddr, nsv_daddr;
	char auv_dst_ip[16] ={0};
	char nsv_dst_ip[16] ={0};
	/*
	 * $UA,��$AU����Ե�ת����NSV����NSV�ڲ��㲥
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
	 * ת����AUV
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
		return 1;		//��ͧ�յ�$AUʱ��ֱ����Routing.c��APP_PK_Rcvd()����ִ��msgQ_snd��ɷ��͸�PC�ˡ�
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
