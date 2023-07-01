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
//#define     PERIOD_LSA                  1000     	// LSA �������� 3�룬����FSR���������ġ����⾶�ھӽڵ�ͨ�����ڳ����ķ��������������ͳһ��һ������     

#define		MAX_NB_NUM					250			// �������ڵ�����Ҳ����N�����ھ�����

/*----------------------------------------------------------------------
				     		IERP-DSR	
----------------------------------------------------------------------*/
/*  Timeouts  */
#define     IERP_PATH_TIMEOUT                   60000		// ���·����Ŀ��ʱʱ�䣬����60��δʹ�ø�·����Ŀ����ɾ��
//#define     IERP_RREQ_TIMEOUT        			2000		// ���·������ʱʱ�䣬Դ�ڵ㷢��RREQ��1����δ�յ�RREP���򱾴�Ѱ·ʧ��

#define     MAX_HOP                           	8		// �������������·�����ʱ����src��dest����ˣ�ʵ���������ΪMAX_HOP-1����7����

#define		MAX_NODE_NUM						65536	// IDȡֵ�ռ䣬���Ϊ65536
/*----------------------------------------------------------------------
				     		Cluster
----------------------------------------------------------------------*/
/* ȫ�����÷ǽ����أ���ˣ��������ĳһ�ڵ�ͬʱ�������������������ϴ�ͷ�����������������*/
#define	  CLUSTER_NONE								0		// �޷ִأ���������ƽ��·��ģʽ��
#define   CLUSTER_MEMBER                          	1		// �ؽڵ����ͣ���ͨ�س�Ա��ֻ��һ����ͷ���ڣ�һ���ھ����޴���ڵ�
#define   CLUSTER_GATEWAY                         	2		// �ؽڵ����ͣ����س�Ա��������ͷ���ڣ�һ���ھ����д���ڵ�
#define   CLUSTER_HEADER                         	3		// �ؽڵ����ͣ���ͷ���ڵ�����ID��С��

/*----------------------------------------------------------------------
				     		ZRP<->CLUSTER
----------------------------------------------------------------------*/
//#define     ZRP_TO_CLUSTER_NUM                   5		// ZRP���ɵ�Cluster�Ľڵ������
//#define     CLUSTER_TO_ZRP_NUM                   3		// Cluster���ɵ�ZRP�Ľڵ������

/*----------------------------------------------------------------------
				     		APP_BUFF		
----------------------------------------------------------------------*/
/* �趨��APP_BUFF����ش�С����Ѱ·�ڼ䣬��������ͱ��ġ�ֻ��Դ�˻��棬�м�ڵ���·������ֱ�Ӷ�������ˣ���Ҫ����ip���� */
#define		BUFF_CACHE_TIMEOUT					60000		// ���ڵȴ�Ѱ·��ҵ������APP_BUFF�ڵĻ���ʱ�䣬������ʱ�䣬��ɾ����������UDP����
#define		MAX_APP_LIST_SIZE					1000	// BUFF�������1000���洢��Ԫ,����1000��������ı��Ĳ�����
#define		BUFF_ITEM_SIZE						1600	// ÿ���洢��Ԫ1600�ֽڣ���С�����ҵ���ĳ��ȣ�

typedef struct{
	
	unsigned long	record_time;	// ��¼ʱ�䣬�����ʱ�䡣ÿ�α���ʱ�Ƚϸ�ֵ�뵱ǰʱ�䣬����ֵ����BUFF_CACHE_TIMEOUT����ɾ��
	unsigned short	dest_ID;		// �����ĵ�Ŀ��ID���������ʱȷ�ϸñ����Ƿ����Ѱ·����·�ɱ����Ƿ���
	unsigned short	app_len;		// �����ĵĳ���
	unsigned char	app_pk[BUFF_ITEM_SIZE];		// ����·��ͷ��ԴIPͷ��ҵ���ģ�·��ͷΪ��Чֵ��Ѱ·֮��Ҫ���¸�ֵ
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
	unsigned short	ID;			// �豸ID = x.y��ȡֵ1~65534��Ψһʶ��һ�������豸��iBox���߶�IP��ַΪ10.x.y.0������255.0.0.0; ���߶�IPΪ10.x.y.1������255.255.255.0
	
	unsigned char	cluster_state;		//·��ģʽ�� �ִ�״̬��0ΪZRPƽ��·�ɣ�1Ϊ��ͨ�س�Ա��2Ϊ���أ�3Ϊ��ͷ��
	unsigned char	cluster_size;		// �ش�С�������г�Ա��Ŀ�����ִ�ʱ��Ч��ZRPʱ��Ϊ0
	unsigned short	cluster_header;		// ��ͷID�����ִ�ʱ��Ч��ZRPʱ��Ϊ0xffff
	
	unsigned char	node_qos;			// �ڵ�������Ϊ���ط�������,��������ԽС����Ӧ��qosԽС.��·��ѡ��ʱ�������м�ڵ����Сnode_qos����Ϊpath_QOS��path_QOSԽ��Խ�á��ݲ��ã���Ϊ1
	unsigned char	IARP_radius;		//��뾶�� ����LSA_PKʱ�������͸ð뾶���ڵ��ھӽڵ㡣ʵ����뾶Ϊ��ֵ+1�������ֵ=1ʱ��LSA��Я���Լ����Լ���һ���ھӣ���ʵ���Ͻڵ���Ի��2���ھ���Ϣ
	
	unsigned char	degree;				// �ڵ�ȣ�˫��һ���ھӸ���������ִرȽϺ�LSA���͡�
								
	unsigned long	rcvd_pk_num;		// �����߶��յ���ҵ���ĵĸ���
	unsigned long	send_pk_num;		// �������߶˵�ҵ���ĵĸ���
	unsigned long	rcvd_pk_size;		// �����߶��յ���ҵ���ĵ����ֽ�����app_len
	unsigned long	send_pk_size;		// �������߶˵�ҵ���ĵ����ֽ����� app_len

	
	}NODE_ATTR;			/* �ڵ����ԣ����ڱ���ڵ���Ծ�̬�Ͷ�̬����*/


typedef struct{
	unsigned short	node_ID;			// �ڵ�ID�����ھӽڵ�ID
	unsigned char	path_QOS;			// �ڵ�������LSA������send_ID��node_ID����·������	
	unsigned char	mpr_distance;		// ����ֶΣ�bit7���Ƿ�ΪLSA���ķ����ߵ�MPR�ڵ㣬0���ǣ�1��,ֻ��һ���ھӲ��п�����MPR��
                                        //           bit6: �Ƿ�ΪLSA���ķ����ߵ�˫���ھӣ�0���ǣ�1�ǡ�ֻ��һ���ھӲ��п�����1
										//           bit3~0:��LSA���ķ����ߵ���������ʾΪ�����ھ�

	}NB_INFO;			/* �ھ��б���Ϣ��������䱨������,���ھӱ��ж�ȡ*/		
	
typedef struct{
	unsigned short	rreq_src_ID;			// ����RREQ��ԴID
	unsigned short	rreq_dest_ID;		// RREQ��Ӧ��ڵ�ID����RREQ��Ŀ��ID
	unsigned short	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��Ѱ·���̣���dest_ID�޹ء����һ��src_ID+rreq_seq,һ���ڵ�ֻ��Ӧһ�Σ����Ǻ�����path_cost��С
				
	unsigned char	path_QOS;		// ·���������Ƚϱ�����ԭֵ�뱾��node_qos��ѡ���С��һ���滻���ֶΣ�Դ�ڵ㷢��ʱ�ĳ�ֵΪ����node_qos���ݲ���
	unsigned char	hop;			// ��src_ID�����ص�����������Դ�ڵ㷢��RREQʱ��hop��Ϊ��ֵ1���Ժ�ÿ��ת������1
	unsigned short	trace_list[MAX_HOP];	// trace_list[0]Ϊsrc_ID, trace_list[hop-1]Ϊ���ͽڵ㣬������Ϊ���ͽڵ��MPR������ trace_list[hop]Ϊ����ID����������3ֵ�󷢳���
											//  ������Ϊdest_ID,���� trace_list[hop]Ϊ����ID��hop�� trace_list ���һ����ѡ·��	
	}RREQ_RECORD;		/* RREQ_PK_Rcvd() ���õ�����ֹ�ظ��㲥���ñ��ж�ͬһ����Դ��Ŀ�ģ��ڵ�ԣ�ֻά��һ��RREQ��¼������seq�����¾�*/
	

/*----------------------------------------------------------------------
				     		PK structure	
----------------------------------------------------------------------*/

/* ************LSA ���Ľṹ ***************** */

/**  ������ÿPERIOD_LSAʱ�䷢��һ��LSA_PK����ʵ���г�ͻ�϶࣬����һ��������������˱ܣ��˱�������Ϊ1ms */
#define		LSA_PK_HEAD_LEN			10
#define		LSA_PK_LEN(x)			(((x)<<2)+12)
	
typedef	struct {
	unsigned char	pk_type;			// �������ͣ�LSA��ʼ���ǹ㲥���ͣ����ǽ�����ֻ������ת��
	unsigned char	degree;				// �ڵ�ȣ���˫��ɴ��һ���ھӸ���
	unsigned short	send_ID;			// ���͸�LSA���ĵ�ID
	
	unsigned char	cluster_state;		// �ִ�״̬��0ΪZRPƽ��·�ɣ�1Ϊ��ͨ�س�Ա��2Ϊ���أ�3Ϊ��ͷ��
	unsigned char	cluster_size;		// �ش�С�������г�Ա��Ŀ�����ִ�ʱ��Ч��ZRPʱ��Ϊ0
	unsigned short	cluster_header;		// ��ͷID�����ִ�ʱ��Ч��ZRPʱ��Ϊ0xffff
	unsigned char	node_qos;			// �ڵ�������ͬNODE_ATTR���ݲ��ã���Ϊ1

	unsigned char	nb_num;				// �����ھӸ���,����N���ھӣ��������ô���
	NB_INFO			nb_list[MAX_NB_NUM];			// �����ھ��б�,�����250����ռ��1000�ֽ�
	
	unsigned short	pk_crc;				// LSA ���ĵ�У��͡�����nb_list���ȿɱ䣬0~1000�ֽڣ����ʵ�ʸ�pk_crc��ֵʱ���ڵ�ǰ���ĵ�ĩβ2�ֽ���ֵ
	}LSA_PK;

/* ************RREQ ���Ľṹ ***************** */
/**  ״̬���ʣ���û������·�ɣ��������Ѱ· */

#define		RREQ_PK_LEN			34

typedef struct{
	unsigned char	pk_type;		// �������ͣ�RREQ��ʼ���ǹ㲥���ͣ����ǽ����߻�����Լ�����mpr_list�ڣ��������Ƿ�ת��
	unsigned char	TTL;			// �㲥����ʱ�䣬���ƹ㲥��Χ��Դ����ΪMAX_HOP-1��ÿת��һ�εݼ�1����0ʱ���ٹ㲥ת��
	
	unsigned short	send_ID;		// ������ID���������м�ڵ��Դ�ڵ�
	unsigned short	rcv_ID;			// ������ID���������м�ڵ��Ŀ�Ľڵ�
	
	unsigned short	src_ID;			// ����RREQ��ԴID
	unsigned short	dest_ID;		// RREQ��Ӧ��ڵ�ID����RREQ��Ŀ��ID
	unsigned short	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��Ѱ·���̣���dest_ID�޹ء����һ��src_ID+rreq_seq,һ���ڵ�ֻ��Ӧһ�Σ����Ǻ�����path_cost��С
				
	unsigned char	path_QOS;		// ·���������Ƚϱ�����ԭֵ�뱾��node_qos��ѡ���С��һ���滻���ֶΣ�Դ�ڵ㷢��ʱ�ĳ�ֵΪ����node_qos���ݲ���
	unsigned char	hop;			// ��src_ID�����ص�����������Դ�ڵ㷢��RREQʱ��hop��Ϊ��ֵ1���Ժ�ÿ��ת������1
	
	unsigned short	trace_list[MAX_HOP];	// trace_list[0]Ϊsrc_ID, trace_list[hop-1]Ϊ���ͽڵ㣬������Ϊ���ͽڵ��MPR������ trace_list[hop]Ϊ����ID����������3ֵ�󷢳���
											//  ������Ϊdest_ID,���� trace_list[hop]Ϊ����ID��hop�� trace_list ���һ����ѡ·��

	unsigned short	pk_crc;					// ����RREQ���ĵ�CRC

	}RREQ_PK;
	


/* ************RREP ���Ľṹ ***************** */


/** һ��RREP��Ŀ�Ľڵ���ݲ�ͬ·�������RREQ����ѡһ�����з��͡���trace_list�з�����ȡrcv_ID�ֶ�*/

#define		RREP_PK_LEN		30	
typedef struct{
	unsigned char	pk_type;		// �������ͣ�RREP���������ͣ�rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
	unsigned char	reserved;		// ����ֽڣ�2�ֽڶ���
	
	unsigned short	send_ID;		// ������ID���м�ڵ�ID��dest_ID
	unsigned short	rcv_ID;			// ������ID���м�ڵ�ID��src_ID; �յ�֮����trace_list������Ѱ����rcv_ID									
		
	unsigned short	rreq_src_ID;	// ����RREQ��ԴID��Ҳ�ǻظ�RREP��Ŀ��ID
	unsigned short	rreq_dest_ID;	// ����RREQ��Ŀ��ID��Ҳ��RREP��ԴID	
	unsigned short	rreq_seq;		// RREQ��ţ���src_IDһ��Ψһʶ��һ��·�ɽ�������ڱ�ʶ·�ɵ��¾ɡ�

	unsigned char	path_QOS;		// ·���������Ƚϱ�����ԭֵ�뱾��node_qos��ѡ���С��һ���滻���ֶΣ�Դ�ڵ㷢��ʱ�ĳ�ֵΪ����node_qos���ݲ���
	unsigned char	hop;			// ��src_ID��dest_ID������
	
	unsigned short	trace_list[MAX_HOP];	// ��src_ID��dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID

	unsigned short	pk_crc;			// ����RREP���ĵ�CRC

	}RREP_PK;


/* ************RERR ���Ľṹ ***************** */

/** ����APP������trace_list��ȡ������һ��ID���ɴ�ʱ����������RERR
 *  ��LSA������ʧ�����ᴥ��RERR������ͳ��˱�����·���ˡ����ԣ��м�ڵ㲻����·�ɼ�¼��Ҳ������νɾ������*/

#define		RERR_PK_LEN			38

typedef struct{
	unsigned char	pk_type;		// �������ͣ�RERR���������ͣ�rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
	unsigned char	reserved;		// ��䣬�ýṹ��2�ֽڶ���
	
	unsigned short	send_ID;		// ������ID���м�ڵ�ID
	unsigned short	rcv_ID;			// ������ID���м�ڵ�ID; �յ�֮����trace_list������Ѱ����rcv_ID��src_ID�յ�����·�ɼ�¼�е�rreq_seq��ͬ���������Ϊ���µ�seq����·������һ����ͨ�����·�;�����ڣ�����
			
	unsigned short	src_ID;			// ����·�����ѣ�������RERR���ĵĽڵ�
	unsigned short	dest_ID;		// RERR��Ŀ�Ľڵ㣬Ҳ����ҵ���ĵ�Դ�ڵ�
	
	unsigned short	rreq_src_ID;	// RREQԴ�ڵ�ID����Դ����rreq_seqһ��λһ��·�ɼ�¼����ҵ��ͷ����ȡ	
	unsigned short	rreq_seq;		// RREQ��ţ���ҵ��������ȡ��Դ�ڵ�ݴ�ɾ��IERP·�ɡ�
	
	unsigned char	err_state;		// ����״̬��0������·�����ѣ�DSR�е���һ�����ɴ�,LSA��ʧʱ���ٴ����������޷����ؼ��޸���1�����ֶ���·�������Ѿ����ؼ��޸�����·������hop �� trace_list�С�
	unsigned char	hop;			// ��src_ID��dest_ID����������δ�޸�֮ǰ����ֵΪ��ֵ���������˱����޸������ֵ��Ϊ��ֵ����trace_listҲ���¡�
	unsigned short	trace_list[MAX_HOP];	// ��src_ID��dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID
	
	unsigned short	err_up_ID;		// ����·��������ID = src_ID���˴��ظ������Ϊ��������ȷ
	unsigned short	err_down_ID;	// ����·��������ID
	unsigned short	err_repair_ID;	// �޸�·����ID��������״̬Ϊ0ʱ����ֵΪ��Чֵ0xffff
											
	unsigned short	pk_crc;			// ����RERR���ĵ�CRC

	}RERR_PK;


	
/* ************APP ���Ľṹ ***************** */
#define			ROUTE_HEAD_LEN		38
#define			MTU_LEN				1500

typedef struct{
	unsigned char	pk_type;		// �������ͣ�����LSA��RREQ��RREP��RERR��DATA���˴������ಥ����������������net���ֶಥ
	unsigned char	reserved;		// ��䣬�ýṹ��2�ֽڶ���
	
	unsigned short	send_ID;		// ������ID���������м�ڵ��Դ�ڵ�
	unsigned short	rcv_ID;			// ������ID���������м�ڵ��Ŀ�Ľڵ�	
	
	unsigned short	src_ID;			// Դ�ڵ�ID
	unsigned short	dest_ID;		// Ŀ�Ľڵ�ID
	
	unsigned short	rreq_src_ID;	// RREQԴ�ڵ�ID����rreq_seqһ��λһ��·�ɼ�¼
	unsigned short	rreq_seq;		// ��Ϊ0�����ʾ����·�ɣ�������Ҳ��������·�ɱ��ʹ��ʱ�䣬�������ɾ����
									// ����0, ���ʾRREQ��ţ���trace_list����ȡ��һ������ɾ�����뱾�رȽϣ�����̫���� �����ڱ���·�ɼ�¼�е�rreq_seq��������·������ɵ�
	
	unsigned char	TTL;			// �㲥����ʱ�䣬���ƹ㲥��Χ��Դ����Ϊ6��ÿת��һ�εݼ�1����0ʱ���ٹ㲥ת��������6��			
	unsigned char	hop;			// ����ʱ��Ч��Ϊ��src_ID��dest_ID��������	
	unsigned short	trace_list[MAX_HOP];	// ����ʱ��Ч��Ϊsrc_ID-->dest_ID��·���б�trace_list[0]Ϊsrc_ID, trace_list[hop]Ϊdest_ID���м�ڵ�������ҵ���һ���ڵ㣬�������ν��Դ·��	
									
	unsigned short	bc_seq;			// �㲥����ţ���ֹ�㲥����ʱ�ĺ鷺��ʹ�����"��Ȧ"�㲥���ݡ����յ��㲥���ĺ󣬼�¼src��pk_seq���´����յ�С�ڼ�¼�Ĺ㲥���ģ�����TTL����ת��
	
	unsigned short	len;			// ����IP���ĵ��ܳ���
	unsigned short	head_crc;		// APP��ͷ��CRC
	
	unsigned char	data[MTU_LEN];	//����IP��������
	
	}APP_PK;



/*----------------------------------------------------------------------
				     		Table structure	
----------------------------------------------------------------------*/
	// �յ�LSA���ĺ�������ONE_HOP_NB_TAB�и��»����һ����¼����Ҫ��direction��������0��1��Ҳ������1��0
typedef struct{
	unsigned short	node_ID;			// һ���ھ�ID
		
	unsigned char	cluster_state;		// �ִ�״̬��0ΪZRPƽ��·�ɣ�1Ϊ��ͨ�س�Ա��2Ϊ���أ�3Ϊ��ͷ��
	unsigned char	cluster_size;		// �ش�С�������г�Ա��Ŀ�����ִ�ʱ��Ч��ZRPʱ��Ϊ0
	unsigned short	cluster_header;		// ��ͷID�����ִ�ʱ��Ч��ZRPʱ��Ϊ0xffff
		
	unsigned short	rcv_state;			// ����ͳ�ƣ���ֵΪ0xff00; ÿ�η���LSA֮ǰ�����������ھӵ�rcv_qos���������ϴν���ʱ���>PERIOD+50,ֱ������1λ��������λ��+1��
	unsigned char	rcv_qos;			// ���������� rcv_stateͳ�ơ�1���ĸ����õ�
	unsigned char	send_qos;			// �����������ɶԶ˵�rcv_stateͳ�ơ�1���ĸ����õ���
	
	unsigned char	path_QOS;			// ��·������ ÿ�η��ͺͽ���LSAʱ�����£�ȡֵ = min{���ؽ���LSA�����ȵ�ͳ�ƣ��ھӽ���LSA�����ȵ�ͳ��ֵ}
	unsigned char	degree;				// �ڵ�ȣ���˫��ɴ��һ���ھӸ���

	unsigned char	direction;			// ��·����0��ֻ���ղ��ܷ�������ͨ �����ھ�LSA�е��ھ��б���û���Լ�����
										//       1��˫���ͨ�����ھ�LSA�е��ھ��б������Լ���

	unsigned char	is_my_mpr;			// 1��node_ID�Ǳ��ڵ��MPR��0�����ǡ�ͨ�����ؼ���õ�
	unsigned char	is_your_mpr;		// 1: ���ڵ���node_ID��MPR��0�����ǡ�ͨ������LSA����ֱ�Ӹ�ֵ
	
	unsigned long long	last_lsa_rcv_time;		// ������ոýڵ�LSA��ʱ�䣬��ʱ����Ҫ��ONE_HOP_NB_TAB��LSA_PK_Tab��ɾ�����ھӽڵ�
	}ONE_HOP_NB_ITEM;
	
	// ֮�������ھ�Ϊ˫���ھӣ������LSA���ĸ���MULTI_HOP_NB_TAB�����ǵ����ھӣ�LSA�����е��ھӱ��в������Լ��ˣ�����ɾ������relay_ID��send_ID�ļ�¼;
	//       ����������һ�����·���ڵ�relay_ID���ѣ�������ɸ�����ص�����2���ھ���ʱ��ʧ(���·������һ��Ψһ)����һ�����ں��ָ�(�ڸ���hop�������ҵ���·��)��
	//			[ע]:ֱ�ӽ�ȡLSA���ĵ�nb_list����ʱ������ֵ���ͬһ��Ŀ��ID��������ͬ��ͬ��relay_id��������ͬ��ͬ�������Ĳ�ͬ��Ŀ�����뱾��ʱ������ͬһ��dest_ID��ֻ����hop��С����Կ����ж��������relay_id��ͬ��
	//				ͬʱ���ʱ���������У�����hop����2,3,4...��ͬһ���������£�node_id֮�䲻���򣬵���ͬnode_id����ĿҪ���ڡ���������������֪����ͬnode_id����Ŀ��hop�ض���ͬ
	//       �����˷�ʽ�����Ķ����ھӱ���һ���ھӱ�һ�𣬿���Ϊ����·�ɱ�
typedef	struct {
	unsigned short	node_ID;			// ��LSA���ĵ�nb_list����ȡ��ID���Ǳ��صĶ����ھ�
	unsigned short	relay_ID;			// �м�ID��Ϊ���ص�˫��һ���ھӣ�Ҳ���յ���LSA���ĵķ�����		
	unsigned char	path_QOS;			// ��·�������յ�LSA���ĺ��ȼ��㱾�ص�send_ID��QOS�����뱨����Я����nb_list[]->path_QOS�Ƚϣ�ȡ��Сֵ	
	unsigned char	hop;				// �ӱ��ص���node_ID���������ض���2��������

	}MULTI_HOP_NB_ITEM;
	
	// �����ھӵ�LSA����������ھ��б��������һ���ھ���˭���ҵ�MPR��Ȼ������LSA���ھ��б���ͨ���ȥ���ҵ��ھ��յ��󣬾���֪�����ǲ����ҵ�MPR�����ҹ㲥����ʱ��ֻ���ҵ�MPR��ת����
	// ���һ�����ݽṹ����ȡ��һ���ڵ�˫���ھӣ�����˫���ھӣ������˫�������ھӣ���Ȼ����������������ھӲ�������Сһ���ھӼ����õ���һ���ھӼ���Ϊ�ҵ�MPR�б�
	// ÿ��һ���������ھӷ����仯����Ҫ���¼���MPR����ˣ����Լ򵥵���ÿ���յ�LSAʱ���¼���MPR��������ONE_HOP_NB_TAB

	// ֮�󣬸���LSA�����б��������·�ɱ�dest_ID������1,2������3,4�����ھӡ�
/*
typedef struct{
	unsigned short	dest_ID;		// ���ڽڵ�ID
	unsigned short	next_ID;		// ��һ��ID���������м̵�һ���ھӽڵ�
	unsigned char	path_QOS;		// ��·����	
	unsigned char	hop;			// ���ڵ㵽��dest_ID������	
	}IARP_ITEM;
*/	
	
	// IERP�м�¼��·����Ŀ��Դ��ַһ�����Լ�������Ŀ����ֱ�ӽ������ϼ�������RREP���ƽ��������ǽ�ȡһ��trace_list��������ġ�
	// ������β����漰һ���ھӵ�ɾ������˫ͨת��ͨ������������������·�ɱ���next_IDΪ���ھӵ���Ŀȫ��ɾ����
	// �м�ڵ㲻���¼��ʼ��ַ�����Լ���IERP·����Ŀ����ˣ����յ�LSA�����ھӶ���ʱ��Ҳ��������ν��RREQԴ�ڵ㷢RERR��
	//        ֻ�����´η���ҵ����ʱ������·���ѣ�������ҵ��ͷ�ڵ���Ϣ������RERR
typedef struct{
	unsigned short	rreq_src_ID;	// �������ķ���RREQ��ԴID��Ҳ��RREP�����е�rreq_src_ID
	unsigned short	rreq_seq;		// RREQ��ţ���rreq_src_IDһ��Ψһʶ��һ��·�ɽ�������ڱ�ʶ·�ɵ���Դ���¾ɡ�
	
	unsigned short	dest_ID;		// ����һ����RREQ�е�Ŀ��ID���п�����������ȡ��ĳ���м�ڵ���ΪĿ��ID
	unsigned short	next_ID;		// ��һ��ID����ҪΪ�˷����ھӶ���ʱ��ɾ������Ŀ
	
	unsigned char	path_QOS;		// ��·������ֱ��Ѱ·��Դ��Ŀ�Ķ˿��Ի�֪path_QOS,���м�ڵ��޷�׼ȷ��֪��ֻ�ܴ��֪��������ڸ�ֵ
									//       ��ˣ����Ѱ·ʱ���ο�path_QOS�����ԣ���Ѱ·��Ŀ�Ķ����ӳٵȴ�����ѡһ��·���ظ�RREP����Ϊ��Ҫ
	unsigned char	hop;			// ����dest_ID������
	unsigned short	trace_list[MAX_HOP];		// ����ID-->dest_ID��·���б�trace_list[0]Ϊ����ID, trace_list[hop]Ϊdest_ID��
	
	unsigned long   last_use_time;	// ���һ��ʹ��dest-->src����src-->dest��ʱ�䡣��ʱδʹ��ʱ����Ҫɾ������·��
	
	}IERP_ITEM;


/* ����seq������ȡֵΪ1~65535,0��������ʾ�������⺬��*/
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
extern NODE_ATTR			Node_Attr;		// ���ؽڵ����ԣ�����ID��·��״̬���շ�ͳ�Ƶ�

extern unsigned char 		LSA_Flag;		// LSA ���Ϳ��أ�Ϊ0�رգ�Ϊ1��

extern unsigned int    RADIUS;                 // ��뾶��
extern unsigned int	   CLUSTER_TO_ZRP_NUM;		// Cluster���ɵ�ZRP�Ľڵ������  3
extern unsigned int	   ZRP_TO_CLUSTER_NUM;		// ZRP���ɵ�Cluster�Ľڵ������ 5
extern unsigned int    RETRY_TIMES;             // LSA��ʱʱ��
extern unsigned int    PERIOD_LSA;              //LSA ��������
extern unsigned int    IERP_RREQ_TIMEOUT;       // RREQ ��ʱʱ��
#endif
