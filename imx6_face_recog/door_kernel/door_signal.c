/***implement door signal**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "door_signal.h"
#include "door_info.h"
#include "taskpool.h"
#include "door_media.h"
#include "door_network.h"
#include "door_log.h"
#include "platform_ioctl.h"
#include "door_elevator.h"


#define DOOR_DIAL_TIME               30000    //呼叫接听等待30秒
#define DOOR_TIMEOU                  120000   // 对讲，预览，留言120秒后自动关闭
 
#define DOOR_LOCKED_TIMEOUT          1000     // 门锁打开过1秒后自动锁上    

#define DOOR_TASK_NUMS        MAX_WORKER_NUMS


#define CHECK_CURRENT_REMOTE_IP(current, remote)                        \
do{                                                                      \
	if(strcmp(current, remote)){                                         \
		printf("WARNING: currentip=%s, remoteip=%s\n",current,remote);    \
	}                                                                     \
}while(0)

typedef struct g_door_data
{
	
	int            center_flg;      //如果与管理机通信时端口不一样  管理机是10000，其它10002
	int            timer_id;        //当前超时定时器
	DR_STATA_E     stata;           //当前门口机状态
	char  current_ip[32];           //当前通信对方的IP
	unsigned int  usr_data;          //当前通信的用户数据
	DOOR_CALLBACK  callback;        //当前用户回调, 对讲，留言使用
	DOOR_CALLBACK  lock_callback;   //门锁回调
	pthread_mutex_t mutex;
}G_DOOR_DATA_S;


#define CENTER_IP_EFFECTIVE_PEROID    86400    //管理机IP地址有效期为一天3600*24 ， 一天后重新获取。

#define DELAY_START_VIDEO        350//350  //延时350ms

static char g_center_ip[32] = {0};
static time_t g_center_ip_active = 0;
static G_DOOR_DATA_S g_door_data;
static DOOR_CALLBACK g_sys_callback = NULL;

static int g_sv_timer_id = -1;
static OPEN_DR_INFO_S  g_opend_info_tmp = {0};

static int delay_sv_callback(int timer_id, int interval)
{
	G_DOOR_DATA_S* pdata = &g_door_data;
	md_start_video_streaming(pdata->current_ip, NULL);
	g_sv_timer_id = -1;
	return 0;
}
//延时打开视频，如果立马打开，对方还没有准备好接收，第一个I帧会丢失
//所以要延时打开，等对方启动好接收线程。
static void delay_start_video(int delay)
{
	if(g_sv_timer_id >= 0){
		task_cancel_timer(g_sv_timer_id);
	}
	g_sv_timer_id = task_timer(delay, delay_sv_callback);
}

int generate_params( struct tcppacket** packet, DOOR_CMD_DT_S** cmd_dt)
{
	struct tcppacket* pmy = (struct tcppacket*)malloc(sizeof(struct tcppacket));
	if(!pmy)
	{
		printf("ERROR: malloc tcp packet fialed\n");
		//free(pParam);
		return -2;
	}
	memset(pmy, 0, sizeof(struct tcppacket));
	(*packet) = pmy;
	DOOR_CMD_DT_S* pcmd = (DOOR_CMD_DT_S*)malloc(sizeof(DOOR_CMD_DT_S));
	if(!pcmd)
	{
		printf("ERROR: malloc door cmd data fialed\n");
		//free(pParam);
		free(pmy);
		return -2;
	}
	memset(pcmd, 0, sizeof(DOOR_CMD_DT_S));
	(*cmd_dt) = pcmd;
	return 0;
	
}

void free_params(int process_ret, unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	struct tcppacket* pPacket = (struct tcppacket*)Data1;
	DOOR_CMD_DT_S* pCMDDt = (DOOR_CMD_DT_S*)Data2;
	if(pPacket){
		//printf("---pthread id = %u---   free pPacket\n", (unsigned int)pthread_self());
		free(pPacket);
	}
	if(pCMDDt){
		//printf("---pthread id = %u---   free pCMDDt\n", (unsigned int)pthread_self());
		free(pCMDDt);
	}
	
	return;
}

//应答呼叫
int door_answer(char* remote_ip, DOOR_CALLBACK callback);

static int send_cmd(struct tcppacket* pPacket, DOOR_CMD_DT_S* pCMDDt)
{
	assert(pPacket);
	assert(pCMDDt);
	pTASK_PARAM pTask = malloc_task_param();
	if(!pTask)
	{
		free_params(0,(unsigned int)pPacket, (unsigned int)pCMDDt, 0);
		printf("ERROR: malloc_task_param fialed\n");
		return -1;
	}
	pTask->Data1 = (unsigned int)pPacket;
	pTask->Data2 = (unsigned int)pCMDDt;
	pTask->Data3 = pCMDDt->reserve1;
	pTask->post_processing = (task_callback)free_params;
	pTask->processing = (task_processing)nw_sendtcpcom;
	put_task_by_param(pTask);
	return 0;
}

static int switch_door_stata(DR_STATA_E stata)
{
	int ret = 0;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	pthread_mutex_lock(&pDoorDt->mutex);
	if((door_idle != stata)&&(door_idle != pDoorDt->stata))
	{
		ret = -1;
	}else{
		pDoorDt->stata = stata;
		if(door_idle == stata){
			pDoorDt->stata = door_idle;
			pDoorDt->callback = NULL;
			pDoorDt->center_flg = 0;
			pDoorDt->usr_data = 0;
			strcpy(pDoorDt->current_ip, "");
		}
	}
	pthread_mutex_unlock(&pDoorDt->mutex);
	return ret;
}

static int notify_close()
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	if(door_idle == pDoorDt->stata){
		printf("INFO: nothing to close\n");
		return -1;
	}
	ret = generate_params(&pPacket, &pCMDDt);
	if(0 != ret){
		return -2;
	}
	pPacket->state = 0xfc;
	
	pCMDDt->port = di_get_server_port();
	if(1 == pDoorDt->center_flg){
		pCMDDt->port = di_get_center_port();
	}
	//printf("send close current ip=%d, port=%d\n",pDoorDt->current_ip, pCMDDt->port);
	strncpy(pCMDDt->address, pDoorDt->current_ip, 32);
	return send_cmd(pPacket, pCMDDt);
}


static int stop_all()
{
	int ret = 0;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	if(g_sv_timer_id >= 0){
		task_cancel_timer(g_sv_timer_id);
		g_sv_timer_id = -1;
	}
	md_stop_audio_streaming();
	md_stop_video_streaming();
	task_cancel_timer(pDoorDt->timer_id);
	if(door_idle != pDoorDt->stata){
		if(pDoorDt->callback){
			pDoorDt->callback(DR_STOPED, (unsigned int)pDoorDt->stata, pDoorDt->usr_data);
		}
	}else{
		ret = -1;
	}
	//门口机切换到空闲
	switch_door_stata(door_idle);
	return ret;
}

// 定时器超时后关闭
static int stop_callback(int timer_id, int interval)
{
	printf("INFO: calling time out\n");
	//通知对方挂断
	notify_close();
	stop_all();
	return 0;
}

// 定时器超时后门锁关闭
static int close_door_callback(int timer_id, int interval)
{
	//操作io口关闭锁
	close_door();
	if(g_sys_callback){
		g_sys_callback(DR_LOCK_CLOSED, 0, 0);
	}
	return 0;
}

static int net_recieving(int rcv_state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	int state = 0;
	int flg = 0;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	state = packet->state;
	switch(state)
	{
		case 0xfd: // room 机请求 图像预览 不分主机或分机
			flg = -1;
			if(0 == di_get_face_enroll()){
				// 门口机切换到预留状态
				flg = switch_door_stata(door_preview);
			}
			if(0 == flg){
				strncpy(pDoorDt->current_ip,packet->ip, 32);
				//延时300ms打开视频流
				//md_start_video_streaming(pDoorDt->current_ip, NULL);
				md_set_ite_video(0);
				if(0 == strcmp(packet->title,"ITE")){
					md_set_ite_video(1);
				}
				delay_start_video(DELAY_START_VIDEO);
				
				//预览120秒后自动挂断
				g_door_data.timer_id = task_timer(DOOR_TIMEOU, stop_callback);
				//回调用户，门口机已被预览
				if(g_sys_callback){
					pDoorDt->callback = g_sys_callback;
					g_sys_callback(DR_BE_PREVIEW, 0, 0);
				}
			}else{
				//返回正在忙
				packet->state = 0xff; // busy
			}
			break;
		case 0xfc: // room -> 手动停止或120S自动  停止通讯，包括图像预览等 
		
			//当前ip与packet的ip不一样就忽略
			if(0 == strcmp(pDoorDt->current_ip, packet->ip)) {
				stop_all();
			}
			break;
		case 0xfb: // 呼叫后应答，并开始通讯,重新开始计算120秒
			if((door_calling == pDoorDt->stata)){   //&&(0 == strcmp(pDoorDt->current_ip, packet->ip))
				task_cancel_timer(g_door_data.timer_id);
				door_answer(packet->ip, NULL);
				if(pDoorDt->callback)
				{
					pDoorDt->callback(DR_CALL_ESTABLISHED, (unsigned int)pDoorDt->current_ip, pDoorDt->usr_data);
				}
				//预览120秒后自动挂断
				g_door_data.timer_id = task_timer(DOOR_TIMEOU, stop_callback);
				packet->state = 0xfb;
				strcpy(packet->ip, di_get_door_ip());
			}else{
				//应答失败
				packet->state = 0xff;  // 失败
				strcpy(packet->ip, di_get_door_ip());
			}
			break;

		case 0xfa: // room -> 30S 超时停止  停止通讯，包括图像预览等 
			//这个命令与0xfc一样，该命令估计不再用了
			break;

		case 0x11: //开锁信号
			door_lock_open(3, "000", "000");
			//梯控函数，接收到网络的state为0x11的包后触发
			call_elevator(di_get_elevator_ip(), (unsigned int)packet, NULL, 0, OPEN_DOOR_BY_NET);
			break;
		case 0xe1:
			//回调用户，人脸数据已更新
			if(g_sys_callback){
				g_sys_callback(DR_NOTIFY_FR_UPDATE, 0, 0);
			}
			break;
		case 0x08: //时间同步
			//时间改成主动获取，该命令估计不再用了
			break;

		case 0xc0: //天气
			break;
			

		case 0xc1://通知门口机去通知室内机有新消息
		   //该命令估计不再用了
		   break;
			
		case 0x70: // 主机发送消息 在门口机显示	
			break;
		case 0x47: // 主机发设备详细信息
			break;		
		case 0xf1: //devinfo
			break;
		case 0xf2: //version
			break;


		/*网络设置这样命令估计都不再用了*/	
		case 0xf3: //getip
			break;
		case 0xf4: //setip
			break;	
		case 0xf5: //set netmask
			break;
		case 0xf6: //set getway
			break;
		case 0xf7: //set serverip
			break;
			
		case 0xef://reboot
			break;	
			
		default:
			break;
	}
	return 1;
}

static int swatch_timer(int id, int interval)
{
	KeepAlive();
	swatch_taskpool();
	return 1;
}

int door_watchdog_enable()
{
	StartWatchdog();
	KeepAlive();
	return 0;
}


static int timer_mytest(int id, int interval)
{
	static unsigned char cnt = 3;
	printf("timer_mytest id=%d, interval=%d \n", id, interval);
	return (int)(cnt--);
}


int door_start(DOOR_CALLBACK callback)
{
	int nRet;

	//重置LCD屏
	//reset_lcd();
	
	//置功放有效
	//openPowerAmplifier();
	
	//加载配置文件
	di_load_door_info();
	
	if(callback){
		callback(DR_FINISH_LOAD_CFG, 0, 0);
	}
	
	//开启线程池
	create_taskpool(DOOR_TASK_NUMS);
	
	//初始化网络   在里面有开启服务器
	nw_init(net_recieving);

	//初始化媒体流
	md_init(callback);

	task_timer(120, swatch_timer);

	//打开485
	ReadyFor485();

	//task_timer(1000, timer_mytest);
	
	memset(&g_door_data, 0 ,sizeof(G_DOOR_DATA_S));
	g_door_data.timer_id = -1;
	g_door_data.stata = door_idle;
	nRet = pthread_mutex_init(&g_door_data.mutex, NULL);
	if(nRet != 0)
	{
		printf("ERROR:-pthread_mutex_init g_door_data.mutex fialed\n");
	}
	g_sys_callback = callback;
	return 0;
}

void door_stop()
{
	md_unint();
	nw_uninit();
	destroy_taskpool();
	StopWatchdog();
	g_sys_callback = NULL;
	pthread_mutex_destroy(&g_door_data.mutex);
	return;
}
// 呼叫电梯响应
static int elevator_response_calling(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	//接收到0x23
	if(0x23 == packet->state){
		printf("---command:0x23 sent sucessful!\n");

	}

	return 0;
}

/*
呼叫电梯函数
elevator_ip:电梯的IP地址
data_buf:   当接收到的是0x11时，data_buf 为指向 接收到的tcppacket 指针
			当调用是由于刷卡调用时，data_buf 为指向 unsigned char DataBfr[16] = {0}; 的指针;并且刷卡只有0x68的用户卡才会呼叫电梯
callback:   用户回调，目前没有用到
usr_data:   用户数据，目前没有用到
open_flag:  OPEN_DOOR_BY_CARD 为刷卡调用
			OPEN_DOOR_BY_NET  为网络接收到0x11开门消息调用
*/
int call_elevator(char* elevator_ip,  unsigned int data_buf, 
						DOOR_CALLBACK callback, unsigned int usr_data, 
						ELEVATOR_CARD_OR_NET open_flag)
{
	int ret = -1;
	char room[7] = {0};
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	struct rs485packet* p485Packet = NULL;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;

	printf("---call_elevator: enter, elevator_ip = %s, open_flag = %d\n", elevator_ip, open_flag);

	//没开梯控功能
	if(di_get_lift_ctl() != 1){
		printf("---call_elevator:elevator ctrl is not open \n");
		return -1;
	}
	//不是门口机
	if(di_get_door_type() != 2){
		printf("---call_elevator:type is not door \n");
		return -2;
	}

	/*当网络发0x11过来时，755door程序中有判断 callcenter 变量，所以我在这里判断门口机状态
	  门口机在 被预览 以及 拨号 过程中发送过来的梯控不起效果
	  原 755door 程序中 callcenter只在拨动00#的时候才置一，但是现在的程序框架中只有拨号的动作，其中拨号动作包含了
	  呼叫00和业主
	  现在只判断 door_calling 状态就可以了，因为门口机每个状态都是不可能同时发生的
	*/
	if( (open_flag == OPEN_DOOR_BY_NET) && (pDoorDt->stata == door_calling) ){
		printf("---call_elevator:door is  preview or calling \n");
		return -4;
	}

	//获取房号
	ret = elevator_get_room(data_buf, room, open_flag);
	if(ret < 0){
		printf("---call_elevator:elevator_get_room failed ret = %d \n", ret);
		return -8;
	}

	switch( di_get_elevator_ctrl() ){
		case ELEVATOR_COMMUNICATION_RS485:
			printf("---call_elevator:elevator use RS485 \n");
			ret = generate_rs485_params(&p485Packet);
			if(0 != ret){
				printf("---call_elevator:generate_rs485_params failed, ret = %d \n", ret);
				return -7;
			}

			p485Packet->door_open_flag = open_flag;
			p485Packet->door_floor = di_get_door_floor();
			strcpy(p485Packet->room, room);

			//向线程池中添加485发送任务
			return elevator_rs485_send_cmd(p485Packet);
			break;

		case ELEVATOR_COMMUNICATION_NET:
			printf("---call_elevator:elevator use NET \n");
			ret = generate_params(&pPacket, &pCMDDt);//动态分配网络包内存
			if(0 != ret){
				printf("---call_elevator:generate_params failed, ret = %d \n", ret);
				return -6;
			}

			//因为发送的数据部分不一样，所以区分是刷卡开的门还是其他机器通过网络发送0x11来开门的
			if(open_flag == OPEN_DOOR_BY_CARD){
				printf("---call_elevator:open door by card--- room=%s\n", room);
				pPacket->state = 0x23;
			}
			else if(open_flag == OPEN_DOOR_BY_NET){
				printf("---call_elevator:open door by net---room=%s\n", room);
				pPacket->state = 0x21;
			}
			else{
				//调用该函数的open_flag不会有其他的参数的，因此不可能进入
				printf("---call_elevator:call_elevator error net \n");
				free(pPacket);//不慎进入就要释放之前申请的内存
				free(pCMDDt);
				return -3;
			}
			
			//根据楼层填充
			if(di_get_door_floor() == 3){
				pPacket->password[0] = 0x33;
			}
			else if(di_get_door_floor() == 2){
				pPacket->password[0] = 0x32;
			}
			else{
				pPacket->password[0] = 0x31;
			}
			memset(pPacket->roomnum, '\0', 7);
			strcpy(pPacket->roomnum, room);
			strcpy(pPacket->title, pPacket->roomnum);
			//strcpy(pCMDDt->room, pPacket->roomnum);//这个操作要不要?????电梯通讯中没有用到回调 callback
			pCMDDt->usr_ck = callback;
			pCMDDt->reserve1 = usr_data;
			pCMDDt->net_respones_ck = elevator_response_calling;
			pCMDDt->port = di_get_server_port();
			strncpy(pCMDDt->address, elevator_ip, 32);
			//NET方式发送
			printf("---call_elevator:NET send_cmd \n");
			return send_cmd(pPacket, pCMDDt);
			break;

		default:
			printf("---call_elevator:unknow elevator communication way \n");
			break;
	}

	return 0;
}

int door_lock_open(int type, char* id, char* room)
{
	int flg = 0;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	g_opend_info_tmp.type = type;
	strcpy(g_opend_info_tmp.infor1, id);
	strcpy(g_opend_info_tmp.infor2, room);
	pthread_mutex_lock(&pDoorDt->mutex);
	if(0 == di_get_lock_authentication()){
		if(1 == di_get_face_enroll()){
			pthread_mutex_unlock(&pDoorDt->mutex);
			return 0;
		}
		if((door_idle == pDoorDt->stata) || (door_preview == pDoorDt->stata)){
			//如果空闲或预览时应许发出欢迎光临声音。
			flg = 1;
		}
	
		//操作io口开锁
		open_door();

		//锁定时器ID不需要保存，因为该定时器不允许关闭
		task_timer(DOOR_LOCKED_TIMEOUT, close_door_callback);

		if(1 == flg){
			md_play_welcome();
		}
	}
	if(g_sys_callback){
		g_sys_callback(DR_LOCK_OPENED, (unsigned int)&g_opend_info_tmp, (unsigned int)0);
	}

	pthread_mutex_unlock(&pDoorDt->mutex);
	return 0;
}

// 呼叫管理机或室内机响应
static int response_calling(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	if(DR_NET_DISCONNECTED == state){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 1, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
		return 0;
	}
	printf("INFO: response cmd=0x%x\n", packet->state);
	if(0x0e == packet->state){
		//开始通话
		md_play_dial();
		//延时300ms开始视频
		//md_start_video_streaming(pCMDDt->address, pCMDDt->usr_ck);
		md_set_ite_video(0);
		if(0 == strcmp(packet->title,"ITE")){
			md_set_ite_video(1);
		}
		delay_start_video(DELAY_START_VIDEO);
		pDoorDt->timer_id = task_timer(DOOR_DIAL_TIME, stop_callback);
		//通知正在等待
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_CALL_DIAL, 0, pCMDDt->reserve1);
		}
		dr_log_call(LOG_OPRATE_SUCCESS, pCMDDt->room, "");
	}else if(0x0c == packet->state){
		//正在忙
		md_play_busy();
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_CALL_BUSY, 0, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
		dr_log_call(LOG_OPRATE_FAILED, pCMDDt->room, "");
	}
	return 0;
}

static int call_center(char* center_ip, DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	ret = generate_params(&pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	strncpy(pDoorDt->current_ip, center_ip, 32);
	pPacket->state = 0x0f;
	strcpy(pPacket->roomnum, "001");
	strcpy(pCMDDt->room, "001");
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_calling;
	pCMDDt->port = di_get_center_port();
	strncpy(pCMDDt->address, center_ip, 32);
	
	return send_cmd(pPacket, pCMDDt);
}

// 获取到IP
static int response_get_cener_ip(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	//G_DOOR_DATA_S* pDoorDt = &g_door_data;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	if(DR_NET_DISCONNECTED == state){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 1, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
		return 0;
	}
	if(0xfd == packet->state){
		strncpy(g_center_ip, packet->ip, 32);
		time(&g_center_ip_active);
		//开始呼叫管理机
		call_center(g_center_ip, pCMDDt->usr_ck, pCMDDt->reserve1);
		
	}else if(packet->state == 0xfe){
	    //操作失败
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 2, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
	}
	return 0;
}


//获取管理机IP同时呼叫中心
static int door_get_center_ip(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	//G_DOOR_DATA_S* pDoorDt = &g_door_data;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 3, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
		return ret;
	}
	pPacket->state = 0xfd;
	strcpy(pPacket->roomnum, "000");
	strcpy(pCMDDt->room, "000");
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_get_cener_ip;
	pCMDDt->port = di_get_server_port();//10002
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}

int door_send_remote_cmd(char* host, unsigned int cmd)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	//G_DOOR_DATA_S* pDoorDt = &g_door_data;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = cmd;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, host, 32);
	return send_cmd(pPacket, pCMDDt);
}
//开始呼叫
int door_make_call(char* room, DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret;	
	time_t curtm;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	char* room_ip = NULL;
	ret = switch_door_stata(door_calling);
	if(0 != ret){
		return -1;
	}
	
	pDoorDt->usr_data = usr_data;
	pDoorDt->callback = callback;
	if(0 == strcmp(room, "00")){
		//呼叫管理中心
		pDoorDt->center_flg = 1;
		door_get_center_ip(callback, usr_data);
		/*
		time(&curtm);
		if((0 == strlen(g_center_ip)) || (curtm - g_center_ip_active) > CENTER_IP_EFFECTIVE_PEROID){
			door_get_center_ip(callback, usr_data);
		}else{
			//直接呼叫管理机
			call_center(g_center_ip, callback, usr_data);
		}
		*/
		return 0;
		
	}
	//呼叫业主
	room_ip = di_get_ip_by_room(room);
	if(NULL ==room_ip){
		//没有房号
		if(callback){
			callback(DR_CALL_NO_ROOM, (unsigned int)room, 0);
		}
		switch_door_stata(door_idle);
		return -2;
	}
	ret = generate_params(&pPacket, &pCMDDt);
	if(0 != ret){
		switch_door_stata(door_idle);
		return ret;
	}
	
	pPacket->state = 0x0f;
	strcpy(pPacket->roomnum, room);
	strcpy(pCMDDt->room, room);
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_calling;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, room_ip, 32);
	pDoorDt->center_flg = 0;
	strncpy(pDoorDt->current_ip, room_ip, 32);
	return send_cmd(pPacket, pCMDDt);	
}

//应答呼叫
int door_answer(char* remote_ip, DOOR_CALLBACK callback)
{
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	//CHECK_CURRENT_REMOTE_IP(pDoorDt->current_ip, remote_ip);
	md_start_audio_streaming(remote_ip, callback);
	return 0;
}


//开始留言
int door_make_message(char* room, DOOR_CALLBACK callback, unsigned int usr_data)
{
	return 0;
}

//取消留言
int door_cancel_message()
{
	return 0;
}

int door_close_every()
{
	int ret;
	notify_close();
	ret = stop_all();
	return ret;
}

static int response_get_time(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	struct timeval tv;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	struct tcppacket* pPacket = packet;
	if(DR_NET_DISCONNECTED == state){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD,1,pCMDDt->reserve1);
		}
		return 0;
	}
	tv.tv_sec = pPacket->ltime+8*3600;
	tv.tv_usec = 0;
	settimeofday(&tv,NULL);
	if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_SUCCESS,1,pCMDDt->reserve1);
		}
	printf("INFO: time get success\n");
	return 0;
}

int door_get_time(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0x80;
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_get_time;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}

#define INI_IP_FILE           "./ip.ini"
#define BLACK_FILE            "./black.ini"
#define WRITE_SIZE_TP         1024

static int net_recv_to_file(FILE** pfp,  char* file_name,  int state, struct tcppacket* packet)
{
	int ret = 0;
	FILE* fp = (*pfp);
	if(DR_RECV_START == state){
		fp = fopen(file_name,"wt+");
		if(fp){
			fwrite(packet->ip, 1, WRITE_SIZE_TP, fp);
			(*pfp) = fp;
		}
	}
	if(DR_RECV_CONTINUE == state){
		if(fp){
			fwrite(packet->ip, 1, WRITE_SIZE_TP, fp);
		}
	}
	if(DR_RECV_COMPLETED == state){
		//完成
		if(NULL == fp){
			fp = fopen(file_name,"wt+");
			(*pfp) = fp;
		}
		if(fp){
			fwrite(packet->ip, 1, WRITE_SIZE_TP, fp);
			ret = 1;
		}
	}
	return ret;
}

static int response_room_ip_list(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	static FILE* fp = NULL;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	if(DR_NET_DISCONNECTED == state){
		if(fp){
			fclose(fp);
		}
		fp = NULL;
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD,1,pCMDDt->reserve1);
		}
		return 0;
	}
	if(1 == net_recv_to_file(&fp, INI_IP_FILE, state, packet)){
		if(fp){
			fclose(fp);
		}
		fp = NULL;
		di_reload_ip_ini();
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_SUCCESS,1,pCMDDt->reserve1);
		}
	}
	return 0;
}

int door_get_room_ip_list(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0x82;
	if(3 == di_get_door_type()){
		//围墙机
		pPacket->state = 0x83;
	}
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_room_ip_list;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}
static int response_get_door_unit(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{

	int i=0;
	char* szPortInfo = NULL;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	struct tcppacket* pPacket = packet;
	if(DR_NET_DISCONNECTED == state){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 1, pCMDDt->reserve1);
		}
		printf("DEBUG: response test cmd fialed\n");
		return 0;
	}
	if(pCMDDt->usr_ck){
		pCMDDt->usr_ck(DR_OPERATE_SUCCESS, pPacket->ip, pCMDDt->reserve1);
	}
	printf("DEBUG:test cmd: %d\n", pPacket->state);
	printf("      title: %s\n",pPacket->title);
	printf("      roomnum: %s\n",pPacket->roomnum);
	printf("      ip: %s\n",pPacket->ip);
	
	return 0;
}

int door_get_door_unit(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0x8b;
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_get_door_unit;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}

static void split(char **arr, char *str, const char *del)
{
	char *s = strtok(str, del);    
	while(s != NULL) 
	{
		*arr++ = s;
		s = strtok(NULL, del);
	}
}

static int response_get_port_info(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{	
	char *arr[100]={0};
	const char *del = "|";
	int i=0;
	char strport[8];
	char* szPortInfo = NULL;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	struct tcppacket* pPacket = packet;
	if(DR_NET_DISCONNECTED == state){
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD,1,pCMDDt->reserve1);
		}
		return 0;
	}
	szPortInfo = pPacket->ip;
	memset(arr,0,sizeof(arr));
	split(arr, szPortInfo,del);
	strcpy(strport,*(arr+i++));
	di_set_server_port((unsigned short) atoi(strport));
	strcpy(strport, *(arr+i++));
	di_set_log_port((unsigned short) atoi(strport));
	
	strcpy(strport,*(arr+i++));
	di_set_center_port((unsigned short) atoi(strport));
	printf("[info.port:%d|info.logport:%d|info.centerport:%d]\n",di_get_server_port(),di_get_server_port(),di_get_center_port());
	di_save_device_cfg(2);
	return 0;
}

int door_get_port_info(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0x78;
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_get_port_info;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}

static int response_black_list(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	static FILE* fp = NULL;
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	if(DR_NET_DISCONNECTED == state){
		if(fp){
			fclose(fp);
		}
		fp = NULL;
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD,1,pCMDDt->reserve1);
		}
		return 0;
	}
	if(1 == net_recv_to_file(&fp, BLACK_FILE, state, packet)){
		if(fp){
			fclose(fp);
		}
		fp = NULL;
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_SUCCESS,1,pCMDDt->reserve1);
		}
	}
	return 0;
}

int door_get_black_list(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0xaa;
	if(3 == di_get_door_type()){
		//围墙机
		pPacket->state = 0xab;
	}
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_black_list;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}


static int response_ic_cord(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	DOOR_CMD_DT_S* pCMDDt = cmddt;
	if(DR_NET_DISCONNECTED == state){
		if(cmddt->usr_ck){
			cmddt->usr_ck(DR_OPERATE_FAILD,1,pCMDDt->reserve1);
		}
		return 0;
	}
	di_set_card(packet->title, packet->roomnum);
	di_update_card();
	if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_SUCCESS,1,pCMDDt->reserve1);
	}
	door_get_black_list(NULL, 0);
	return 0;
}


int door_get_ic_cord(DOOR_CALLBACK callback, unsigned int usr_data)
{
	int ret = 0;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return ret;
	}
	pPacket->state = 0x83;
	if(3 == di_get_door_type()){
		//围墙机
		pPacket->state = 0x93;
	}
	pCMDDt->usr_ck = callback;
	pCMDDt->reserve1 = usr_data;
	pCMDDt->net_respones_ck = response_ic_cord;
	pCMDDt->port = di_get_server_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	return send_cmd(pPacket, pCMDDt);
}


int door_get_passwd(DOOR_CALLBACK callback, unsigned int usr_data)
{
	return 0;
}


int door_post_rest_passwd(char* room, DOOR_CALLBACK callback, unsigned int usr_data)
{
	return 0;
}

DR_STATA_E door_get_cur_stata()
{
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	return pDoorDt->stata;
}


//
