/*linklistͷ�ļ�*/

#ifndef LINKLIST_H
#define LINKLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#define		LIST_POS_HEAD		1
#define		LIST_POS_TAIL		0


/**define node struct in the linklist
struct members:
	Data *elem:data pointer of listnode,point to the data struct.
    struct listnode *next:next pointer of listnode,point to the next node in linklist
*/
typedef struct listnode
{
	void *elem;// data pointer of listnode
	struct listnode *next;//next pointer
}LNode;

/**define list struct which contains a head,a tail pointer and length of the linklist
struct members:
    struct listnode *head:head pointer of the linklist,point to the first node in linklist.
	struct listnode *tail:tail pointer of the linklist,point to the last node in linklist.
	length:the number of elements in the linklist/the length of the linklist.
*/
typedef struct list
{
    struct listnode *head; //pointer points to the first node in the linklist
	struct listnode *tail;//pointer points to the last node in the linklist
	int length;//the number of elements in the linklist
}List;

extern List*	prg_list_create();									//����������
extern int 		prg_list_size(List *L); 							//����Ԫ�ظ���
extern void*	prg_list_access(List *L,int list_index);			//�������,���ҵ�i��Ԫ�أ���������ڵ��е�Ԫ��ָ��
extern int 		prg_list_insert(List *L,void *x,int list_index);	//�������Ϊ3������ָ��λ�ò���
extern int 		prg_list_remove(List *L,int list_index);			//�Ƴ�Ԫ�ز����Ԫ�غ�����ڵ��ڴ�
extern int 		prg_list_delete(List *L);							//ɾ�������������������������ڴ�

#endif