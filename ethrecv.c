/**
 * @fuction：	创建从有线接收ip报文的线程，创建raw socket，接收网络层的所有报文数据，
 * 				通过一定的过滤策略，将符合过滤策略的报文加入到接收队列，供roter模块使用。
 * @author：  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date：		2015-2-4
 * @version：	v0.0.1
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
 * 接收buffer长度
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
	//特定端口过滤
	switch (destport)
	{
	case 53:		//53端口为DNS服务器所开放，主要用于域名解析，DNS服务在NT系统中使用最为广泛。
	case 80:	//为HTTP 开发的
	case 137: //netbios
	case 138: //brower
	case 433:
	case 445: //netbios session server
	case 5355:	// LLMNR 多播查询主机名
	case 5353:	// MDNS 多播域名解析
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
	//特定端口过滤
	case 53:
		//53端口为DNS服务器所开放，主要用于域名解析，DNS服务在NT系统中使用最为广泛。
	case 80:
		//为HTTP 开发的
	case 137:
		//netbios
	case 138:
		//brower
	case 433:
	case 445:
		//netbios session server
	case 5355:
		// LLMNR 多播查询主机名
	case 5353:
		// MDNS 多播域名解析
	case 8080:
		return -1;
	default: break;
	}

	return 1;
}

/*
 * 过滤不需要的raw sock数据.
 * ipheader ,udpheader
 * 如果不需要处理返回-1
 * 否则返回1;
 */
int lan_recv_filter(const struct ip *ipheader, const char *raw_frame) {
	int ret = -1;
	int destport;
	struct udphdr udphdr;
	char *tranhead;
	//LOG_MSG(INFO_LEVEL,"version = %d,",version);

	if (ipheader->ip_v != 4) //非v4的不处理 version
	{
		return -1;
	}

	// LAN 口接收到的PC机的包  源IP在有线网段内且主机地址大于等于3
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
	 * 判断收到的IP报文源地址是否为本网段内，通过子网掩码与报文IP地址进行&操作
	 * 并且判断源IP地址的最后1个字节是否>=3
	 * 如果是，即IP源地址为本网关网段内的机器地址，且地址有效，则进一步判断step2条件
	*/
	if ((glnm.s_addr & ipheader->ip_src.s_addr) == (glnm.s_addr & gl.s_addr)
			&& (ipheader->ip_src.s_addr >> 24 & 0xff) >= 3)
	{
		/*
		 * step2>
		 * 判断IP报文的源地址是否为本机g_wlan地址，是则丢弃，返回-1
		 * 判断IP报文的源地址是否为本机g_lan地址，是则丢弃，返回-1
		 * 判断IP报文的源地址是否为本机127.0.0.1地址，是则丢弃，返回-1
		 * 判断IP报文的目的地址是否为本机127.0.0.1地址，是则丢弃，返回-1
		 * 判断IP报文的目的地址是否为本机eth0网卡网段内，即同一个子网，是则丢弃，返回-1
		 * 判断IP报文的目的地址是否为是否为10.x.y.z,其中(3<z<244)，Wlan子网掩码为255.0.0.0
		 * */
#ifndef UART_Wireless_INTERFACE

		if (gwl.s_addr == ipheader->ip_src.s_addr
				|| gl.s_addr == ipheader->ip_src.s_addr
				|| self == ipheader->ip_src.s_addr
				|| self == ipheader->ip_dst.s_addr
				|| (glnm.s_addr & ipheader->ip_dst.s_addr)	//eth0网段内
						== (glnm.s_addr & gl.s_addr)
				|| ((gwlnm.s_addr & ipheader->ip_dst.s_addr) < 224
						&& (gwlnm.s_addr & ipheader->ip_dst.s_addr) != 192))
		{
			//LOG_MSG(DBG_LEVEL," discard raw pk: proto = %d, srcIP = 0x%08x, destIP = 0x%08x, data[0]= %d\n",ipheader->ip_p,ipheader->ip_src.s_addr,ipheader->ip_dst.s_addr,*(raw_frame + ipheader->ip_hl * 4));

			return -1;
		}

#else
//		LOG_MSG(DBG_LEVEL," glnm.s_addr & ipheader->ip_dst.s_addr = 0x%08x \n",glnm.s_addr & ipheader->ip_dst.s_addr);

		/*网关*/
		if (gl.s_addr == ipheader->ip_src.s_addr		/*通控器网关自己发送的IP报文，则丢弃*/
					|| self == ipheader->ip_src.s_addr
					|| self == ipheader->ip_dst.s_addr
					//网段内广播、点对点的数据包文，丢弃
					|| ((glnm.s_addr & ipheader->ip_dst.s_addr)	== (glnm.s_addr & gl.s_addr))//目的地址在eth0网段内
					//目的地址是否为192.0.y.(3~243)格式
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
			return -1;		//针对哈工程项目，暂定不支持TCP接口
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
	 * step3> 若源IP地址为本网关IP地址，或WIFI口地址
	 * */
	// 本机发来的包  源IP是LAN口IP或WIFI口IP。

#ifndef UART_Wireless_INTERFACE
	else if (gwl.s_addr == ipheader->ip_src.s_addr
			|| gl.s_addr == ipheader->ip_src.s_addr)
#else
	else if (gl.s_addr == ipheader->ip_src.s_addr)
#endif
	{
		// 目的IP是其它节点的有线网，且非广播包，且不是dst_port== 20150的UDP包，则交给route_Serv 封装成APP发送。

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
	// wifi 口发来的包
	else {

		return -1;
	}
}

 /* *************************************************************************************************
 * 在TCP/UDP协议中，规定IP Header定义如下：
 * 4-bit version | 4-bit header length | 8bit typeof service | 16bit total length in bytes          |
 * 16-bit identification			   					     | 3-bit flags  | 13-bit fragment offset|
 * 8-bit time to live				   | 8-bit protocol		 |    16-bit header checksum			|
 * 32-bit source IP address																			|
 * 32-bit destination IP address																	|
 * options(if any)																					|
 * data	（UDP头+ AppData）
 *
 * *IP头中4-bit header length字段（见如下）代表了IP头多少个4个字节，包括协议中的option(if any)字段。
 * 																						|
 * **************************************************************************************************
 * 在TCP/UDP协议中，规定UDP Header协议如下：
 *
 * 16-bit source port number    | 16-bit destination port number |
 * 16-bit UDP length			| 16-bit UDP checksum			 |
 * data															 |
 * 其中UDP length 包含了8字节UDP头+ UDP data字节
 * ***************************************************************
 * */
/**
 * 创建RAW_SOCKET,截获网卡上的所IP报文，过滤掉不需要的报文，然后转到接收队列，待route处理。
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

	//创建RAW套接字，接收UDP TCP ICMP数据包

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
	 * 绑定网卡eth0
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
	//参考libpcap实现
	if (bind(sock, (struct sockaddr *) &sll, sizeof(sll)) < 0) {
		LOG_MSG(ERR_LEVEL, "lan recv bind");
		pthread_exit(1);
	}
	/**/
	while (1)
	{
		//recvfrom函数默认为阻塞方式
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

		//以太网802.2/802.3封装ethhead[12:13]为长度
		//以太网RFC894封装ethhead[12:13]是type
		//type取值有0800代表IP Datagram； 0806代表ARP Request/Reply； 8035代表 RARP request/reply
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
		 * 获得IP报文
		 * */
		raw_frame = ethhead + 14;

		//拷贝IP头
		memcpy(&iphdr, iphead, sizeof(iphdr));
		//调用ntohs，获得IP数据报长度
		ip_len = ntohs(iphdr.ip_len); // get the real len from ip header

		//过滤
		if (lan_recv_filter(&iphdr, raw_frame) < 0) {
			//LOG_MSG(INFO_LEVEL,"raw sock recv ip src = %s,is from myself,ignore it\n",inet_ntoa(iphdr.ip_src));
			continue;
		}
		/*
		 * 获取UDP目的端口信息
		 * */
		struct udphdr udphdr;
		char 	*tranhead;
		int 	destport;
		int		srcPort;
		int  	user_data_len =0;
		u8*		user_data = NULL;

		/*
		 * 由于通控与水声模块进行通信时，是通过有线RS232接口进行直接通信的。当大艇与水下航行器进行通信时，水面无人艇需要进行转发处理。开发实现
		 * $USV -> $AUV的转发策略是：
		 * 1> 大艇总控平台（192.0.1.3）将数据打包好，通过调用UDP接口sendto()函数发送数据包，在sendto()函数中指定某个中继节点IP地址+端口作为
		 * 目的地址和目的端口（8015代表$USV -> $AUV的数据包）。
		 * 2> 小艇收到来自大艇的数据包时，在通控网关程序中判断目的端口是否为8015或8016,如果是8015端口，则表示接收到$USV-> $AUV5数据包。
		 * 如果是8016端口，则表示接收到$USV-> $AUV6数据包。
		 * 3> 收到上述数据包后，截获“数据端内容”，调用水声模块通信接口（RS232 或其它）和协议解析/转换模块，最后调用发送函数至水声
		 *
		 * PC端发送“123”时，提示输出： head_len =5(对应IP头为20字节), total_bytes =31（IP头+ TCP（20字节）/UDP头(8字节) + APP_DATA）,   。
		 *	31 = 20(IP头) + 8（UDP头）+ 3（AppData）
		 * WJW：tranhead指向传输层UDP head的收地址
		 */
		tranhead = raw_frame + iphdr.ip_hl * 4; //to the udp or tcp head;
		memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
		destport = ntohs(udphdr.dest);
		srcPort	= ntohs(udphdr.source);
/**/
		user_data_len = ntohs(udphdr.len)-8;
		user_data = tranhead +8;


		/*
		 * 由PC（总控平台）发送$UA数据包，即大艇发送至水下的数据包，直接通过超短波
		 * 通信发送至NSV自然艇进行转发和处理。
		 * 因此通控模块在ethrecv.c中无需对$UA数据包进行通控广播处理，而是通过超短波路由转发。
		 * 即： $USV ----------> 经无线通信-----> 某个NSV节点接收$UA，在ethsend.c中处理$UA
		 * */
/*
  	  	char* dst_auv_id = find_n_comma(user_data, 4);
 		if(user_data[0] == '$' &&  user_data[1] == 'U' &&  user_data[2] == 'A')
		{
			//$UA,93,1,2,6,
//			if(destport == AUV5_LISTEN_PORT)
			if(*(dst_auv_id+1) == '5')
			{
				//发送USV至AUV5水下航行器
				nsv_sendto_auv_usv(AUV5,user_data, user_data_len);
			}
//			else if(destport == AUV6_LISTEN_PORT)
			else if(*(dst_auv_id+1) == '6')
			{
				//发送USV至AUV6水下航行器
				nsv_sendto_auv_usv(AUV6, user_data, user_data_len);
			}
			else
			{

				LOG_MSG(ERR_LEVEL, "unkown pkt $UA to AUV%c,\n", *(dst_auv_id+1));
			}
		}
*/
		/*
		 * 由水下航行器AUV发送$AU数据包至大艇时，源IP为AUV的IP地址（192.0.3.53），目的地址为大艇的IP地址（192.0.1.3）;
		 * 因此，发送$AU数据包时需通过某个NSV的通控器网关无线转发。
		 * 另外NVS平台的智能模块、探测模块均需要获得$AU数据包处理，因此NSV通控处理$AU的流程是：
		 * 1> 将收到的$AU发送给NSV自主平台;
		 * 2> 将收到的$AU通过无线发送给大艇。
		 * */

		/*
		 * 20180605 为了与大系统无通控器时流程一致，暂定$AU发给NSV平台，平台收到后点对点转发给大船，同时广播至探测模块和智能
		 * */
//		if(user_data[0] == '$' &&  user_data[1] == 'A' &&  user_data[2] == 'U' )
//		{
//			//转送AUV $AU给自主平台
//			//$AU,118,5,2,1
//			char* src_auv_id = find_n_comma(user_data, 2);
//			src_auv_id++;
//			/*
//			 * AUV->NSV
//			 * NSV->自主平台
//			 * */
//			nsv_sendto_selfPlatfrom(*src_auv_id, user_data, user_data_len);
//		}

		//无线发送至其它节点，譬如USA#1/ NSV#2/ NSV#3/ NSV#4
		insert_into_lan_recv_queue(msgqid, raw_frame, ip_len);

		char ip_src[16], ip_dst[16];

		char user_pk_hd[4]={0,};
		user_pk_hd[0] = user_data[0];
		user_pk_hd[1] = user_data[1];
		user_pk_hd[2] = user_data[2];
		//调用inet_ntop将网络字节地址转换为主机字节
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

