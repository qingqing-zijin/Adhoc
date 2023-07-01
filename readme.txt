2015-02-04
	此版本未写完整，其中RREQ_Seq是否需要在路由表和路由头中出现，以及其意义，出现思路分歧。
	另，由于网络负载的易变性，即使在寻路之初把各跳传输速率，时延等量化、比较，几秒或若干时间后，仍会发生变化。因此，确定不在域间寻路中使用path_cost；
	同理，域内路由中，虽可维护，但当出现与hop优先级冲突时，仍不好处理。因此，域内路由内也暂不使用path_cost。
	
	因此，另辟新版本。

2015-02-05
	由于思路分歧，另开新版本：
		1. 先不做DSR旁听路由。以后若做，可以做RREQ，RREP，RERR报文和业务报文的旁听
		2. 先不做DSR本地修复，全由原端去做。
	同时，新版本中明确若干事项：
		1. rreq_src_id与rreq_seq的定义：RREP/RERR/APP内都加入了这两个字段，唯一识别一条域间路由；同时RREP内还增加rreq_dest_id，因为RREQ记录表不能仅根据rreq_src_ID与seq区分，
			还要根据rreq_dest_ID,因为一个节点肯定会向多个目的寻路，而不同目的ID的寻路过程，仅比较seq不能确定新旧.
		2. path_QOS重新定义为链路最小发送速率，其值越大越好；node_cost更名为node_qos，表示本地发送速率；彻底放弃path_cost。
		3. RREP报文反馈机制重定义：RREQ的中间节点收到相同src_id和rreq_seq的更好的路径（跳数小？qos大？），则转发，否则不转发。
					   RREQ的目的节点收到RREQ后，不会作超时发送RREP的操作，但是会比较新RREQ与RREQ记录表，若新收到的src_id和rreq_seq相同的RREQ中路径更好，则返回									RREP，并更新域间路由表和RREQ记录表 
		4. Routing模块内的子功能函数尽量加入可能访问的链表的指针，方便独立测试。

2015-02-11
	完成linklist库
	完成Routing模块中除了MPR算法以外的代码

2015-03-05
	完成MPR算法及测试

2015-03-06
	增加LOG_MSG函数，实现多等级输出控制

2015-03-10
	添加测试函数，完成若干报文的收发流程测试；
	添加报文收发及处理过程中的必要输出(DBG_LEVEL)，方便调试。

2015-03-12
	完成辅助函数，并与其他模块集成。
	暂时屏蔽LSA发送和APP_BUFF 缓存。
	
2015-04-02
	LAN口raw socket 过滤器设置。组播地址当广播处理，dest-id置为0xffff.
	CRC 算法优化。
	路由及节点信息上报。
	梳理LOG输出，APP相关为DBG_LEVEL，路由控制报文为INFO_LEVEL，错误信息直接输出，Print_xx 函数为ERR_LEVEL。
	修正两处BUG：发送广播报文和RREQ报文（也是广播）时，发送者本身也应该先将其加入广播序号表或RREQ序号表，防止重复转发。
	修改Is_yourMPR() ，为了适应DSR路由，当 radius = 0时，应始终返回1.
	增加可设置变量 LSA_Flag，为1打开，为0关闭。 暂时不用
	修改 Is_1hopNb() --- 判断id是否为自身的一跳邻居，是双向邻居返回2，单向邻居返回1，不是邻居返回0。
	注意：寻路和判断路径失效时，不需判双向，是邻居即可。这也是为了适应radius= 0 的情况
	
2015-04-08
	MPR算法中，当两跳邻居数为0时，特殊处理，将所有一跳邻居的my_mpr项置0.
	寻路时，必须判定为双向邻居，因为在规划路由表时，若一跳非双向可达时，就会有多跳路由项。
	当radius = 0时，必定不存在域内路由，全是 域间路由，即IERP。则route_search中判双向邻居对其无影响，主要是判路径失效。
	判路径失效时，若radius=0,则是邻居即可，否则，必须是双向邻居。
	route_search中有一处BUG，每次while 前为给i重新赋值。
	
2015-04-09
	perror全部替换为LOG_MSG(ERR_LEVEL)
	增加校验和计算和Is_myPing函数，收到发给本地的ping_request，则通过APP_PK_Send返回reply.
	修改Report_Table，只报双向邻居。
	修正ethsend的BUG，1）daddr赋值错误。2) sock要置SO_BROADCAST 为1。
	
2015-04-10
	找到一个导致程序崩溃的大BUG：给终端上报时，只报双向邻居，但是所有情况都应i++。加if条件时，忘记将该行提出来了。
	修改LOG_MSG. 增加文件输出。当Log_Level是正值，出文件；负值，出串口。
	LSA超时6次才删除，2秒一包。
	LSA发送时，2s + 32ms(随机)。
	
2015-04-13
	重大改动：重定义path_QOS，为链路上最小双向接收质量，0~16；根据LSA报文接收情况进行统计。
			改动涉及LSA报文的nb_list，及收发处理；一跳邻居表，多跳、IERP的path_QOS的赋值方法；Route_Search算法。
			另外，还改动了Is_1hopNb，Is_yourMPR,Get_MprDistance函数定义，各携带出1项必要信息。
	修改LOG——MSG时间输出
	修改LSA发送：注意，有时出现了系统内IPC消息丢失。重启后OK。以后观察。
	RREQ_TimeoutHandler也该为msgsnd, 防止Is_Routing变量死锁。本质原因是收到RREP后，未解锁。修改BUG
	修改IERP中HOP的打印和上报，-1上报。
	修改main.c, 增加log配置文件，开机重设log文件名，最多保存16个文件。
	rcv_state初值设为0xaaaa;
	修正getAllIfconfig中指针溢出的BUG。
    修改Update_Nb_Tab的BUG。当邻居变成双向邻居时，除了下一跳，还要在多跳、IERP内删除相同目的ID的项，同时更新多跳表后，还要删除相同目的ID的IERP项。
    修改Del_Timeout_1hopNb的BUG，必须先删多跳、IERP相关项，再删1跳邻居项。
    
2015-04-15
    修改转发RREQ的BUG。当网络工作一段时间后，RREQ序号累加到100左右，源端重启，RREQ号清0，导致很长一段时间不转发。RREQ号不等就转发，BC号相差32以后也转发。
    重新梳理打印信息。
    目前域间节点寻路，还是要各寻一次，以便生成两个RREQ_Record,中间链路断裂后，RERR朝一个方向发送。否则，RERR必须朝APP的源节点发送(rreq_src仍需保存携带)
    
2015-04-17
    LSA 周期改为1秒；RREQ超时改为2秒。 rcv_state 初值改为0x0020。必须在接下来的10秒内收到4次，。
    增加邻居丢失超时时间，RETRY_TIMES = 7
    增加routeCfg.ini 配置文件的操作。
    增加邻居链路稳定性，收到新节点LSA时，置rcv_state 初值改为0x0020，在接下来的9秒内收到4次LSA，且dir和qos同时满足要求，才认为邻居建立。
    而删除链路时，仍仅以连续RETRY_TIMES未收到LSA为依据。但仍存在第三种情况：链路从双向转为单向，收到LSA报文即可触发，其稳定性由对端保证
    
2015-04-21
    修改一个BUG，Set_Mpr_By_Mask中应注意扣除单向邻居后，再根据MASK赋值。
    
2015-04-22
    修改一个关于广播的BUG：
        当从不稳定链(qos低)或单向链收到了两跳或两跳外的节点的RREQ，直接更新SEQ再转发，导致我的MPR转发来的RREQ反而不转，寻出的路不对。
        修改： 收到RREQ包，必须是其双向邻居，才处理（转发或反馈RREP，并记录）.
        
        但是收到广播APP包则不行，否则一跳内广播岂不是收不到了。
        而原来的做法也不行（所有上报PC仅靠序号，转发则MPR，TTL，SEQ都看，且转发后才更新SEQ）。这会导致一个问题：
            在转发之前（收到非MPR节点转发的包），可能连续收到几次相同的广播包，必须靠应用层来屏蔽。
        修改：增加Bcast_ForwardSeq_Record[]和Bcast_ReportSeq_Record[]，区分是否上报PC和是否转发。
            
2015-04-23
    BUG: 由于原LSA报文中不能区分邻居与邻居的邻居是单向链还是双向链，导致本地对两跳节点的连接关系认知错误。
    修改：在mpr_distance中增加direction比特位，主要用于distance == 1时的判定。
    
2015-05-04
    修改main，增加“-bg”参数，后台启动时，不开启cmd线程
    修改aux, 获取系统时间时，做一判定。系统启动时，时间为2000年1月1日，Debian用户登录后，将系统时间更新为2014年4月24日。
    所以要在程序中间判定，变为2014年后，给starttime重赋值
    
2015-05-07
   修改LOG_MSG，每次都打开关闭log文件，低效，但或许可以避免文件出错。以观后效。
   
2015-05-08
   修改Route.c，主要是簇状态的判决与更新，并增加了一个函数，由LSA_PK_Send调用
   
2015-05-12
    Node_Attr.degree改为邻居数加1，含自己
    分簇与合并时，只看ID，不看节点度了。簇头不相见
    LSA发送时本地更新簇状态，收到LSA时，更新簇状态。两者独立，针对不同情况。
    必须是本地双向一跳邻居的LSA才更新簇状态，防止抖动。
    
2015-05-15
    修改Route_Search,域间寻路时，找最佳（hop最小的中选QOS最大）的路径，而不只是QOS最大。
    修改RREQ转发时的BUG。不论是new 还是 TTL，MPR的原因，不转发则不更新RREQ_Tab，否则，紧跟着收到合适的RREQ也不转发，造成问题。
    
2015-05-21
    修改RREQ收到时的处理，若为目的端，则除了返回RREP，也记录逆向路径，rreq_seq为本地RREQ_Seq+1后的值
    修改Update_RREQ_Tab，特别添加加入本地节点后对path_QOS的影响，更新该值。这样接下来返回RREP才是正确的，而不是忽略最后一跳的结果。
    
2015-05-23
    删去了Is_myping,代之为彻底解决PC-->任意IP的互通。1#，2#可以远程控制任何一个WiBox，同样，其它节点可以通过设备串口
                    直接Ping通1#PC，2#PC。改动包括：
        修改网络传输基础部分。eth的send raw sock 不绑定。eth rcv raw socket 不绑定，且改为ETH_P_ALL。
        更改lan_rcv，先过滤非IP包，再过滤IP包。
        更改Lan_rcv_filter,按IP，协议，端口进行过滤。