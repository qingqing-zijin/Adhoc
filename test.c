#include	"Routing.h"
#include	"linklist.h"
#include	"aux_func.h"		// 内含消息队列，定时器，系统时间读取，线程，CRC校验等操作

#include	<time.h>

void log_bin(unsigned int a)
{
	int	i;
	for(i = 0; i< 32; i++)
	{
		putchar((a>>i&1)+0x30);
		if((i&3) == 3)
			putchar(' ');
	}

	putchar('\n');
}


void test_Check_Mask_By_RelayNum(int relay_num)
{
	unsigned int mask;
	unsigned int	cnt = 0;
	time_t	t;
	time(&t);
	printf("time = %d\n",t);
	for(mask = 0; mask <= 0xffff; mask++)
	{
		if (Check_Mask_By_RelayNum(mask, relay_num))
		{
			//printf("%d: ",mask);
			//log_bin(mask);
			cnt++;
			
		}
			
	}
	printf("total cnt: %d\n",cnt);		// 16*15*14/3!
	time(&t);
	printf("time = %d\n",t);
}

void test_Get_New_RelayList_By_Mask(unsigned int mask, int relay_num)
{
	unsigned short old_relay_list[32];
	unsigned short new_relay_list[32];
	int		i,cnt;

	for(i = 1; i<= 32; i++)
	{
		old_relay_list[i-1] = i;
	}

	cnt = Get_New_RelayList_By_Mask(mask, relay_num, old_relay_list,new_relay_list);
	printf("%d\n",cnt);

	for(i = 0; i< cnt; i++)
	{
		printf("%d ",new_relay_list[i]);
		if((i&7) == 7)
			putchar('\n');
	}
}


void test_lsa_rcv()
{
	
	LSA_PK lsa_pk;

	LSA_PK *lsa_pkptr = &lsa_pk;
	unsigned short lsa_pklen = 1;	// 未用到

	
	// 按big_endian 方式给LSA_PK赋值
	lsa_pkptr->pk_type = LSA_PK_TYPE;
	lsa_pkptr->cluster_state = CLUSTER_NONE;
	lsa_pkptr->cluster_size = 0;
	lsa_pkptr->cluster_header = 0xffff;
	lsa_pkptr->node_qos = 1;

	// node 10
	lsa_pkptr->send_ID = htons(10);
	lsa_pkptr->degree = 4;
	lsa_pkptr->nb_num = 7;
	lsa_pkptr->nb_list[0].node_ID = htons(Node_Attr.ID);
	lsa_pkptr->nb_list[0].path_QOS = 1;
	lsa_pkptr->nb_list[0].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[1].node_ID = htons(14);
	lsa_pkptr->nb_list[1].path_QOS = 1;
	lsa_pkptr->nb_list[1].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[2].node_ID = htons(15);
	lsa_pkptr->nb_list[2].path_QOS = 1;
	lsa_pkptr->nb_list[2].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[3].node_ID = htons(16);
	lsa_pkptr->nb_list[3].path_QOS = 1;
	lsa_pkptr->nb_list[3].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[4].node_ID = htons(12);
	lsa_pkptr->nb_list[4].path_QOS = 1;
	lsa_pkptr->nb_list[4].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[5].node_ID = htons(13);
	lsa_pkptr->nb_list[5].path_QOS = 1;
	lsa_pkptr->nb_list[5].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[6].node_ID = htons(11);
	lsa_pkptr->nb_list[6].path_QOS = 1;
	lsa_pkptr->nb_list[6].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算

	lsa_pklen = LSA_PK_LEN(lsa_pkptr->nb_num);
	*(unsigned short*)((unsigned char*)lsa_pkptr + lsa_pklen -2)= getCRC((unsigned char*)lsa_pkptr,lsa_pklen-2);
	LSA_PK_Rcvd(lsa_pkptr, lsa_pklen);

	// node 11
	lsa_pkptr->send_ID = htons(11);
	lsa_pkptr->degree = 2;
	lsa_pkptr->nb_num = 5;
	lsa_pkptr->nb_list[0].node_ID = htons(Node_Attr.ID);
	lsa_pkptr->nb_list[0].path_QOS = 1;
	lsa_pkptr->nb_list[0].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[1].node_ID = htons(12);
	lsa_pkptr->nb_list[1].path_QOS = 1;
	lsa_pkptr->nb_list[1].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[2].node_ID = htons(18);
	lsa_pkptr->nb_list[2].path_QOS = 1;
	lsa_pkptr->nb_list[2].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[3].node_ID = htons(10);
	lsa_pkptr->nb_list[3].path_QOS = 1;
	lsa_pkptr->nb_list[3].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[4].node_ID = htons(13);
	lsa_pkptr->nb_list[4].path_QOS = 1;
	lsa_pkptr->nb_list[4].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	
	lsa_pklen = LSA_PK_LEN(lsa_pkptr->nb_num);
	*(unsigned short*)((unsigned char*)lsa_pkptr + lsa_pklen -2)= getCRC((unsigned char*)lsa_pkptr,lsa_pklen-2);
	LSA_PK_Rcvd(lsa_pkptr, lsa_pklen);

	// node 12
	lsa_pkptr->send_ID = htons(12);
	lsa_pkptr->degree = 4;
	lsa_pkptr->nb_num = 8;
	lsa_pkptr->nb_list[0].node_ID = htons(Node_Attr.ID);
	lsa_pkptr->nb_list[0].path_QOS = 1;
	lsa_pkptr->nb_list[0].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[1].node_ID = htons(11);
	lsa_pkptr->nb_list[1].path_QOS = 1;
	lsa_pkptr->nb_list[1].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[2].node_ID = htons(13);
	lsa_pkptr->nb_list[2].path_QOS = 1;
	lsa_pkptr->nb_list[2].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[3].node_ID = htons(18);
	lsa_pkptr->nb_list[3].path_QOS = 1;
	lsa_pkptr->nb_list[3].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[4].node_ID = htons(10);
	lsa_pkptr->nb_list[4].path_QOS = 1;
	lsa_pkptr->nb_list[4].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[5].node_ID = htons(16);
	lsa_pkptr->nb_list[5].path_QOS = 1;
	lsa_pkptr->nb_list[5].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[6].node_ID = htons(17);
	lsa_pkptr->nb_list[6].path_QOS = 1;
	lsa_pkptr->nb_list[6].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[7].node_ID = htons(19);
	lsa_pkptr->nb_list[7].path_QOS = 1;
	lsa_pkptr->nb_list[7].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算

	lsa_pklen = LSA_PK_LEN(lsa_pkptr->nb_num);
	*(unsigned short*)((unsigned char*)lsa_pkptr + lsa_pklen -2)= getCRC((unsigned char*)lsa_pkptr,lsa_pklen-2);
	LSA_PK_Rcvd(lsa_pkptr, lsa_pklen);

	// node 13
	lsa_pkptr->send_ID = htons(13);
	lsa_pkptr->degree = 4;
	lsa_pkptr->nb_num = 7;
	lsa_pkptr->nb_list[0].node_ID = htons(Node_Attr.ID);
	lsa_pkptr->nb_list[0].path_QOS = 1;
	lsa_pkptr->nb_list[0].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[1].node_ID = htons(12);
	lsa_pkptr->nb_list[1].path_QOS = 1;
	lsa_pkptr->nb_list[1].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[2].node_ID = htons(16);
	lsa_pkptr->nb_list[2].path_QOS = 1;
	lsa_pkptr->nb_list[2].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[3].node_ID = htons(17);
	lsa_pkptr->nb_list[3].path_QOS = 1;
	lsa_pkptr->nb_list[3].mpr_distance = 1;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[4].node_ID = htons(18);
	lsa_pkptr->nb_list[4].path_QOS = 1;
	lsa_pkptr->nb_list[4].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[5].node_ID = htons(11);
	lsa_pkptr->nb_list[5].path_QOS = 1;
	lsa_pkptr->nb_list[5].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算
	lsa_pkptr->nb_list[6].node_ID = htons(10);
	lsa_pkptr->nb_list[6].path_QOS = 1;
	lsa_pkptr->nb_list[6].mpr_distance = 2;		// 注：bit7的mpr位都置0，暂不计算

	lsa_pklen = LSA_PK_LEN(lsa_pkptr->nb_num);
	*(unsigned short*)((unsigned char*)lsa_pkptr + lsa_pklen -2)= getCRC((unsigned char*)lsa_pkptr,lsa_pklen-2);
	LSA_PK_Rcvd(lsa_pkptr, lsa_pklen);
	
	// 打印一跳邻居表 oneHopNb_List，多跳邻居表 mHopNb_List
	Print_oneHopNb_List();
	Print_mHopNb_List();
}

void test_rrep_rcv()
{
	/* 1->3->2->4->5*/
	RREP_PK rrep_pk;

	RREP_PK *rrep_pkptr = &rrep_pk;
	
	
	rrep_pkptr->pk_type = RREP_PK_TYPE;
	rrep_pkptr->reserved = 0;
	rrep_pkptr->send_ID = htons(4);
	rrep_pkptr->rcv_ID = htons(Node_Attr.ID);
	rrep_pkptr->rreq_src_ID = htons(1);
	rrep_pkptr->rreq_dest_ID = htons(5);
	rrep_pkptr->rreq_seq = htons(203);
	rrep_pkptr->path_QOS = 1;
	rrep_pkptr->hop = 5;
	rrep_pkptr->trace_list[0]=htons(1);
	rrep_pkptr->trace_list[1]=htons(3);
	rrep_pkptr->trace_list[2]=htons(2);
	rrep_pkptr->trace_list[3]=htons(4);
	rrep_pkptr->trace_list[4]=htons(5);

	rrep_pkptr->pk_crc = getCRC((unsigned char*)rrep_pkptr, sizeof(RREP_PK)-2);
	RREP_PK_Rcvd(rrep_pkptr, sizeof(RREP_PK));

	Print_IERP_List();
}

void test_rreq_rcv()
{
	/* 2->3->1  ->4->5*/
	RREQ_PK		rreq_pk;
	RREQ_PK*	rreq_pkptr = &rreq_pk;

	rreq_pkptr->pk_type = RREQ_PK_TYPE;
	rreq_pkptr->TTL = MAX_HOP-3;
	rreq_pkptr->send_ID = htons(3);
	rreq_pkptr->rcv_ID = htons(0xffff);
	rreq_pkptr->src_ID = htons(2);
	rreq_pkptr->dest_ID = htons(5);
	rreq_pkptr->rreq_seq = htons(48);
	rreq_pkptr->path_QOS = 1;
	rreq_pkptr->hop = 2;
	rreq_pkptr->trace_list[0] = htons(2);
	rreq_pkptr->trace_list[1] = htons(3);

	rreq_pkptr->pk_crc = getCRC((unsigned char*)rreq_pkptr, sizeof(RREQ_PK)-2);
	RREQ_PK_Rcvd(rreq_pkptr, sizeof(RREQ_PK));

	Print_RREQ_Record_List();
}

void test_APP_PK_Send()
{
	unsigned char buff[1024];
	unsigned char *app_pkptr = buff;
	unsigned short app_len = 1024;

	APP_PK_Send(app_pkptr, app_len);
}