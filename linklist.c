
//modified Version1

/*linklist.h
*define each data element struct
*define node struct in the linklist
*define list struct(contains a head,a tail pointer and length of the linklist)
*/
#include "linklist.h"
#include "mac.h"

/*prg_list_creat
  *function:creat an empty linklist;
  *input parameters:none;
  *return(List *): 
       NULL:creat linklist unsuccessfully;
       L:creat a linklist successfully and return the head pointer;
*/
List *prg_list_create()//����������
{
	List *L= (List *)malloc(sizeof(List));
	if(L==NULL)
		return NULL;
	L->head=NULL;
	L->tail=NULL;
	L->length=0;
	return L;
}

/*prg_list_size
  *function:obtain the number of all elements in the linklist;
  *input parameters:
       (List *)L:the head pointer of the linklist;
  *return(int): 
        L->length:the number of all elements in the non-empty linklist;
*/
int prg_list_size(List *L)//����Ԫ�ظ���
{
	if(L == NULL)
		return 0;
	else
		return L->length;
}



/*prg_list_find
  *function:access the element with an assigned index in the linklist;
  *input parameters:
      (List *)L:the head pointer of the linklist;
      (int)list_index:the assigned location index of element to be accessed in the linklist. 
  *return(LNode *):
      NULL:the input index is unsuitable and the element can not ne accessed.
      p:return the pointer that points to the accessed element
*/

LNode *prg_list_find(List *L,int list_index)//�������,���ҵ�i��Ԫ�أ���������ڵ�ָ��
{
	int j=1;
	LNode *p;
	if(L==NULL)
		return NULL;
		
	p = L->head;
	
	if(p==NULL)//list is empty 
		return NULL;
	else if((list_index<0)||(list_index>L->length))
		return NULL;
	else if (list_index==LIST_POS_HEAD) 
		return L->head;
	else if(list_index==LIST_POS_TAIL) 
		return L->tail;
	else
	{	
		while( j<list_index)
        {
        	if (p->next!=NULL)
        	{
				p=p->next;
				j++;	
			}
			else
			{
				return NULL;
			}
		}	
		return p;
	}
	
}

/*prg_list_access
  *function:access the element with an assigned index in the linklist;
  *input parameters:
      (List *)L:the head pointer of the linklist;
      (int)list_index:the assigned location index of element to be accessed in the linklist. 
  *return(LNode *):
      NULL:the input index is unsuitable and the element can not ne accessed.
      p->elem:return the pointer that points to the accessed element's data member
*/
void *prg_list_access(List *L,int list_index)//�������,���ҵ�i��Ԫ�أ���������ڵ��е�Ԫ��ָ��
{
	//pthread_mutex_lock(&mac_LinkList_mutex);
	LNode *p=prg_list_find(L,list_index);
	if(p==NULL)
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return NULL;
	}
	else
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return p->elem;
	}

}


/*prg_list_insert
  function:insert an element with the assigned location index to the linklist;
  input parameters:
       (List*)L:the head pointer of the linklist;
       (Data*)x:the pointer that points to the inserted data;
	(int)list_index:the assigned location index of the linklist;
        *when list_index==1,insert to the head of the list;
        *when list_index==0 or (length of list +1),then insert to the tail of the list;
        *when list_index is other interger, then insert the element according to it.
  return(int):0:insert unsuccessfully.
              1:insert successfully.
*/
int prg_list_insert(List *L,void *x,int list_index)//�������Ϊ3������ָ��λ�ò���
{

	LNode* s;
	LNode* pre=NULL;
	

	//pthread_mutex_lock(&mac_LinkList_mutex);

	if((list_index<0)||(x==NULL)||(list_index>L->length+1))
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 0;
	}
		

	s=(LNode*)malloc(sizeof(LNode));
	if(s == NULL)
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 0;
	}

	
	s->elem=x;
	s->next=NULL;
	
	if(L->head==NULL)//�ձ���ֱ�Ӳ嵽β��
	{
		L->head=s;
		L->tail=s;
		L->length++;//������
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 1;
	}
	else if((list_index==L->length+1)||(list_index==LIST_POS_TAIL))//β������
	{
		L->tail->next=s;
		L->tail=s;
		L->length++;
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 1;
	}
	else if(list_index==LIST_POS_HEAD)//ͷ������
	{
		s->next=L->head;
		L->head=s;
		L->length++;
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 1;
	}
	else//����βλ�ò���
	{
		pre=prg_list_find(L,list_index-1);
		if(pre==NULL)//����ʧ��
		{
			free(s->elem);
			free(s);
			//pthread_mutex_unlock(&mac_LinkList_mutex);
			return 0;
		}
		s->next=pre->next;
		pre->next=s;
		L->length++;//������

		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 1;
	}
}

/*prg_list_remove
  function:remove the element with the assigned location index from the linklist.
  input parameters:
       (List *)L:the head pointer of the linklist;
       (int)list_index:the assigned location index of the element to be removed from the linklist; 
  return(int):
       0:the input index is unsuitable and remove unsuccessfully.
       1:the element with the assigned index has been removed successfully.
*/
int prg_list_remove(List *L,int list_index)//�Ƴ�Ԫ�أ�ע������ڴ�
{
	LNode *p;
	LNode *s;
	//pthread_mutex_lock(&mac_LinkList_mutex);
	if (list_index<0 || L == NULL)
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 0;
	}
	else if(list_index>L->length)
	{
		//pthread_mutex_unlock(&mac_LinkList_mutex);
		return 0;
	}

	if(L->length==1&&list_index==1)//ɾ������һ���ڵ������
	{
		s=L->head;
		L->head=s->next;
		free(s->elem);//free����
		free(s);
		L->tail=NULL;
		L->head=NULL;
		L->length--;
	}
	else if(list_index==LIST_POS_HEAD)//L->length����1
	{
		s=L->head;
		L->head=L->head->next;
		free(s->elem);//free����
		free(s);

		L->length--;
	}
	else if((list_index==L->length)||(list_index==LIST_POS_TAIL))
	{
		p=prg_list_find(L,list_index-1);
		s=p->next;//sָ���i�����
		L->tail=p;
		p->next=NULL;
		
		free(s->elem);//free����
		free(s);

		L->length--;
	}
	else//�Ƴ���Ԫ�ز�����βԪ�أ�������ı�������βָ��
	{
		p=prg_list_find(L,list_index-1);
		if(p==NULL)
			return 0;
		s=p->next;//sָ���i�����
		p->next=s->next;
		free(s->elem);
		free(s);
		L->length--;//���ݼ�
	}
	//pthread_mutex_unlock(&mac_LinkList_mutex);
	return 1;
}

/*prg_list_delete
  function:delete the linklist.
  input parameters:
      (List *)L:the head pointer of the linklist;
  return(int):
      0:delete theunsuccessfully.
      1:the element with the assigned index has been removed successfully.
*/
int prg_list_delete(List *L)//ɾ������ע���������������ڴ�
{
	if(L==NULL)
		return 0;
	//pthread_mutex_lock(&mac_LinkList_mutex);
	while(prg_list_remove(L,LIST_POS_HEAD));
	free(L);
	//pthread_mutex_unlock(&mac_LinkList_mutex);
	return 1;	
}
 
