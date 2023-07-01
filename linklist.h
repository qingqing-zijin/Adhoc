/*linklist头文件*/

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

extern List*	prg_list_create();									//建立空链表
extern int 		prg_list_size(List *L); 							//链表元素个数
extern void*	prg_list_access(List *L,int list_index);			//链表访问,查找第i个元素，返回链表节点中的元素指针
extern int 		prg_list_insert(List *L,void *x,int list_index);	//输入参数为3个，则按指定位置插入
extern int 		prg_list_remove(List *L,int list_index);			//移除元素并清除元素和链表节点内存
extern int 		prg_list_delete(List *L);							//删除整个链表，并清空所有申请的内存

#endif