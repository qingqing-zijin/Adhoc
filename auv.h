/*
 * auv.h
 *
 *  Created on: May 17, 2018
 *      Author: root
 */

#ifndef AUV_H_
#define AUV_H_

typedef unsigned char 	u8;
typedef unsigned int 	u32;

typedef enum{
	USV1=1,
	AUV5=5,
	AUV6=6
}node_type_t;

int nsv_sendto_auv_usv(node_type_t node_type ,u8* buffer , u32 len);
int nsv_network_init();
void nsv_NodeLinks_TableReport();
int node_relay_auv_pkt(	unsigned char	*IP_pkptr);
#endif /* AUV_H_ */
