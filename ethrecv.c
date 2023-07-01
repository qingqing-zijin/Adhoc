/**
 * @fuction��	���������߽���ip���ĵ��̣߳�����raw socket���������������б������ݣ�
 * 				ͨ��һ���Ĺ��˲��ԣ������Ϲ��˲��Եı��ļ��뵽���ն��У���roterģ��ʹ�á�
 * @author��  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date��		2015-2-4
 * @version��	v0.0.1
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <sys/ioctl.h>        // macro ioctl is defined
#include <pthread.h>
#include "main.h"
#include "aux_func.h"
#include "queues.h"
#include <netpacket/packet.h> //struct sockaddr_ll
#include <ifaddrs.h>
#include "auv.h"

/*
 * ����buffer����
 */
#define MAX_RECV_BUFF_SIZE 		2048

//define in main.c
extern struct in_addr gwl;
extern struct in_addr gl;
extern struct in_addr glbc; 	//lan broadcast ip
extern struct in_addr gwlbc; 	//wlan broadcast ip
extern in_addr_t self;
extern struct in_addr glnm; 	//lan netmask ip
extern struct in_addr gwlnm; 	//wlan netmask ip
extern long long glInTime, glOutTime;
extern long long gwlInTime, gwlOutTime;

void *lan_recv_routine(void * args);
void print_data(const char*buffer, int len);
void print_mac(const char *buffer);
void print_ip(const struct ip * iphdr);
void print_udp(const struct udphdr* udphdr);
int lan_recv_filter(const struct ip *ipheader, const char *raw_frame);
int insert_into_lan_recv_queue(int qid, char* data, int len);

int create_lan_recv_thread() {
	int ret;
	pthread_t tid;

	ret = pthread_create(&tid, NULL, &lan_recv_routine, NULL);
	if (ret != 0) {
		LOG_MSG(ERR_LEVEL, "create lan recv thread\n");
	}
	//LOG_MSG(INFO_LEVEL,"Create lan recv thread success\n");
	return tid;
}

int udp_port_fliter(int destport) {
	//�ض��˿ڹ���
	switch (destport)
	{
	case 53:		//53�˿�ΪDNS�����������ţ���Ҫ��������������DNS������NTϵͳ��ʹ����Ϊ�㷺��
	case 80:	//ΪHTTP ������
	case 137: //netbios
	case 138: //brower
	case 433:
	case 445: //netbios session server
	case 5355:	// LLMNR �ಥ��ѯ������
	case 5353:	// MDNS �ಥ��������
	case 8080:
	case 8000:
	case 11711:
	case 3478:
	case 15712:
		return -1;
	default:
		{break;}
	}
	return 1;
}
int tcp_port_fliter(int destport)
{
	switch (destport)
	{
	//�ض��˿ڹ���
	case 53:
		//53�˿�ΪDNS�����������ţ���Ҫ��������������DNS������NTϵͳ��ʹ����Ϊ�㷺��
	case 80:
		//ΪHTTP ������
	case 137:
		//netbios
	case 138:
		//brower
	case 433:
	case 445:
		//netbios session server
	case 5355:
		// LLMNR �ಥ��ѯ������
	case 5353:
		// MDNS �ಥ��������
	case 8080:
		return -1;
	default: break;
	}

	return 1;
}

/*
 * ���˲���Ҫ��raw sock����.
 * ipheader ,udpheader
 * �������Ҫ������-1
 * ���򷵻�1;
 */
int lan_recv_filter(const struct ip *ipheader, const char *raw_frame) {
	int ret = -1;
	int destport;
	struct udphdr udphdr;
	char *tranhead;
	//LOG_MSG(INFO_LEVEL,"version = %d,",version);

	if (ipheader->ip_v != 4) //��v4�Ĳ����� version
	{
		return -1;
	}

	// LAN �ڽ��յ���PC���İ�  ԴIP��������������������ַ���ڵ���3
	/*
	 gwl = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_addr)->sin_addr;
	 gl = (struct in_addr)((struct sockaddr_in*)(&g_lanif)->ifa_addr)->sin_addr;
	 self = inet_addr("127.0.0.1");
	 glbc = (struct in_addr)((struct sockaddr_in*)(&g_lanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
	 gwlbc =(struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;

	 gwlnm = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_netmask)->sin_addr;
	 glnm = (struct in_addr)((struct sockaddr_in*)(&g_lanif)->ifa_netmask)->sin_addr;

	 */
	/*step1>
	 * �ж��յ���IP����Դ��ַ�Ƿ�Ϊ�������ڣ�ͨ�����������뱨��IP��ַ����&����
	 * �����ж�ԴIP��ַ�����1���ֽ��Ƿ�>=3
	 * ����ǣ���IPԴ��ַΪ�����������ڵĻ�����ַ���ҵ�ַ��Ч�����һ���ж�step2����
	*/
	if ((glnm.s_addr & ipheader->ip_src.s_addr) == (glnm.s_addr & gl.s_addr)
			&& (ipheader->ip_src.s_addr >> 24 & 0xff) >= 3)
	{
		/*
		 * step2>
		 * �ж�IP���ĵ�Դ��ַ�Ƿ�Ϊ����g_wlan��ַ��������������-1
		 * �ж�IP���ĵ�Դ��ַ�Ƿ�Ϊ����g_lan��ַ��������������-1
		 * �ж�IP���ĵ�Դ��ַ�Ƿ�Ϊ����127.0.0.1��ַ��������������-1
		 * �ж�IP���ĵ�Ŀ�ĵ�ַ�Ƿ�Ϊ����127.0.0.1��ַ��������������-1
		 * �ж�IP���ĵ�Ŀ�ĵ�ַ�Ƿ�Ϊ����eth0���������ڣ���ͬһ��������������������-1
		 * �ж�IP���ĵ�Ŀ�ĵ�ַ�Ƿ�Ϊ�Ƿ�Ϊ10.x.y.z,����(3<z<244)��Wlan��������Ϊ255.0.0.0
		 * */
#ifndef UART_Wireless_INTERFACE

		if (gwl.s_addr == ipheader->ip_src.s_addr
				|| gl.s_addr == ipheader->ip_src.s_addr
				|| self == ipheader->ip_src.s_addr
				|| self == ipheader->ip_dst.s_addr
				|| (glnm.s_addr & ipheader->ip_dst.s_addr)	//eth0������
						== (glnm.s_addr & gl.s_addr)
				|| ((gwlnm.s_addr & ipheader->ip_dst.s_addr) < 224
						&& (gwlnm.s_addr & ipheader->ip_dst.s_addr) != 192))
		{
			//LOG_MSG(DBG_LEVEL," discard raw pk: proto = %d, srcIP = 0x%08x, destIP = 0x%08x, data[0]= %d\n",ipheader->ip_p,ipheader->ip_src.s_addr,ipheader->ip_dst.s_addr,*(raw_frame + ipheader->ip_hl * 4));

			return -1;
		}

#else
//		LOG_MSG(DBG_LEVEL," glnm.s_addr & ipheader->ip_dst.s_addr = 0x%08x \n",glnm.s_addr & ipheader->ip_dst.s_addr);

		/*����*/
		if (gl.s_addr == ipheader->ip_src.s_addr		/*ͨ���������Լ����͵�IP���ģ�����*/
					|| self == ipheader->ip_src.s_addr
					|| self == ipheader->ip_dst.s_addr
					//�����ڹ㲥����Ե�����ݰ��ģ�����
					|| ((glnm.s_addr & ipheader->ip_dst.s_addr)	== (glnm.s_addr & gl.s_addr))//Ŀ�ĵ�ַ��eth0������
					//Ŀ�ĵ�ַ�Ƿ�Ϊ192.0.y.(3~243)��ʽ
					|| (((glnm.s_addr & ipheader->ip_dst.s_addr) & 0xff) < 224 && ((glnm.s_addr & ipheader->ip_dst.s_addr)& 0xff) != 192))
			{
//				LOG_MSG(DBG_LEVEL," discard raw pk: proto = %d, srcIP = 0x%08x, destIP = 0x%08x, data[0]= %d\n",ipheader->ip_p,ipheader->ip_src.s_addr,ipheader->ip_dst.s_addr,*(raw_frame + ipheader->ip_hl * 4));

				return -1;
			}



#endif

		switch (ipheader->ip_p) //protocol
		{
		case IPPROTO_ICMP:
			//LOG_MSG(INFO_LEVEL," proto = ICMP\n");
			ret = 1;
			break;
		case IPPROTO_UDP:
			tranhead = raw_frame + ipheader->ip_hl * 4; //to the udp or tcp head;
			memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
			destport = ntohs(udphdr.dest);
			ret = udp_port_fliter(destport);
			//LOG_MSG(INFO_LEVEL," proto = UDP,destpot = %d\n",destport);
			break;
		case IPPROTO_TCP:
			tranhead = raw_frame + ipheader->ip_hl * 4; //to the udp or tcp head;
			memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port
			destport = ntohs(udphdr.dest);
			//hexprint(tranhead,sizeof(udphdr));
			//hexprint((char*)&udphdr,sizeof(udphdr));
			ret = tcp_port_fliter(destport);
			//LOG_MSG(INFO_LEVEL," proto = TCP,destpot = %d\n",destport);
			return -1;		//��Թ�������Ŀ���ݶ���֧��TCP�ӿ�
			break;
		case IPPROTO_IGMP:
		case IPPROTO_IPIP:
		case IPPROTO_RAW:
			return -1;
		default: {
			return -1;
			break;
		}
		}
		return ret;
	}

	/*
	 * step3> ��ԴIP��ַΪ������IP��ַ����WIFI�ڵ�ַ
	 * */
	// ���������İ�  ԴIP��LAN��IP��WIFI��IP��

#ifndef UART_Wireless_INTERFACE
	else if (gwl.s_addr == ipheader->ip_src.s_addr
			|| gl.s_addr == ipheader->ip_src.s_addr)
#else
	else if (gl.s_addr == ipheader->ip_src.s_addr)
#endif
	{
		// Ŀ��IP�������ڵ�����������ҷǹ㲥�����Ҳ���dst_port== 20150��UDP�����򽻸�route_Serv ��װ��APP���͡�

		if ((ipheader->ip_dst.s_addr & 0xff) == 192
				&& (ipheader->ip_dst.s_addr >> 8 & 0xffff) != 0xffff
				&& (ipheader->ip_dst.s_addr >> 24 & 0xff) >= 3
				&& (glnm.s_addr & ipheader->ip_dst.s_addr)
						!= (glnm.s_addr & gl.s_addr)) {
			if (ipheader->ip_p == IPPROTO_UDP) {
				tranhead = raw_frame + ipheader->ip_hl * 4; //to the udp or tcp head;
				memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port
				destport = ntohs(udphdr.dest);
				if (destport != WLAN_LISTEN_PORT) {

					LOG_MSG(DBG_LEVEL,
							"resend local UDP pk (proto=%d, len=%d) from %08x to %08x\n",
							ipheader->ip_p, ntohs(ipheader->ip_len),
							ipheader->ip_src.s_addr, ipheader->ip_dst.s_addr);
					print_ip(ipheader);

					return 1;
				} else {

					return -1;
				}
			} else if (ipheader->ip_p == IPPROTO_TCP) {

				LOG_MSG(DBG_LEVEL,
						"resend local TCP pk (proto=%d, len=%d)  from %08x to %08x\n",
						ipheader->ip_p, ntohs(ipheader->ip_len),
						ipheader->ip_src.s_addr, ipheader->ip_dst.s_addr);
				print_ip(ipheader);
				return 1;
			} else if (ipheader->ip_p == IPPROTO_ICMP) {
				LOG_MSG(DBG_LEVEL,
						"resend local ICMP pk (proto=%d, len=%d)  from %08x to %08x\n",
						ipheader->ip_p, ntohs(ipheader->ip_len),
						ipheader->ip_src.s_addr, ipheader->ip_dst.s_addr);
				print_ip(ipheader);
				return 1;
			} else {
				return -1;
			}
		}

		return -1;

	}
	// wifi �ڷ����İ�
	else {

		return -1;
	}
}

 /* *************************************************************************************************
 * ��TCP/UDPЭ���У��涨IP Header�������£�
 * 4-bit version | 4-bit header length | 8bit typeof service | 16bit total length in bytes          |
 * 16-bit identification			   					     | 3-bit flags  | 13-bit fragment offset|
 * 8-bit time to live				   | 8-bit protocol		 |    16-bit header checksum			|
 * 32-bit source IP address																			|
 * 32-bit destination IP address																	|
 * options(if any)																					|
 * data	��UDPͷ+ AppData��
 *
 * *IPͷ��4-bit header length�ֶΣ������£�������IPͷ���ٸ�4���ֽڣ�����Э���е�option(if any)�ֶΡ�
 * 																						|
 * **************************************************************************************************
 * ��TCP/UDPЭ���У��涨UDP HeaderЭ�����£�
 *
 * 16-bit source port number    | 16-bit destination port number |
 * 16-bit UDP length			| 16-bit UDP checksum			 |
 * data															 |
 * ����UDP length ������8�ֽ�UDPͷ+ UDP data�ֽ�
 * ***************************************************************
 * */
/**
 * ����RAW_SOCKET,�ػ������ϵ���IP���ģ����˵�����Ҫ�ı��ģ�Ȼ��ת�����ն��У���route����
 */


void *lan_recv_routine(void * args)
{
	int msgqid;
	int sock, n_read, ip_len;
	char buffer[MAX_RECV_BUFF_SIZE];
	char *ethhead, *iphead, *tranhead, *raw_frame;
	struct sockaddr_ll sll;
	struct ip iphdr;

	struct ifreq ifr;

	//����RAW�׽��֣�����UDP TCP ICMP���ݰ�

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) //
	{
		LOG_MSG(ERR_LEVEL, "lan create recv raw socket ");
		exit(0);
	}

	//LOG_MSG(INFO_LEVEL,"lan create recv raw socket! sockfd = %d\n",sock);
	//LOG_MSG(INFO_LEVEL,"lan create raw sock success, start listening....\n");

	msgqid = msgget(RECV_CQ, IPC_EXCL);
	if (msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL, "get lan RECV cq");
		pthread_exit(1);
	}
	LOG_MSG(INFO_LEVEL, "lan get route_cq_id = %d\n", msgqid);

	/*
	 * ������eth0
	 * */
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", LAN_INTERFACE);
	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
		LOG_MSG(ERR_LEVEL, "lan ioctl() failed to find interface ");
		return (-1);
	}
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = PF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htonl(ETH_P_IP);
	//�ο�libpcapʵ��
	if (bind(sock, (struct sockaddr *) &sll, sizeof(sll)) < 0) {
		LOG_MSG(ERR_LEVEL, "lan recv bind");
		pthread_exit(1);
	}
	/**/
	while (1)
	{
		//recvfrom����Ĭ��Ϊ������ʽ
		n_read = recvfrom(sock, buffer, MAX_RECV_BUFF_SIZE, 0, NULL, NULL);
		/*
		 14   6(dest)+6(source)+2(type or length)
		 +
		 20   ip header
		 +
		 8   icmp,tcp or udp header
		 = 42
		 */
		if (n_read < 42) {

			LOG_MSG(ERR_LEVEL, "LAN sockRcv %d bytes\n", n_read);
			continue;
		}

		//printf("--------------recv from PC port %d\n",);
		//continue;
		ethhead = buffer;

		//��̫��802.2/802.3��װethhead[12:13]Ϊ����
		//��̫��RFC894��װethhead[12:13]��type
		//typeȡֵ��0800����IP Datagram�� 0806����ARP Request/Reply�� 8035���� RARP request/reply
		if (ethhead[12] != 0x08 || ethhead[13] != 0x00) {

			if (ethhead[12] == 0x08 && ethhead[13] == 0x06) {
				if (ethhead[14 + 7] == 1)  //request
				{
//					LOG_MSG(DBG_LEVEL,"ARP request: tell %d.%d.%d.%d where is %d.%d.%d.%d?\n",
//							ethhead[14 + 14], ethhead[14 + 15],
//							ethhead[14 + 16], ethhead[14 + 17],
//							ethhead[14 + 24], ethhead[14 + 25],
//							ethhead[14 + 26], ethhead[14 + 27]);
				}
				else if (ethhead[14 + 7] == 2) // reply
				{
					LOG_MSG(DBG_LEVEL,"ARP reply: %d.%d.%d.%d tell %d.%d.%d.%d, I am here!\n",
							ethhead[14 + 14], ethhead[14 + 15],
							ethhead[14 + 16], ethhead[14 + 17],
							ethhead[14 + 24], ethhead[14 + 25],
							ethhead[14 + 26], ethhead[14 + 27]);
				}
				else
				{
					LOG_MSG(DBG_LEVEL, "arp type = %d\n", ethhead[14 + 7]);
				}

			} else {
				LOG_MSG(DBG_LEVEL, "not IP %2x %2x\n", ethhead[12],ethhead[13]);
			}

			continue;
		}

		iphead = ethhead + 14; //ethnet header is 14B
		/*
		 * ���IP����
		 * */
		raw_frame = ethhead + 14;

		//����IPͷ
		memcpy(&iphdr, iphead, sizeof(iphdr));
		//����ntohs�����IP���ݱ�����
		ip_len = ntohs(iphdr.ip_len); // get the real len from ip header

		//����
		if (lan_recv_filter(&iphdr, raw_frame) < 0) {
			//LOG_MSG(INFO_LEVEL,"raw sock recv ip src = %s,is from myself,ignore it\n",inet_ntoa(iphdr.ip_src));
			continue;
		}
		/*
		 * ��ȡUDPĿ�Ķ˿���Ϣ
		 * */
		struct udphdr udphdr;
		char 	*tranhead;
		int 	destport;
		int		srcPort;
		int  	user_data_len =0;
		u8*		user_data = NULL;

		/*
		 * ����ͨ����ˮ��ģ�����ͨ��ʱ����ͨ������RS232�ӿڽ���ֱ��ͨ�ŵġ�����ͧ��ˮ�º���������ͨ��ʱ��ˮ������ͧ��Ҫ����ת����������ʵ��
		 * $USV -> $AUV��ת�������ǣ�
		 * 1> ��ͧ�ܿ�ƽ̨��192.0.1.3�������ݴ���ã�ͨ������UDP�ӿ�sendto()�����������ݰ�����sendto()������ָ��ĳ���м̽ڵ�IP��ַ+�˿���Ϊ
		 * Ŀ�ĵ�ַ��Ŀ�Ķ˿ڣ�8015����$USV -> $AUV�����ݰ�����
		 * 2> Сͧ�յ����Դ�ͧ�����ݰ�ʱ����ͨ�����س������ж�Ŀ�Ķ˿��Ƿ�Ϊ8015��8016,�����8015�˿ڣ����ʾ���յ�$USV-> $AUV5���ݰ���
		 * �����8016�˿ڣ����ʾ���յ�$USV-> $AUV6���ݰ���
		 * 3> �յ��������ݰ��󣬽ػ����ݶ����ݡ�������ˮ��ģ��ͨ�Žӿڣ�RS232 ����������Э�����/ת��ģ�飬�����÷��ͺ�����ˮ��
		 *
		 * PC�˷��͡�123��ʱ����ʾ����� head_len =5(��ӦIPͷΪ20�ֽ�), total_bytes =31��IPͷ+ TCP��20�ֽڣ�/UDPͷ(8�ֽ�) + APP_DATA��,   ��
		 *	31 = 20(IPͷ) + 8��UDPͷ��+ 3��AppData��
		 * WJW��tranheadָ�����UDP head���յ�ַ
		 */
		tranhead = raw_frame + iphdr.ip_hl * 4; //to the udp or tcp head;
		memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
		destport = ntohs(udphdr.dest);
		srcPort	= ntohs(udphdr.source);
/**/
		user_data_len = ntohs(udphdr.len)-8;
		user_data = tranhead +8;


		/*
		 * ��PC���ܿ�ƽ̨������$UA���ݰ�������ͧ������ˮ�µ����ݰ���ֱ��ͨ�����̲�
		 * ͨ�ŷ�����NSV��Ȼͧ����ת���ʹ���
		 * ���ͨ��ģ����ethrecv.c�������$UA���ݰ�����ͨ�ع㲥��������ͨ�����̲�·��ת����
		 * ���� $USV ----------> ������ͨ��-----> ĳ��NSV�ڵ����$UA����ethsend.c�д���$UA
		 * */
/*
  	  	char* dst_auv_id = find_n_comma(user_data, 4);
 		if(user_data[0] == '$' &&  user_data[1] == 'U' &&  user_data[2] == 'A')
		{
			//$UA,93,1,2,6,
//			if(destport == AUV5_LISTEN_PORT)
			if(*(dst_auv_id+1) == '5')
			{
				//����USV��AUV5ˮ�º�����
				nsv_sendto_auv_usv(AUV5,user_data, user_data_len);
			}
//			else if(destport == AUV6_LISTEN_PORT)
			else if(*(dst_auv_id+1) == '6')
			{
				//����USV��AUV6ˮ�º�����
				nsv_sendto_auv_usv(AUV6, user_data, user_data_len);
			}
			else
			{

				LOG_MSG(ERR_LEVEL, "unkown pkt $UA to AUV%c,\n", *(dst_auv_id+1));
			}
		}
*/
		/*
		 * ��ˮ�º�����AUV����$AU���ݰ�����ͧʱ��ԴIPΪAUV��IP��ַ��192.0.3.53����Ŀ�ĵ�ַΪ��ͧ��IP��ַ��192.0.1.3��;
		 * ��ˣ�����$AU���ݰ�ʱ��ͨ��ĳ��NSV��ͨ������������ת����
		 * ����NVSƽ̨������ģ�顢̽��ģ�����Ҫ���$AU���ݰ��������NSVͨ�ش���$AU�������ǣ�
		 * 1> ���յ���$AU���͸�NSV����ƽ̨;
		 * 2> ���յ���$AUͨ�����߷��͸���ͧ��
		 * */

		/*
		 * 20180605 Ϊ�����ϵͳ��ͨ����ʱ����һ�£��ݶ�$AU����NSVƽ̨��ƽ̨�յ����Ե�ת�����󴬣�ͬʱ�㲥��̽��ģ�������
		 * */
//		if(user_data[0] == '$' &&  user_data[1] == 'A' &&  user_data[2] == 'U' )
//		{
//			//ת��AUV $AU������ƽ̨
//			//$AU,118,5,2,1
//			char* src_auv_id = find_n_comma(user_data, 2);
//			src_auv_id++;
//			/*
//			 * AUV->NSV
//			 * NSV->����ƽ̨
//			 * */
//			nsv_sendto_selfPlatfrom(*src_auv_id, user_data, user_data_len);
//		}

		//���߷����������ڵ㣬Ʃ��USA#1/ NSV#2/ NSV#3/ NSV#4
		insert_into_lan_recv_queue(msgqid, raw_frame, ip_len);

		char ip_src[16], ip_dst[16];

		char user_pk_hd[4]={0,};
		user_pk_hd[0] = user_data[0];
		user_pk_hd[1] = user_data[1];
		user_pk_hd[2] = user_data[2];
		//����inet_ntop�������ֽڵ�ַת��Ϊ�����ֽ�
		inet_ntop(AF_INET, &(iphdr.ip_src), ip_src, INET_ADDRSTRLEN);
		inet_ntop(AF_INET, &(iphdr.ip_dst), ip_dst, INET_ADDRSTRLEN);

		LOG_MSG(INFO_LEVEL,	"--------------------------------------LAN Recv %s ,srcIP %s: %d , destIP %s: %d, IP ,IP_head_len %d, total_bytes %d, user_data bytes %d ,proto = %s\n",
				user_pk_hd,\
				ip_src,\
				srcPort,\
				ip_dst,\
				destport,\
				iphdr.ip_hl ,\
				ip_len, \
				user_data_len , \
				iphdr.ip_p==IPPROTO_TCP? "TCP" :(iphdr.ip_p==IPPROTO_UDP? "UDP": "Other PPROTO"));

	/*	LOG_MSG(INFO_LEVEL,	"============================================LAN Recv len = %d,proto = %d\n",
				ip_len, iphdr.ip_p);*/


	}
}

/*
 * data is recv by raw socket,include ipheader and udpheader,and applicaiton data
 * len is the data's length
 */
int insert_into_lan_recv_queue(int qid, char* data, int len) {
	int ret;
	struct q_item titem;
	struct msqid_ds ds;
	titem.type = LAN_DATA;
	memcpy(&(titem.data), data, len);
	ret = msgsnd(qid, &titem, len, IPC_NOWAIT);
	if (ret < 0) {
		if (0 == msgctl(qid, IPC_STAT, &ds)) {
			LOG_MSG(INFO_LEVEL, "current size = %ld,max size = %ld\n",ds.__msg_cbytes, ds.msg_qbytes);
		}
		LOG_MSG(ERR_LEVEL, "lan recv send queue");
	}
	return ret;
}

void print_data(const char* data, int len) {
	int i = 0;
	while (i++ < len) {
		printf("%.2X ", ((*data++) & 0xff));
		if (i % 16 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}

void print_mac(const char* p) {

	int n = 0XFF;
	LOG_MSG(DBG_LEVEL, "MAC: %.2X:%02X:%02X:%02X:%02X:%02X==>"
			"%.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n\n", p[6] & n, p[7] & n, p[8] & n,
			p[9] & n, p[10] & n, p[11] & n, p[0] & n, p[1] & n, p[2] & n,
			p[3] & n, p[4] & n, p[5] & n);

}

void print_ip(const struct ip *iphdr) {
	LOG_MSG(DBG_LEVEL, " from: %d.%d.%d.%d,to: %d.%d.%d.%d\n",
			iphdr->ip_src.s_addr & 0xff, iphdr->ip_src.s_addr >> 8 & 0xff,
			iphdr->ip_src.s_addr >> 16 & 0xff,
			iphdr->ip_src.s_addr >> 24 & 0xff, iphdr->ip_dst.s_addr & 0xff,
			iphdr->ip_dst.s_addr >> 8 & 0xff, iphdr->ip_dst.s_addr >> 16 & 0xff,
			iphdr->ip_dst.s_addr >> 24 & 0xff);

}

void print_udp(const struct udphdr *udphdr) {
	LOG_MSG(INFO_LEVEL, "source port: %u,", ntohs(udphdr->source));
	LOG_MSG(INFO_LEVEL, "dest port: %u\n", ntohs(udphdr->dest));
	LOG_MSG(INFO_LEVEL, "\n");
}

