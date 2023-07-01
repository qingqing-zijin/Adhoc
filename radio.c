/*
 * radio.c
 *
 *  Created on: May 4, 2018
 *      Author: root
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "radio.h"
#include "serial.h"
#include "gpio.h"
#include "aux_func.h"
#include "Congitive.h"
#include "nRF24L01.h"

extern int e43_aux_gpio_input_fd ;

extern int self_node_id ;
extern int radio_dat_mode;
//extern int e32_uart_fd;

//extern u8 	global_radio_on_receving ;

/*����E32-����ģ��Ĺ���˵����
 * AUX���ţ�AUX���������ַ�����ָʾ���Լ�ָʾ
 * ��ָʾģ���Ƿ���������δͨ�����߷����ȥ�����Ѿ��յ��������ݰ��Ƿ���δͨ������ȫ������
 * ��ģ�����ڳ�ʼ���Լ�����С�
 * 5.6.1 �����������ָʾ
 * ���ڻ��������е��ⲿMCU��ģ�鴮���ⷢ����ʱ��AUX����
 * ����ǰ2��3ms����͵�ƽ�����ڻ����ⲿMCU�������յ����������ݶ��Ѿ�
 * ͨ��TXD������AUX����ߵ�ƽ����ʵ����RX������Ϊ�ա�
 *
 * 5.6.2 ���߷���ָʾ
 * ��AUX=1ʱ�������������512�ֽڵ����ݶ���д�뵽����оƬ���Զ��ְ�����Ҳ�����һ�������Ѿ�д��RFоƬ���������䡣
 * �û����Լ�������512�ֽڵ����ݣ����������
 * ע�⣺��AUX =1ʱ����������ģ��ȫ���������ݾ�ͨ�����߷�����ϣ�Ҳ�������һ���������ڷ����С�
 * ��AUX=0ʱ����������Ϊ�գ���ʱģ���п����ڵȴ��û����ݽ�����ʱ�����ڲ����512�ֽڻ�������������δȫ��д�뵽����оƬ���������䡣
 * ��ʱ���ڽ������߷ְ����䡣
 *
 * ע��5.6.2��5.6.3��������1&2,AUX����͵�ƽ���ȣ��������κ�һ������͵�ƽ������AUX������͵�ƽ��������
 * ������������ʱ��AUX����ߵ�ƽ��
 * �������M0M1=00ģʽ�շ����Ʋο�6.2��
 *
 * *
 * 5.6.3ģ���������ù�����
 * ���ڸ�λ���˳�����ģʽ������ģʽM0M1=11����ʱ�򣬾������AUX=0,�����Լ����̿�ʼ��
 * �Լ���ɺ�����������AUX=1;
 * ע�⣺�û��л����µ�ģʽ��������Ҫ��AUX������2ms��ģ��Ż����������ģʽ�����AUX
 * һֱ���ڸߵ�ƽ����ôģʽ�л���������Ч�����⣬�û���ģʽ3���뵽����ģʽ���ڸ�λ�����У�
 * ģ��������������ڲ������ڼ�AUX=0��
 *
 * *
 *  6.2 һ��ģʽ��ģʽ0��
 *  ��M0M1=00ʱ��ģ�鹤����ģʽ0��
 *  *
 *
 *  ����ʱ��ģ��������Դ��ڵ��û����ݣ�ģ�鷢���������ݰ�����Ϊ58�ֽڡ����û�
 *  �����������ﵽ58�ֽ�ʱ��ģ�齫�������߷���;��ʱ�û����Լ���������Ҫ��������ݡ�
 *  ����ģ����Զ��ְ����ԣ�
 *  1�����û���Ҫ������ֽ�С��58�ֽ�ʱ��ģ��ȴ�3�ֽ�ʱ�䣬�����û����ݼ������룬
 *  ����Ϊ������ֹ����ʱģ�齫�������ݾ����߷���;
 *  2����ģ���յ���һ���û����ݺ󣬽�AUX����͵�ƽ����ģ����������ݶ��ŵ�RFоƬ�������һ�����ݾ�
 *  ���������AUX����ߵ�ƽ��
 * *
 *
 *  ����ʱ��ģ��һֱ�����߽��չ��ܣ����Խ�������ģʽ0��ģʽ1���������ݰ���
 *  �յ����ݰ���ģ��AUX����͵�ƽ�����ӳ�5ms�󣬿�ʼ����������ͨ������TXD�����������������
 *  ���ݶ�ͨ�����������ģ�齫AUX����ߵ�ƽ��
 * *
 *
 * */
/*
 *1�� 16���Ƹ�ʽ����C0+5BYTES ��������������
 * */
u8 cfgSet_cmd0[6] = { 0xC0, 0, };
/*
 *2�� 16���Ƹ�ʽ����C1+C1+C1 ����ѯ�ѱ���Ĳ���	 * */
u8 Get_Params_Cmd[3] = { 0xC1, 0xC1, 0xC1 };

/*
 *3�� 16���Ƹ�ʽ����C2+5BYTES ������������ʧ
 * */
u8 cfgSet_cmd1[6] = { 0xC2, 0, };

/*
 *4��������ģʽ�£�C3 C3 C3ָ���ѯ��ǰ�����ò��������ر���C3 32 XX YY,
 *CHAN32��ʾ433MHz����Ƶ�ʡ����㹫ʽΪ��410MHz+ CHAN*1MHz
 * */
u8 version_cmd[3] = { 0xC3, 0xC3, 0xC3 };

/*
 * 5��16���Ƹ�ʽ����,ģ�齫����һ�θ�λ
 * */
u8 reset_cmd[3] = { 0xC4, 0XC4, 0XC4 };

int total_rving_bytes = -1;


#define APP_MALLOC_EN		0


#if APP_MALLOC_EN

#else

RADIO_E01_APP_t e01_app_pks[E01_RADIO_PKTS_MAX_SZ+1];
RADIO_E32_APP_t e32_app_pks[E32_RADIO_PKTS_MAX_SZ+1];

#endif


static int e01_radio_pks_sendto( u8* sendBytes, int send_len, unsigned short addrto)
{
	int pks = 0;
	int last_pk_sz = 0;
	int i=0;
	int offset =0;
	/*
	 *STEP1> ��������͵��ֽ���Ҫ�ּ�������
	 * */
	int len = send_len;

	/*
	 *�жϴ������ֽ����Ƿ�ΪAPP_PACK_PAY_LOAD_MAX���������������ǻ��ô������һ��
	 * */
	last_pk_sz = len % E01_APP_PKT_PAY_LOAD_MAX;

	if(last_pk_sz !=0 )
	{
		//�����͵��ֽ�����ΪPACK_LEN��������
		pks = (len / E01_APP_PKT_PAY_LOAD_MAX) +1;
	}
	else
	{
		//�����͵��ֽ���ΪPACK_LEN������
		pks = len / E01_APP_PKT_PAY_LOAD_MAX ;
	}
	if(pks ==0 || sendBytes == NULL)
		return 0;

#if APP_MALLOC_EN
	RADIO_APP_t *e01_app_pks = malloc(sizeof(RADIO_APP_t)*pks);
	if(e01_app_pks == NULL)
	{
		LOG_MSG(ERR_LEVEL, "e01_radio_pks_sendto malloc error!\n");
		exit(-1);
	}
#endif

	memset((u8*)e01_app_pks,0, sizeof(RADIO_E01_APP_t)*pks);

	/*
	 * �������͵����ݵģ���������Ӧ�ķ���
	 * */
	for(i=0; i< (pks-1);i++)
	{
//		app_pks[i].head.pk_type = Others_PK;
		e01_app_pks[i].head.rcv_id  = addrto & 0xff;
		e01_app_pks[i].head.pay_total_bytes = len;

		e01_app_pks[i].head.pk_seq = i;
		e01_app_pks[i].head.pk_payload_len = E01_APP_PKT_PAY_LOAD_MAX;

		e01_app_pks[i].head.pk_head_crc = getCRC((u8*)&e01_app_pks[i], sizeof(RADIO_APP_Head)-2);

		//�����������ֽ���buffer��
		offset = i*E01_APP_PKT_PAY_LOAD_MAX;
		memcpy(e01_app_pks[i].data ,&sendBytes[offset], E01_APP_PKT_PAY_LOAD_MAX);
	}

	//�������һ��
	e01_app_pks[i].head.rcv_id  = addrto & 0xff;
//	app_pks[i].head.pk_type = Others_PK;
	e01_app_pks[i].head.pay_total_bytes = len;
	e01_app_pks[i].head.pk_seq = i;

	//��last_pk_sz=0,˵�����һ��Ϊ����
	e01_app_pks[i].head.pk_payload_len = last_pk_sz > 0 ? last_pk_sz : E01_APP_PKT_PAY_LOAD_MAX;
	e01_app_pks[i].head.pk_head_crc = getCRC((u8*)&e01_app_pks[i], sizeof(RADIO_APP_Head)-2);
	offset = i*E01_APP_PKT_PAY_LOAD_MAX;
	memcpy(e01_app_pks[i].data ,&sendBytes[offset], e01_app_pks[i].head.pk_payload_len);

	i=0;
	//��ʼ����������ģ��
	for(i=0;i<(pks-1); i++)
	{
		L01_SendPacket((u8*)&e01_app_pks[i],E01_RADIO_PKT_MAX_BYTES);
	}
	//�������һ��
	L01_SendPacket((u8*)&e01_app_pks[i],(sizeof(RADIO_APP_Head) + e01_app_pks[i].head.pk_payload_len) );
	LOG_MSG(DBG_LEVEL, "e01_radio_pks_sendto over!\n");
#if APP_MALLOC_EN
	free(e01_app_pks);
	e01_app_pks= NULL;
#endif

	return 0;
}

/*
 * @function:
 * 		e32_radio_pks_sendto
 * @desc:
 * 		E32��̨���ͺ���
 *  E32����ʱ��ģ��������Դ��ڵ��û����ݣ�ģ�鷢���������ݰ�����Ϊ58�ֽڡ�����ģ����Զ��ְ����ԣ�
 *  1�����û���Ҫ������ֽ�С��58�ֽ�ʱ��ģ��ȴ�3�ֽ�ʱ�䣬�����û����ݼ������룬����Ϊ������ֹ����ʱģ�齫�������ݾ����߷���;
 *  2�����û������������ﵽ58�ֽ�ʱ��ģ�齫�������߷���;��ʱ�û����Լ���������Ҫ��������ݡ�
 *  3�����⵱ģ���յ���һ���û����ݺ󣬽�AUX����͵�ƽ����ģ����������ݶ��ŵ�RFоƬ�������һ�����ݾ����������AUX����ߵ�ƽ��
 *
 * @param sendBytes:
 * 		�����ֽ�����
 * @param send_len:
 * 		�����ֽ����鳤�ȣ��������650�ֽ�.
 * @param addrto
 * 		����Ŀ�ĵ�ַ
 * @return:
 * 		int, 0 �ɹ���
 *
 * */
static int e32_radio_pks_sendto(int uart_fd, int aux_fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	int pks = 0;
	int last_pk_sz = 0;
	int i=0;
	int offset =0;
	int last_send_sz =0;
	int len =  send_len;
	/*
	 *STEP1> ��������͵��ֽ���Ҫ�ּ�������
	 *�жϴ������ֽ����Ƿ�ΪAPP_PACK_PAY_LOAD_MAX���������������ǻ��ô������һ��
	 * */
	last_pk_sz = len % E32_APP_PKT_PAY_LOAD_MAX;

	if(last_pk_sz !=0 )
	{
		//�����͵��ֽ�����ΪPACK_LEN��������
		pks = (len /E32_APP_PKT_PAY_LOAD_MAX) +1;
	}
	else
	{
		//�����͵��ֽ���ΪPACK_LEN������
		pks = len / E32_APP_PKT_PAY_LOAD_MAX ;
	}


	if(pks ==0)
		return 0;
//	LOG_MSG(ERR_LEVEL, "last_pk_sz=%d,pks=%d \n", last_pk_sz, pks);

#if APP_MALLOC_EN
	RADIO_APP_t* e32_app_pks= (RADIO_APP_t*)malloc(sizeof(RADIO_APP_t)*pks);

	if(e32_app_pks == NULL)
	{
		LOG_MSG(ERR_LEVEL, "e32_radio_pks_sendto malloc error!\n");
		exit(-1);
	}
#endif

	memset((u8*)e32_app_pks,0, sizeof(RADIO_E32_APP_t)*pks);

	/*
	 * �������͵����ݵģ���������Ӧ�ķ���
	 * */
	i=0;
	for(i=0; i< (pks-1);i++)
	{
//		app_pks[i].head.pk_type = Others_PK;
		e32_app_pks[i].head.rcv_id  = addrto & 0xff;
		e32_app_pks[i].head.pay_total_bytes = len;

		e32_app_pks[i].head.pk_seq = i;
		e32_app_pks[i].head.pk_payload_len = E32_APP_PKT_PAY_LOAD_MAX;

		e32_app_pks[i].head.pk_head_crc = getCRC((u8*)&e32_app_pks[i], sizeof(RADIO_APP_Head)-2);

		//�����������ֽ���buffer��
		offset = i*E32_APP_PKT_PAY_LOAD_MAX;
		memcpy(e32_app_pks[i].data ,&sendBytes[offset], E32_APP_PKT_PAY_LOAD_MAX);
	}

	//�������һ��
	e32_app_pks[i].head.rcv_id  = addrto & 0xff;
//	app_pks[i].head.pk_type = Others_PK;
	e32_app_pks[i].head.pay_total_bytes = len;
	e32_app_pks[i].head.pk_seq = i;

	e32_app_pks[i].head.pk_payload_len = last_pk_sz > 0 ? last_pk_sz : E32_APP_PKT_PAY_LOAD_MAX;
//	e32_app_pks[i].head.pk_payload_len = E32_APP_PKT_PAY_LOAD_MAX;
	e32_app_pks[i].head.pk_head_crc = getCRC((u8*)&e32_app_pks[i], sizeof(RADIO_APP_Head)-2);
	offset = i*E32_APP_PKT_PAY_LOAD_MAX;

	memcpy(e32_app_pks[i].data ,&sendBytes[offset], e32_app_pks[i].head.pk_payload_len);



	/*
	 * 2018-07-23
	 * E32-DTU-5W�Զ��ְ����͹���
	 * 	1> ���䳤�ȣ�����512�ֽڣ��ڲ��Զ��ְ�58�ֽڷ���;
	 * 	   ���ѡ��͸�����䣬����buffer�ɴ�512�ֽ�
	 *  2> ���ճ��ȣ�����512�ֽڣ��ڲ��Զ��ְ�58�ֽڷ���
	 *
	 * �������e32ģ���ڲ��Զ��ְ�58�ֽڷ���ԭ�򣬿������һ����飺
	 * 1> �����Ͱ���С��58�ֽڣ�����58�ֽڵ���ĩβ����ֽ�0�� Ȼ��58�ֽڷ��ͣ�
	 * �Ӷ�ȷ��ÿ��һС������֡��MAC_REQ/REP/OK/OK_AC���ܼ�ʱ���ͣ�ÿ����������С��Ҳ�ܶ������ͳ�ȥ��
	 * �˲�����Ҫradio_pkts_recvfrom()����pay_total_bytes,�����Ƿ������ɡ�
	 * */
	if(radio_dat_mode == DTU_MOD)
	{
		/*
		if(last_pk_sz == 0)
		{
			last_send_sz = E32_RADIO_PKT_MAX_BYTES* pks;
		}
		else
		{
			last_send_sz = E32_RADIO_PKT_MAX_BYTES* (pks-1) +sizeof(RADIO_APP_Head)+last_pk_sz ;
		}*/
		last_send_sz = E32_RADIO_PKT_MAX_BYTES* pks;		//���һ�����飬Ҳ��58�ֽڷ���

		//DTUģʽ�������з���һ����������ģ��
		uartsendto(uart_fd, (u8*)e32_app_pks, last_send_sz);

		/*
		 * 1>��E32ģ����������ݶ��ŵ�RFоƬ�������һ�����ݾ����������AUX����ߵ�ƽ��
		 * ���溯��gpio_edge_poll�����ȴ�AUX�����ص�����Ȼ�󷵻�.
		 * 2> Poll��������timeout =-1ʱ, ��ʾ�����ȴ�TX sent�¼�����
		 * */
		gpio_edge_poll(aux_fd, TX_AUX, -1); //0x1ffff);
		LOG_MSG(DBG_LEVEL, "e32_radio_pks_sendto over\n");
//		usleep(10000);	//�ȴ�10ms
//		LOG_MSG(ERR_LEVEL, "e32_radio_pks_sendto over, sizeof(RADIO_APP_Head)=%d last_send_sz=%d!\n", sizeof(RADIO_APP_Head),last_send_sz);

//		if(sendBytes[0]==0x22)
//		{
//			print_buffer((u8*)e32_app_pks, last_send_sz);
//		}
	}
	else
	{
		/*
		 * ���㴫��ģʽ��
		 * ����оƬȱ�ݣ����㴫��ģʽ��ֻ�е�һ�����е�ַ��Ϣ��������������ַ��Ϣ��
		 * ��˹��̲�֧�ֶ��㴫��ģʽ��
		 * */
	}

#if APP_MALLOC_EN
	free(e32_app_pks);
	e32_app_pks =NULL;
#endif

	return 0;

}

/*
 * @function:
 * 		radio_pks_sendto
 * @desc:
 * 		E32��̨���ͺ���
 *  E32����ʱ��ģ��������Դ��ڵ��û����ݣ�ģ�鷢���������ݰ�����Ϊ58�ֽڡ�����ģ����Զ��ְ����ԣ�
 *  1�����û���Ҫ������ֽ�С��58�ֽ�ʱ��ģ��ȴ�3�ֽ�ʱ�䣬�����û����ݼ������룬����Ϊ������ֹ����ʱģ�齫�������ݾ����߷���;
 *  2�����û������������ﵽ58�ֽ�ʱ��ģ�齫�������߷���;��ʱ�û����Լ���������Ҫ��������ݡ�
 *  3�����⵱ģ���յ���һ���û����ݺ󣬽�AUX����͵�ƽ����ģ����������ݶ��ŵ�RFоƬ�������һ�����ݾ����������AUX����ߵ�ƽ��
 *
 * @param sendBytes:
 * 		�����ֽ�����
 * @param send_len:
 * 		�����ֽ����鳤�ȣ��������650�ֽ�.
 * @param addrto
 * 		����Ŀ�ĵ�ַ
 * @return:
 * 		int, 0 �ɹ���
 *
 * */
int radio_pks_sendto(int fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	int len = send_len > 512 ? 512: send_len;

	if(g_UPG_on == TRUE)
	{
		return 0;
	}

	switch(g_radio_speed)
	{
		case SPD_E32_4K8:
		case SPD_E32_9K6:
		case SPD_E32_19K2:
		case SPD_E32_FLAG:
		{
			e32_radio_pks_sendto(fd,M_e32_aux_gpio_input_fd, sendBytes, len, addrto);
			break;
		}
		case SPD_E01_250K:
		case SPD_E01_1M:
		case SPD_E01_2M:
		case SPD_E01_FLAG:
		{
			e01_radio_pks_sendto(sendBytes, len, addrto);
			break;
		}
		default:
			break;
	}
	return 0;
}

void print_buffer(u8* buf, int len)
{
	int i=0;
	for(i=0; i<len;i++)
	{
		printf("[%d]=%02x ",i, buf[i]);
		if((i+1)%5==0)
		{
			printf("\n");
		}
	}
	printf("\n");
}

extern int L01_IRQ_fd;

static int e01_radio_pks_recvfrom(u8* buffer, int len)
{
	int rv_sz = 0;

	u8  pk_seq=0;
	u8  pk_payload_len=0;

	u8 rcv_id;

	RADIO_E01_APP_t e01_app_data;

	memset((u8*)&e01_app_data ,0, sizeof(e01_app_data));

	int rv_total_sz = 0;		/*�ѽ��ն����ֽ�*/

	total_rving_bytes =0;		/*ʣ����ն����ֽڴ�����*/

	u8 rv_pk_head = FALSE;

	//u8 tempBuff[58]={0,};

	//�����µ����ݰ�����
	do{

		gpio_edge_poll(L01_IRQ_fd, RX_AUX,0x1ff);//0x1ffff); //0x1ff);		//timeout =0����ʾ������ʽ

		rv_sz = L01_RecvPacket((u8*) &e01_app_data, E01_RADIO_PKT_MAX_BYTES);

		if(rv_sz <=0)
		{
			//�������ݵȴ���ʱ�������
			continue;
		}

		/*
		 * ���ܽ��յ������ݰ�Ŀ�ĵ�ַ�Ƿ�Ϊ���ڵ㣬ֻҪ���յ����ݰ�������Ϊ
		 * �ŵ����ڱ������ڵ���ʡ�����
		 * ��Ϊ���������нڵ�����ŵ���ʽ��ͬƵ�����Թ㲥�ķ�ʽ���������߿տ�
		 * */
//		global_radio_on_receving = TRUE;

		if(getCRC((u8*)&e01_app_data, sizeof(RADIO_APP_Head)) !=0)
		{
			LOG_MSG(ERR_LEVEL, "e01_radio_pks_recvfrom rcv_pk_crc error!, rv_sz=%d\n", rv_sz);
			total_rving_bytes =0;
			rv_total_sz =0;
			rv_pk_head = FALSE;
			continue;
		}
		//�ж�Ŀ�ĵ�ַ�Ƿ�Ϊ�㲥��ַ�򱾽ڵ��id
		rcv_id = e01_app_data.head.rcv_id ;
		pk_seq = e01_app_data.head.pk_seq ;

		pk_payload_len =e01_app_data.head.pk_payload_len;

		if(rcv_id != 255 && rcv_id!= self_node_id)
		{
//			printf("--------------------------------------rcv_pk dest_id=%d ,discard!\n", rcv_id);
			continue;
		}
		else
		{
			if(pk_seq ==0)
			{
				//���յ���һ��
				total_rving_bytes = e01_app_data.head.pay_total_bytes;
				memcpy(buffer, e01_app_data.data, pk_payload_len);
				//printf("rcv_pack[%d], total_bytes = %d, rcv_pk_sz =%d\n",pk_seq, total_rving_bytes, pk_payload_len);
				//���¼�������յ��ֽں����յ��ֽ���
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				rv_pk_head = TRUE;
			}
			else
			{
				//���ǵ�һ�����ݿ�������Ӧ��λ��
				memcpy(&buffer[rv_total_sz], e01_app_data.data, pk_payload_len);

				//���¼�������յ��ֽ�
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				//printf("rcv_pack[%d], rv_total_sz = %d, pk_rv_sz=%d, total_rving_bytes=%d \n",pk_seq ,rv_total_sz, pk_payload_len ,total_rving_bytes);
			}
		}
	}while(total_rving_bytes >0);

//	global_radio_on_receving = FALSE;

	if(rv_pk_head == TRUE)
	{
		return rv_total_sz;
	}
	else
	{
		return 0;
	}
}


static int e32_radio_pks_recvfrom(int uart_fd, int aux_fd,u8* buffer, int len)
{
	int rv_sz = 0;
	u8  pk_seq=0;
	u8  pk_payload_len=0;
	u8 rcv_id;

	RADIO_E32_APP_t app_data;
	int rv_total_sz = 0;		/*�ѽ��ն����ֽ�*/

	memset((u8*)&app_data ,0, sizeof(app_data));
	total_rving_bytes =0;		/*ʣ����ն����ֽڴ�����*/

	u8 rv_pk_head = FALSE;

	/*
	 * �������ݰ�����:
	 * ����Ĺ����Ǵ����߽���ÿһ��RADIO_APP_PKT, ������RADIO_APP_HEAD���ݣ�
	 * ��ȡ��Ч�û�pkt_payload���ݡ�
	 * ͬʱ���Խ��յ���ÿһ��pkt_payload���ݣ�������buffer��Ӧ��λ�á�
	 * */
	do{
//		gpio_edge_poll(e32_aux_gpio_input_fd, RX_AUX,0);
		/*
		 * ���ó�ʱʱ��(0x1ff ms)���ȴ�RX�¼�����
		 * */
		gpio_edge_poll(aux_fd, RX_AUX,-1);//0x1ff);//0x1ffff);

		rv_sz = uartrecvfrom(uart_fd,(u8*) &app_data, E32_RADIO_PKT_MAX_BYTES);
		if(rv_sz <=0)
		{
//			total_rving_bytes =0;
//			rv_total_sz =0;
//			rv_pk_head = false;
			//�������ݵȴ���ʱ�������
			continue;
		}

		/*
		 * ���ܽ��յ������ݰ�Ŀ�ĵ�ַ�Ƿ�Ϊ���ڵ㣬ֻҪ���յ����ݰ�������Ϊ
		 * �ŵ����ڱ������ڵ���ʡ�����
		 * ��Ϊ���������нڵ�����ŵ���ʽ��ͬƵ�����Թ㲥�ķ�ʽ���������߿տ�
		 * */
//		global_radio_on_receving = TRUE;

		if(getCRC((u8*)&app_data, sizeof(RADIO_APP_Head)) !=0)
		{
			LOG_MSG(ERR_LEVEL, "e32_radio_pks_recvfrom rcv_pk_crc error, rv_sz=%d!\n", rv_sz);

			print_buffer((u8*)&(app_data),  rv_sz);

			total_rving_bytes =0;
			rv_total_sz =0;
			rv_pk_head = FALSE;
			continue;
		}
//		LOG_MSG(ERR_LEVEL, "e32_radio_pks_recvfrom rcvd_pk, rv_sz=%d!\n", rv_sz);

		//�ж�Ŀ�ĵ�ַ�Ƿ�Ϊ�㲥��ַ�򱾽ڵ��id
		rcv_id = app_data.head.rcv_id ;
		pk_seq = app_data.head.pk_seq ;
		pk_payload_len =app_data.head.pk_payload_len;

		if(rcv_id != 255 && rcv_id!= self_node_id)
		{
//			printf("--------------------------------------rcv_pk dest_id=%d ,discard!\n", rcv_id);
			continue;
		}
		else
		{

			/*2018-06-22�������packets����������
			 * ����E32���ߵ�һ�η��͵ı��ĳ������ƣ��û��ն˷��͵����ݱ���ֳ�Ϊ����Ӱ�����
			 * ���������ڽ����Ӱ����������߳��л���������uartSender��ʱ����ʱ��������ʱ
			 * uartSender��ʱ���ڶ�ʱ���ص������У�Ӧ�ò��á�����˱ܲ��ԡ����ó��ŵ����ȴ�������ϣ��ڷ��͡�
			 * */
			if(pk_seq ==0)
			{
				/*
				 * ���յ���һ�����������������������ĳ���pay_total_bytes�ֶΣ�
				 * �����ݸ��ֶμ����ʱ�������һ�������ģ�
				 * Ȼ�󷵻ش���
				 * */
				total_rving_bytes = app_data.head.pay_total_bytes;

				memcpy(buffer, app_data.data, pk_payload_len);
				//printf("rcv_pack[%d], total_bytes = %d, rcv_pk_sz =%d\n",pk_seq, total_rving_bytes, pk_payload_len);
				//���¼�������յ��ֽں����յ��ֽ���
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				rv_pk_head = TRUE;
			}
			else
			{
				//���ǵ�һ�����ݿ�������Ӧ��λ��
				memcpy(&buffer[rv_total_sz], app_data.data, pk_payload_len);

				//���¼�������յ��ֽ�
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				//printf("rcv_pack[%d], rv_total_sz = %d, pk_rv_sz=%d, total_rving_bytes=%d \n",pk_seq ,rv_total_sz, pk_payload_len ,total_rving_bytes);
			}
		}
	}while(total_rving_bytes >0);

//	global_radio_on_receving = FALSE;

	if(rv_pk_head == TRUE)
	{
		return rv_total_sz;
	}
	else
	{
		return 0;
	}
}

/*
 * @function radio_recvfrom
 *
 * @desc ������ģ����տտ�����
 *  ����ʱ��ģ��һֱ�����߽��չ��ܣ����Խ��������������ģ��ģʽ0��ģʽ1���������ݰ���
 *  �յ����ݰ���ģ��AUX����͵�ƽ�����ӳ�5ms�󣬿�ʼ����������ͨ������TXD�����������������
 *  ���ݶ�ͨ�����������ģ�齫AUX����ߵ�ƽ��
 *
 * @return :���ش����߽��յ���������pkt_payload, ������buffer�У�
 * 		����ֵΪ���յ�pkt_payload�ֽ���
 * */
int radio_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len)
{
	int ret=0;
	if(g_UPG_on == TRUE)
	{
		return 0;
	}

	switch(g_radio_speed)
	{
		case SPD_E32_4K8:
		case SPD_E32_9K6:
		case SPD_E32_19K2:
		case SPD_E32_FLAG:
		{
			ret = e32_radio_pks_recvfrom(fd, aux_fd, buffer, len);
			break;
		}
		case SPD_E01_250K:
		case SPD_E01_1M:
		case SPD_E01_2M:
		case SPD_E01_FLAG:
		{
			ret = e01_radio_pks_recvfrom(buffer, len);
			break;
		}
		default:
			break;
	}
	return ret;
}

int L_E32_pks_sendto(int uart_fd,int aux_fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	e32_radio_pks_sendto(uart_fd,aux_fd, sendBytes, send_len, addrto);
	return 0;
}

int L_E32_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len)
{
	int ret= e32_radio_pks_recvfrom(fd, aux_fd, buffer, len);
	return ret;
}

/*
 *��ʼ��E32\E43����ģ�飬 ������������ģ��AUX��MCU����Ϊ���뷽��, ��ʹ���������ж�
 * */
int radios_gpio_init()
{
	/********************************************************
	 *				����E32ģ��GPIO��ʼ��
	 *******************************************************/
	gpio_export(M_E32_AUX_GPIO_PIN_NUM);
	gpio_direction(M_E32_AUX_GPIO_PIN_NUM, GPIO_IN);
	//ʹ���������ж�
	gpio_edge(M_E32_AUX_GPIO_PIN_NUM,RISING);
	//��AUX ��Ӧ/sys/class/gpio/gpiox/value�ļ���ʵʱ��ȡAUXֵ
	M_e32_aux_gpio_input_fd = open_gpio_rw_fd(M_E32_AUX_GPIO_PIN_NUM);

	if(M_e32_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open_gpio_rw_fd M_e32_aux_gpio_input_fd value!\n");
		return -1;
	}

	gpio_export(M_E32_M0_GPIO_PIN_NUM);
	gpio_direction(M_E32_M0_GPIO_PIN_NUM, GPIO_OUT);



	/********************************************************
	 *				����E32ģ��GPIO��ʼ��
	 *******************************************************/
	gpio_export(L_E32_AUX_GPIO_PIN_NUM);
	gpio_direction(L_E32_AUX_GPIO_PIN_NUM, GPIO_IN);
	//ʹ���������ж�
	gpio_edge(L_E32_AUX_GPIO_PIN_NUM,RISING);
	//��AUX ��Ӧ/sys/class/gpio/gpiox/value�ļ���ʵʱ��ȡAUXֵ
	L_e32_aux_gpio_input_fd = open_gpio_rw_fd(L_E32_AUX_GPIO_PIN_NUM);

	if(L_e32_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open_gpio_rw_fd L_e32_aux_gpio_input_fd value!\n");
		return -1;
	}

	gpio_export(L_E32_M0_GPIO_PIN_NUM);
	gpio_direction(L_E32_M0_GPIO_PIN_NUM, GPIO_OUT);

	/********************************************************
	 *				E43ģ��GPIO��ʼ��
	 *******************************************************/
	if(global_e43_existed == FALSE)
	{
		return 0;		//���E43ģ�鲻���ڣ����ʼ����ɷ���
	}
	/*
	 *STEP2> �����������߸�֪ģ��MCU AUX����Ϊ���뷽��, ��ʹ���������ж�
	 * */
	if(e43_aux_gpio_input_fd >0)
		close(e43_aux_gpio_input_fd);

	gpio_export(E43_AUX_GPIO_PIN_NUM);
	gpio_direction(E43_AUX_GPIO_PIN_NUM, GPIO_IN);
	//ʹ���������ж�
	gpio_edge(E43_AUX_GPIO_PIN_NUM,RISING);
	//��AUX ��Ӧ/sys/class/gpio/gpiox/value�ļ���ʵʱ��ȡAUXֵ
	e43_aux_gpio_input_fd = open_gpio_rw_fd(E43_AUX_GPIO_PIN_NUM);

	if(e43_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open value!\n");
		return -1;
	}

	gpio_export(E43_M0_GPIO_PIN_NUM);
	gpio_export(E43_M1_GPIO_PIN_NUM);

	gpio_direction(E43_M0_GPIO_PIN_NUM, GPIO_OUT);
	gpio_direction(E43_M1_GPIO_PIN_NUM, GPIO_OUT);

	return 0;
}

/*
 *
 *��������ģ�鹤��ģʽ
 * */
int radio_cfg_mode(RADIO_MOD_t modType, RADIO_DEV_t dev_t)
{
	/*
	 * ģ���������ģʽģʽ��M0M1=11
	 * */
	int rv =0;


	if(dev_t==E32_M)
	{
		switch(modType)
		{
			case DTU_MOD:
			case ADDR_MOD:
				gpio_write(M_E32_M0_GPIO_PIN_NUM, GPIO_L);
				break;

			case STANDBY_MOD:
				gpio_write(M_E32_M0_GPIO_PIN_NUM, GPIO_H);
				break;

			default:
				rv =-1;
				break;
		}

	}
	else if(dev_t==E32_L)
	{
		switch(modType)
		{
			case DTU_MOD:
			case ADDR_MOD:
				gpio_write(L_E32_M0_GPIO_PIN_NUM, GPIO_L);
				break;
			case STANDBY_MOD:
				gpio_write(L_E32_M0_GPIO_PIN_NUM, GPIO_H);
				break;

			default:
				rv =-1;
				break;
		}

	}
	else if(dev_t == E43)
	{

		switch(modType)
		{
				//E43ģ�飬����ģʽM1=1, M0=0
			case STANDBY_MOD:
				gpio_write(E43_M0_GPIO_PIN_NUM, GPIO_L);
				gpio_write(E43_M1_GPIO_PIN_NUM, GPIO_H);
				break;
			case RSSI_MOD:
				//RSSIģʽ��M1=0, M0=1
				gpio_write(E43_M0_GPIO_PIN_NUM, GPIO_H);
				gpio_write(E43_M1_GPIO_PIN_NUM, GPIO_L);
				break;

			default:
			{
				rv =-1;
				break;
			}
		}
	}
	return rv;
}

/*
 * M_E32_set_radio_params
 * �������ߵ�ģ�鴮�����ʡ�����ģʽ��DTU/���㴫�䣩��IOģʽ��FEC��/�ر��Լ����书�ʴ�С
 * */
int M_E32_set_radio_params(Radio_CHL_t ch_i, u8 air_link_speed)
{
	g_UPG_on = TRUE;

	/*
	 * ģʽ�л�
	 * ��ģʽ0��һ��ģʽ����ģʽ1������ģʽ�����û���������������ݣ�ͬʱ
	 * ����ģʽ�л�����ʱ���л�ģʽ��������Ч��;
	 * ģ��Ὣ�����û����ݴ�����Ϻ󣬲�
	 * �����µ�ģʽ��⣬����һ�㽨��Ϊ��
	 * ���AUX�������״̬���ȴ�AUX����ߵ�ƽ��2ms�ٽ����л���
	 * */
	radio_cfg_mode(STANDBY_MOD,E32_M);
	gpio_edge_poll(M_e32_aux_gpio_input_fd, CFG_AUX, -1);//0x1ffff);

	usleep(2000);

	if(M_e32_uart_fd>0)
	{
		printf("M_e32_uart_fd=%d, L_e32_uart_fd=%d\n", M_e32_uart_fd,L_e32_uart_fd);
		close(M_e32_uart_fd);
	}

	M_e32_uart_fd = Uart_Open(COM1, 9600, 8, 'N', 1, 1);
	if(M_e32_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM1 for configure Radio!\n");
		return -1;
	}

	radio_cfg_params(
					E32_M,						//��E32_L,�ò���ȡE32_M/Lһ����ͨ��uart_fd�������ĸ���̨
					C0_FLASH_PARAM,
					M_e32_uart_fd,
					UART_SPEEDTYPE_115200,
					UART_MOD_8N1,
					air_link_speed,
					DTU_MOD_ON,					//ע��͸������ʱ������ΪDTU_MOD_ON
					TXD_AUX_OC_RX_PULLUP,		//6,5,4,3,2λ����д0
					FEC_OFF,
					POWER_30dBm_ON,
					(ch_i > CH_MAX_VAL ? CH_MAX_VAL : ch_i),
					0xFFFF );


	/*
	 * STEP6> �Ƴ�����ģʽ����������ģ�����DTUģʽ.
	 * ע�⣺6.5.�������ߣ����ã�ģʽ��������ģʽ��ģ����������ò��������ù����У�
	 * AUX���ֵ͵�ƽ����Ϻ�����ߵ�ƽ�����Խ����û����AUX�����ء�
	 ** */
	radio_cfg_mode(DTU_MOD,E32_M);

	/*
	 * ���ó�ʱʱ��(0x1ffff ms)���ȴ�ģʽ�л�AUX�¼�����
	 * */
	gpio_edge_poll(M_e32_aux_gpio_input_fd, CFG_AUX, -1);//0x1ffff);

	usleep(2000);	/**/

	close(M_e32_uart_fd);

	LOG_MSG(INFO_LEVEL,"--------------------------------------E32 radio cfg param over\n");

	/*
	 * STEP7> ��������õĲ����ʴ�MCU���ڣ������û�����͸������
	 * */
	M_e32_uart_fd = Uart_Open(COM1, 115200, 8, 'N', 1,1);
	if(M_e32_uart_fd <=-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM1 for configure Radio!\n");
		exit (-1);
	}
	g_UPG_on = FALSE;

	printf("LAST, M_e32_uart_fd=%d, L_e32_uart_fd=%d\n", M_e32_uart_fd,L_e32_uart_fd);

	return TRUE;
}

/*
 * radio_params_cfg
 * �������ߵ�ģ�鴮�����ʡ�����ģʽ��DTU/���㴫�䣩��IOģʽ��FEC��/�ر��Լ����书�ʴ�С
 * */
int radio_cfg_params(RADIO_DEV_t dev_t,HEAD_TYPE_t head_type,  int fd, int uartSpeed, int uart_mod, int link_speed,
		int dtu_mod_on, int io_mod, int fec_on, int pwr_type, Radio_CHL_t channl, unsigned short addr)
{
	u8 cfg_params[7] = { 0, };
	u8 getted_params[20] = { 0, };
	int recv_sz = 0;
	int cfg_times = 20;
	int cfg_state = 0;

	Radio_CHL_t temp_ch = 0;
	temp_ch = (dev_t ==E43) ? (channl*4) : ((dev_t ==E32_L||dev_t ==E32_M) ? channl+15: 0);

	do{
		cfg_params[0] = head_type; //��������в���������

		cfg_params[1] = (addr>>8) &0xff; //ģ���ַ-���ֽ�
		cfg_params[2] = addr & 0xff; //ģ���ַ-���ֽ�

		//SPED-�ֽ�byte3; ��������ģ��UART 115200bps, 8N1ģʽ;�տ�����Ϊ19.2kbps
		cfg_params[3] &=~(UART_SPEEDTYPE_Msk | UART_MOD_8N1_Msk | AIR_SPEEDTYPE_Msk);
		cfg_params[3] |= (uartSpeed) | (uart_mod) | (link_speed);

		//�����豸���ͣ�������radio.h�ж����ͳһ�ŵ����㹫ʽ�����ŵ�id

		cfg_params[4] = temp_ch;

		//OPTION�ֽڣ���������ģ�飺DTU͸��ģʽ��TX_AUX��·�����RX��������;FEC��;����ѡ��30dBm���
		cfg_params[5] &= ~(DTU_MOD_Msk | IO_MOD_Msk | FEC_ON_Msk | TX_POWER__Msk);
		cfg_params[5] |= (dtu_mod_on) | (io_mod) | (fec_on) | (pwr_type);


		//���������������ã����صĽ������C0 00 00 1A 17 44
		uartsendto(fd, cfg_params, sizeof(cfg_params)-1);

		//usleep(200);

//		uartsendto(fd, Get_Params_Cmd, 3);
		memset(getted_params,0, 6);
//		printf("----------------uartrecvfrom parameters from chip %s ...\n", dev_t==E43 ?"E43": "E32");
		recv_sz = uartrecvfrom(fd, getted_params, 6);
		if(strcmp(cfg_params, getted_params) !=0)
		{
			//���ò��ɹ�
			printf("\n----------------------------------radio cfgParams failed:\n cfg_bytes[%d] :%02x %02x %02x %02x %02x %02x\n get_bytes[%d] :%02x %02x %02x %02x %02x %02x\n",
				 6, cfg_params[0], cfg_params[1], cfg_params[2], cfg_params[3],	cfg_params[4], cfg_params[5],
				 recv_sz,
				 getted_params[0], getted_params[1], getted_params[2], getted_params[3], getted_params[4], getted_params[5]
			);

			printf("\----------------------------------end\n");

			//exit(-1);
			cfg_state = FALSE;

		}
		else
		{
//			//���óɹ�
//			printf("\----------------------------------radio cfgParams ok:\n cfg_bytes[%d] :%02x %02x %02x %02x %02x %02x\n get_bytes[%d] :%02x %02x %02x %02x %02x %02x\n",
//				  6, cfg_params[0], cfg_params[1], cfg_params[2], cfg_params[3],	cfg_params[4], cfg_params[5],
//				 recv_sz,
//				 getted_params[0], getted_params[1], getted_params[2], getted_params[3], getted_params[4], getted_params[5]
//			);
			LOG_MSG(INFO_LEVEL, "--------------------------------------radio_cfg_params ok\n");

			cfg_state = TRUE;
			break;
		}
	}while(cfg_times-- >0);

	if(cfg_state == FALSE)
	{
		close(fd);
		printf("----------------%s Config Failed Times(20), ------------------\n",dev_t==E43? "E43": (dev_t==E32_L?"E32_L": "E32_M"));
		return -1;
//		exit(-1);
	}


	return 0;
}



/*
 *@function:
 *	 config_E43_radio_rssi_mode_test
 *@brief:
 *  	����E43 RSSI����ģ�飬�����䴮��ͨ�������Լ�RSSI����ģʽ����Ȿ��Ƶ�׻���
 *@Note:
 *		�����ڲ���ģʽ
 * */
int g_e43_uart_fd =-1;

int config_E43_radio_rssi_mode_test()
{
	int sz =-1;
	u8 rv_buff[20]={0,};

	int test_cnt = CONG_MAX_TIMES_PER_CHNL;

	/*
	 *STEP2> ��������ģ���������ģʽM0M1 =11������ģ���������,��������Ƶ���ŵ���ͨ�Ŵ�������
	 * */
	radio_cfg_mode(STANDBY_MOD,E43);

	/*
	 *STEP3> ��MCU����,���ô��ڲ�����Ϊ9600��8N1ģʽ��������ģ�����ͨ�š�
	 * */
	g_e43_uart_fd = Uart_Open(COM2, 9600, 8, 'N', 1, 0);
	if(g_e43_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
		exit(-1) ;
	}
	/*
	 *STEP4> ����ģʽ�£�����ִ�в�ѯ�汾����,��֤ͨ���Ƿ�����
	 * */
//	memset(rv_buff, 0,20);
//	uartsendto(e43_uart_fd, version_cmd, 3);
//
//	//�ȴ�ACK
//	//sleep(1);	//80ms
//
//	sz = uartrecvfrom(e43_uart_fd, rv_buff, 4);
//	if(sz >0)rv_buff[sz >0] = 0;
//	//C3 32 31 14������32�������ߵ�ģ��Ƶ��Ϊ433MHz, 31��������汾
//	LOG_MSG(DBG_LEVEL,"version bytes %d, bytes: %02x %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);

	//
	/*
	 * STEP5> ����ģʽ�£�������ģ�����ò������£�
	 * ��1������������Ϊ115200��8N1ģʽ
	 * ��2���տ���������Ϊ19.2k
	 * ��3��͸������ģʽ �򶨵㴫��ģʽ������֧�ֹ㲥��ַ��
	 * ��4��FEC_ONģʽ
	 * ��5�����书�����
	 *  (6) �ŵ�ΪCH20
	 *  (7) ģ���ַ���ڵ�id��
	 * */
	radio_dat_mode = DTU_MOD;		//ͨ��ģʽ��͸�����Ե�ģʽ
	radio_cfg_params(E43,C2_RAM_PARAM,
					g_e43_uart_fd,
					UART_SPEEDTYPE_9600,
					UART_MOD_8N1,
					AIR_SPEEDTYPE_300,	//���E43,Ϊ1.2Kbps
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//ע��͸������ʱ������ΪDTU_MOD_ON
					TXD_AUX_OPEN_RX_OPEN,		//6,5,4,3,2λ����д0
					FEC_OFF,
					POWER_30dBm_ON,
					CHANL5,		//֮ǰ����ʱ��CH20��Ӧ430MHz���µļ��㹫ʽ�£�425+ CHANL*1MHz
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));

	/*
	 * STEP6> �˳�����ģʽ����������ģ�����RSSIģʽ.
	 * ע�⣺6.5.�������ߣ����ã�ģʽ��������ģʽ��ģ����������ò��������ù����У�
	 * AUX���ֵ͵�ƽ����Ϻ�����ߵ�ƽ�����Խ����û����AUX�����ء�
	 *
	 * RSSIģʽ�£����ɽ������߷��䣬�յ��Ĵ������ݽ�������;ͬ�����ɽ���
	 * �����������ݣ���ɨ�赱ǰ�ŵ����ź�ǿ�ȣ�ÿ100msͨ���������һ��ǿ��ֵ�����ֵ��
	 ** */
	radio_cfg_mode(RSSI_MOD,E43);

	/*
	 * ���ó�ʱʱ��(0x1ffff ms)���ȴ�ģʽ�л�AUX�¼�����
	 * */
	gpio_edge_poll(e43_aux_gpio_input_fd, CFG_AUX, 0x1ffff);


//	usleep(2000);
//	close(e43_uart_fd);

	/*
	 * �����������õĲ����ʴ�MCU���ڣ���ȡRSSI���ֵ
	 * */
//	e43_uart_fd = Uart_Open(COM2, 115200, 8, 'N', 1,1);
//	if(e43_uart_fd ==-1)
//	{
//		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
//		exit (-1);
//	}
	memset(rv_buff, 0,4);

	/*
	 * ��ȡRSSIֵ test_cnt��������ӡ���
	 * */
	while(test_cnt--)
	{
		sz = uartrecvfrom(g_e43_uart_fd, rv_buff, 4);
		if(sz >0)
			rv_buff[sz] = 0;

		LOG_MSG(DBG_LEVEL,"-------------------------RSSI bytes %d, bytes: %d %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);

	}


	close(g_e43_uart_fd);
	g_e43_uart_fd =-1;
	return 0;

}

//

/*@funtion:
 * 		 E43_channl_rssi_congitive
 *@brief:
 *		 ��֪����Ƶ�׻�������ȡ�ŵ�ch_i��ǰ�տ�RSSIֵ
 *@param ch_i��
 *		�ŵ���id
 *@return
 *		���������û���ȡRSSIֵ�Ĵ��ھ��
 * */
//int g_e43_uart_fd =-1;
int E43_channl_rssi_congitive(Radio_CHL_t ch_i)
{

//	static u8 config_aux_once = 0;

	int ret=0;
	/*
	 *STEP2> ��������ģ��������ߣ����ã�ģʽM0M1 =11������ģ���������,��������Ƶ���ŵ���ͨ�Ŵ�������
	 * */
	do{


	radio_cfg_mode(STANDBY_MOD,E43);

	/*
	 *STEP3> ����ģʽ�£�������MCU����,���ô��ڲ�����Ϊ9600��8N1ģʽ��������ģ�����ͨ�š�
	 * */
	if(g_e43_uart_fd == -1 || ret ==-1 )
	{
		g_e43_uart_fd = Uart_Open(COM2, 9600, 8, 'N', 1,0);
		if(g_e43_uart_fd ==-1)
		{
			LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
			return -1;
		}

	}

	/*
	 * ����select������ʵ�ִ��ڵķ���������ʱ����ģʽ
	 * */
//	LOG_MSG(DBG_LEVEL,"-------------------------------------congitive ch_i =%d\n",  ch_i);

	radio_dat_mode = DTU_MOD;		//ͨ��ģʽ��͸�����Ե�ģʽ
	//
	/*
	 * STEP4> ������ģ�����ò������£�
	 * ��1������������Ϊ115200��8N1ģʽ
	 * ��2���տ���������Ϊ19.2k
	 * ��3��͸������ģʽ �򶨵㴫��ģʽ������֧�ֹ㲥��ַ��
	 * ��4��FEC_ONģʽ
	 * ��5�����书�����
	 *  (6) �ŵ�Ϊch_i
	 *  (7) ģ���ַ���ڵ�id��㲥��ַ��
	 *  ��radio_cfg_params�����У�ͨ���жϷ��صĲ��������õĲ����Ƿ�һ�£���һ����������ã�ֱ�����ó�������ɹ����ء�
	 * */
	ret =radio_cfg_params(E43,
					C2_RAM_PARAM,
					g_e43_uart_fd,
					UART_SPEEDTYPE_9600,
					UART_MOD_8N1,
					AIR_SPEEDTYPE_300,	//���E43,Ϊ1.2Kbps
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//ע��͸������ʱ������ΪDTU_MOD_ON
					TXD_AUX_OPEN_RX_OPEN,		//6,5,4,3,2λ����д0
					FEC_OFF,
					POWER_30dBm_ON,
					(ch_i > CH_MAX_VAL ? CH_MAX_VAL : ch_i),		//֮ǰ����ʱ��CH20��Ӧ430MHz���µļ��㹫ʽ�£�425+ CHANL*1MHz
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));


	}while(ret <0);
	/*
	 * STEP5> �˳�����ģʽ����������ģ�����RSSIģʽ.
	 * ע�⣺6.5.�������ߣ����ã�ģʽ��������ģʽ��ģ����������ò��������ù����У�
	 * AUX���ֵ͵�ƽ����Ϻ�����ߵ�ƽ�����Խ����û����AUX�����ء�
	 *
	 * RSSIģʽ�£����ɽ������߷��䣬�յ��Ĵ������ݽ�������;ͬ�����ɽ���
	 * �����������ݣ���ɨ�赱ǰ�ŵ����ź�ǿ�ȣ�ÿ100msͨ���������һ��ǿ��ֵ�����ֵ��
	 ** */
	radio_cfg_mode(RSSI_MOD,E43);
	/*
	 * ���ó�ʱʱ��(0x1ffff ms)���ȴ�ģʽ�л�AUX�¼�����
	 * */
	gpio_edge_poll(e43_aux_gpio_input_fd, CFG_AUX, 0x1ffff);

//	close(e43_uart_fd);
//	LOG_MSG(INFO_LEVEL,"---------------------radio cfg param over\n");

	/*
	 * �����������õĲ����ʴ�MCU���ڣ���ȡRSSI���ֵ
	 * */
//	e43_uart_fd = Uart_Open(COM2, 115200, 8, 'N', 1,1);
	if(g_e43_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
		exit (-1);
	}

//	FD_ZERO(&rdset);
//	FD_SET(e43_uart_fd, &rdset);

	return g_e43_uart_fd;
}



/*
 * ����E32��������ģ�鹤�������������տ����ʡ�����ͨ�Ų�����
 * */
int config_E32_radio_params(RADIO_COM_t com, int aux_fd, Radio_CHL_t ch_val, u8 speed_val, u8 pwr_val)
{
	int fd =-1;
	RADIO_DEV_t dev = E32_M;;
	/*
	 *STEP2> ��������ģ���������ģʽM0M1 =11������ģ���������
	 * */
    if(com == COM1)
    {
    	dev = E32_M;
    	if(  M_e32_uart_fd >0)	close(M_e32_uart_fd);
    }
    else if(com == COM7 )
    {
    	dev = E32_L;
    	if(  L_e32_uart_fd >0)	close(L_e32_uart_fd);
    }

	radio_cfg_mode(STANDBY_MOD,dev);

	/*
	 *STEP3> ��MCU����,���ô��ڲ�����Ϊ9600��8N1ģʽ��������ģ�����ͨ�š�
	 * */
	fd = Uart_Open(com, 9600, 8, 'N', 1, 1);
	if(fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", com);
		return -1;
	}
	/*
	 *STEP4> ����ģʽ�£�����ִ�в�ѯ�汾����,��֤ͨ���Ƿ�����
	 * */
	/*
	memset(rv_buff, 0,sizeof(rv_buff));
	uartsendto(uart_fd, version_cmd, 3);
	sz = uartrecvfrom(uart_fd, rv_buff, 4);
	if(sz >0)rv_buff[sz >0] = 0;
	//C3 32 31 14������32�������ߵ�ģ��Ƶ��Ϊ433MHz, 31��������汾
	LOG_MSG(DBG_LEVEL,"version bytes %d, bytes: %02x %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);
	*/

	//
	/*
	 * STEP5> ����ģʽ�£�������ģ�����ò������£�
	 * ��1������������Ϊ115200��8N1ģʽ
	 * ��2���տ���������Ϊ19.2k
	 * ��3��͸������ģʽ �򶨵㴫��ģʽ������֧�ֹ㲥��ַ��
	 * ��4��FEC_ONģʽ
	 * ��5�����书�����
	 *  (6) �ŵ�ΪCH20
	 *  (7) ģ���ַ���ڵ�id��
	 * */
	LOG_MSG(DBG_LEVEL,"--------------------------------------my node id =%d\n", self_node_id);

	radio_dat_mode = DTU_MOD;		//ͨ��ģʽ��͸�����Ե�ģʽ
	radio_cfg_params(dev,
					C0_FLASH_PARAM,
					fd,
					UART_SPEEDTYPE_115200,
					UART_MOD_8N1,
					speed_val,
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//ע��͸������ʱ������ΪDTU_MOD_ON
					TXD_AUX_OC_RX_PULLUP,
					FEC_OFF,
					pwr_val,
					ch_val,
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));

	/*
	 * STEP6> �Ƴ�����ģʽ����������ģ�����DTUģʽ.
	 * ע�⣺6.5.�������ߣ����ã�ģʽ��������ģʽ��ģ����������ò��������ù����У�
	 * AUX���ֵ͵�ƽ����Ϻ�����ߵ�ƽ�����Խ����û����AUX�����ء�
	 ** */
	radio_cfg_mode(radio_dat_mode,dev);

	/*
	 * ���ó�ʱʱ��(0x1ffff ms)���ȴ�ģʽ�л�AUX�¼�����
	 * */
	gpio_edge_poll(aux_fd, CFG_AUX, -1);//0x1ffff);

	close(fd);

	LOG_MSG(INFO_LEVEL,"--------------------------------------E32 radio cfg param over\n");

	/*
	 * STEP7> ��������õĲ����ʴ�MCU���ڣ������û�����͸������
	 * */
	fd = Uart_Open(com, 115200, 8, 'N', 1,1);
	if(fd <=-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", com);
		exit (-1);
	}

	return fd;

}


/*
 * @function:
 * 			 e43_cong_band_rssi_test
 * @brief:
 * 			e43ȫƵ���ŵ�RSSI��֪���Ժ���
 * @param:
 * @return:
 * 			��
 * */
void e43_cong_band_rssi_test()
{
	int i =0;
	u8 rssi, max,min, avg,temp_rssi_acc =0;
	int fd=0, rv, cong_cnt=0;

	for(i=0;i < CH_MAX_NUM;i++)
	{
		fd =E43_channl_rssi_congitive(i);

		max =0, min=0, avg=0;
		temp_rssi_acc =0;
		cong_cnt =0;
		/*
		 * ���ɨ�赱ǰ�ŵ�RSSI��������¼&����RSSI���ֵ����Сֵ��ƽ��ֵ
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(fd, &rssi, 1);
			if(rv >0)
			{
				if(cong_cnt==0)
				{
					min = rssi;
					max = rssi;	//��ǰ�ŵ�RSSI���ֵ
				}
				else
				{
					if(rssi > max)
					{
						max = rssi;	//��ǰ�ŵ�RSSI���ֵ
					}

					if(rssi < min)
					{
						min = rssi;	//��ǰ�ŵ�RSSI��Сֵ
					}
				}
				temp_rssi_acc += rssi;		//�ۼ�RSSIֵ��
			}
			cong_cnt++;
			LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,i, rv ,rssi);
		}//while

		avg = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//��ǰ�ŵ�RSSIƽ��ֵ
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					i,\
					max,\
					avg,\
					min);

		close(fd);
		fd =-1;

		//׼��ɨ����һ���ŵ�
	}//for
}


