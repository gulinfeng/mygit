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


#define DOOR_DIAL_TIME               30000    //���н����ȴ�30��
#define DOOR_TIMEOU                  120000   // �Խ���Ԥ��������120����Զ��ر�
 
#define DOOR_LOCKED_TIMEOUT          1000     // �����򿪹�1����Զ�����    

#define DOOR_TASK_NUMS        MAX_WORKER_NUMS


#define CHECK_CURRENT_REMOTE_IP(current, remote)                        \
do{                                                                      \
	if(strcmp(current, remote)){                                         \
		printf("WARNING: currentip=%s, remoteip=%s\n",current,remote);    \
	}                                                                     \
}while(0)

typedef struct g_door_data
{
	
	int            center_flg;      //���������ͨ��ʱ�˿ڲ�һ��  �������10000������10002
	int            timer_id;        //��ǰ��ʱ��ʱ��
	DR_STATA_E     stata;           //��ǰ�ſڻ�״̬
	char  current_ip[32];           //��ǰͨ�ŶԷ���IP
	unsigned int  usr_data;          //��ǰͨ�ŵ��û�����
	DOOR_CALLBACK  callback;        //��ǰ�û��ص�, �Խ�������ʹ��
	DOOR_CALLBACK  lock_callback;   //�����ص�
	pthread_mutex_t mutex;
}G_DOOR_DATA_S;


#define CENTER_IP_EFFECTIVE_PEROID    86400    //�����IP��ַ��Ч��Ϊһ��3600*24 �� һ������»�ȡ��

#define DELAY_START_VIDEO        350//350  //��ʱ350ms

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
//��ʱ����Ƶ���������򿪣��Է���û��׼���ý��գ���һ��I֡�ᶪʧ
//����Ҫ��ʱ�򿪣��ȶԷ������ý����̡߳�
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

//Ӧ�����
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
	//�ſڻ��л�������
	switch_door_stata(door_idle);
	return ret;
}

// ��ʱ����ʱ��ر�
static int stop_callback(int timer_id, int interval)
{
	printf("INFO: calling time out\n");
	//֪ͨ�Է��Ҷ�
	notify_close();
	stop_all();
	return 0;
}

// ��ʱ����ʱ�������ر�
static int close_door_callback(int timer_id, int interval)
{
	//����io�ڹر���
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
		case 0xfd: // room ������ ͼ��Ԥ�� ����������ֻ�
			flg = -1;
			if(0 == di_get_face_enroll()){
				// �ſڻ��л���Ԥ��״̬
				flg = switch_door_stata(door_preview);
			}
			if(0 == flg){
				strncpy(pDoorDt->current_ip,packet->ip, 32);
				//��ʱ300ms����Ƶ��
				//md_start_video_streaming(pDoorDt->current_ip, NULL);
				md_set_ite_video(0);
				if(0 == strcmp(packet->title,"ITE")){
					md_set_ite_video(1);
				}
				delay_start_video(DELAY_START_VIDEO);
				
				//Ԥ��120����Զ��Ҷ�
				g_door_data.timer_id = task_timer(DOOR_TIMEOU, stop_callback);
				//�ص��û����ſڻ��ѱ�Ԥ��
				if(g_sys_callback){
					pDoorDt->callback = g_sys_callback;
					g_sys_callback(DR_BE_PREVIEW, 0, 0);
				}
			}else{
				//��������æ
				packet->state = 0xff; // busy
			}
			break;
		case 0xfc: // room -> �ֶ�ֹͣ��120S�Զ�  ֹͣͨѶ������ͼ��Ԥ���� 
		
			//��ǰip��packet��ip��һ���ͺ���
			if(0 == strcmp(pDoorDt->current_ip, packet->ip)) {
				stop_all();
			}
			break;
		case 0xfb: // ���к�Ӧ�𣬲���ʼͨѶ,���¿�ʼ����120��
			if((door_calling == pDoorDt->stata)){   //&&(0 == strcmp(pDoorDt->current_ip, packet->ip))
				task_cancel_timer(g_door_data.timer_id);
				door_answer(packet->ip, NULL);
				if(pDoorDt->callback)
				{
					pDoorDt->callback(DR_CALL_ESTABLISHED, (unsigned int)pDoorDt->current_ip, pDoorDt->usr_data);
				}
				//Ԥ��120����Զ��Ҷ�
				g_door_data.timer_id = task_timer(DOOR_TIMEOU, stop_callback);
				packet->state = 0xfb;
				strcpy(packet->ip, di_get_door_ip());
			}else{
				//Ӧ��ʧ��
				packet->state = 0xff;  // ʧ��
				strcpy(packet->ip, di_get_door_ip());
			}
			break;

		case 0xfa: // room -> 30S ��ʱֹͣ  ֹͣͨѶ������ͼ��Ԥ���� 
			//���������0xfcһ������������Ʋ�������
			break;

		case 0x11: //�����ź�
			door_lock_open(3, "000", "000");
			//�ݿغ��������յ������stateΪ0x11�İ��󴥷�
			call_elevator(di_get_elevator_ip(), (unsigned int)packet, NULL, 0, OPEN_DOOR_BY_NET);
			break;
		case 0xe1:
			//�ص��û������������Ѹ���
			if(g_sys_callback){
				g_sys_callback(DR_NOTIFY_FR_UPDATE, 0, 0);
			}
			break;
		case 0x08: //ʱ��ͬ��
			//ʱ��ĳ�������ȡ����������Ʋ�������
			break;

		case 0xc0: //����
			break;
			

		case 0xc1://֪ͨ�ſڻ�ȥ֪ͨ���ڻ�������Ϣ
		   //��������Ʋ�������
		   break;
			
		case 0x70: // ����������Ϣ ���ſڻ���ʾ	
			break;
		case 0x47: // �������豸��ϸ��Ϣ
			break;		
		case 0xf1: //devinfo
			break;
		case 0xf2: //version
			break;


		/*������������������ƶ���������*/	
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

	//����LCD��
	//reset_lcd();
	
	//�ù�����Ч
	//openPowerAmplifier();
	
	//���������ļ�
	di_load_door_info();
	
	if(callback){
		callback(DR_FINISH_LOAD_CFG, 0, 0);
	}
	
	//�����̳߳�
	create_taskpool(DOOR_TASK_NUMS);
	
	//��ʼ������   �������п���������
	nw_init(net_recieving);

	//��ʼ��ý����
	md_init(callback);

	task_timer(120, swatch_timer);

	//��485
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
// ���е�����Ӧ
static int elevator_response_calling(int state, struct tcppacket* packet, DOOR_CMD_DT_S* cmddt)
{
	//���յ�0x23
	if(0x23 == packet->state){
		printf("---command:0x23 sent sucessful!\n");

	}

	return 0;
}

/*
���е��ݺ���
elevator_ip:���ݵ�IP��ַ
data_buf:   �����յ�����0x11ʱ��data_buf Ϊָ�� ���յ���tcppacket ָ��
			������������ˢ������ʱ��data_buf Ϊָ�� unsigned char DataBfr[16] = {0}; ��ָ��;����ˢ��ֻ��0x68���û����Ż���е���
callback:   �û��ص���Ŀǰû���õ�
usr_data:   �û����ݣ�Ŀǰû���õ�
open_flag:  OPEN_DOOR_BY_CARD Ϊˢ������
			OPEN_DOOR_BY_NET  Ϊ������յ�0x11������Ϣ����
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

	//û���ݿع���
	if(di_get_lift_ctl() != 1){
		printf("---call_elevator:elevator ctrl is not open \n");
		return -1;
	}
	//�����ſڻ�
	if(di_get_door_type() != 2){
		printf("---call_elevator:type is not door \n");
		return -2;
	}

	/*�����緢0x11����ʱ��755door���������ж� callcenter �������������������ж��ſڻ�״̬
	  �ſڻ��� ��Ԥ�� �Լ� ���� �����з��͹������ݿز���Ч��
	  ԭ 755door ������ callcenterֻ�ڲ���00#��ʱ�����һ���������ڵĳ�������ֻ�в��ŵĶ��������в��Ŷ���������
	  ����00��ҵ��
	  ����ֻ�ж� door_calling ״̬�Ϳ����ˣ���Ϊ�ſڻ�ÿ��״̬���ǲ�����ͬʱ������
	*/
	if( (open_flag == OPEN_DOOR_BY_NET) && (pDoorDt->stata == door_calling) ){
		printf("---call_elevator:door is  preview or calling \n");
		return -4;
	}

	//��ȡ����
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

			//���̳߳������485��������
			return elevator_rs485_send_cmd(p485Packet);
			break;

		case ELEVATOR_COMMUNICATION_NET:
			printf("---call_elevator:elevator use NET \n");
			ret = generate_params(&pPacket, &pCMDDt);//��̬����������ڴ�
			if(0 != ret){
				printf("---call_elevator:generate_params failed, ret = %d \n", ret);
				return -6;
			}

			//��Ϊ���͵����ݲ��ֲ�һ��������������ˢ�������Ż�����������ͨ�����緢��0x11�����ŵ�
			if(open_flag == OPEN_DOOR_BY_CARD){
				printf("---call_elevator:open door by card--- room=%s\n", room);
				pPacket->state = 0x23;
			}
			else if(open_flag == OPEN_DOOR_BY_NET){
				printf("---call_elevator:open door by net---room=%s\n", room);
				pPacket->state = 0x21;
			}
			else{
				//���øú�����open_flag�����������Ĳ����ģ���˲����ܽ���
				printf("---call_elevator:call_elevator error net \n");
				free(pPacket);//���������Ҫ�ͷ�֮ǰ������ڴ�
				free(pCMDDt);
				return -3;
			}
			
			//����¥�����
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
			//strcpy(pCMDDt->room, pPacket->roomnum);//�������Ҫ��Ҫ?????����ͨѶ��û���õ��ص� callback
			pCMDDt->usr_ck = callback;
			pCMDDt->reserve1 = usr_data;
			pCMDDt->net_respones_ck = elevator_response_calling;
			pCMDDt->port = di_get_server_port();
			strncpy(pCMDDt->address, elevator_ip, 32);
			//NET��ʽ����
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
			//������л�Ԥ��ʱӦ������ӭ����������
			flg = 1;
		}
	
		//����io�ڿ���
		open_door();

		//����ʱ��ID����Ҫ���棬��Ϊ�ö�ʱ��������ر�
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

// ���й���������ڻ���Ӧ
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
		//��ʼͨ��
		md_play_dial();
		//��ʱ300ms��ʼ��Ƶ
		//md_start_video_streaming(pCMDDt->address, pCMDDt->usr_ck);
		md_set_ite_video(0);
		if(0 == strcmp(packet->title,"ITE")){
			md_set_ite_video(1);
		}
		delay_start_video(DELAY_START_VIDEO);
		pDoorDt->timer_id = task_timer(DOOR_DIAL_TIME, stop_callback);
		//֪ͨ���ڵȴ�
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_CALL_DIAL, 0, pCMDDt->reserve1);
		}
		dr_log_call(LOG_OPRATE_SUCCESS, pCMDDt->room, "");
	}else if(0x0c == packet->state){
		//����æ
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

// ��ȡ��IP
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
		//��ʼ���й����
		call_center(g_center_ip, pCMDDt->usr_ck, pCMDDt->reserve1);
		
	}else if(packet->state == 0xfe){
	    //����ʧ��
		if(pCMDDt->usr_ck){
			pCMDDt->usr_ck(DR_OPERATE_FAILD, 2, pCMDDt->reserve1);
		}
		switch_door_stata(door_idle);
	}
	return 0;
}


//��ȡ�����IPͬʱ��������
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
//��ʼ����
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
		//���й�������
		pDoorDt->center_flg = 1;
		door_get_center_ip(callback, usr_data);
		/*
		time(&curtm);
		if((0 == strlen(g_center_ip)) || (curtm - g_center_ip_active) > CENTER_IP_EFFECTIVE_PEROID){
			door_get_center_ip(callback, usr_data);
		}else{
			//ֱ�Ӻ��й����
			call_center(g_center_ip, callback, usr_data);
		}
		*/
		return 0;
		
	}
	//����ҵ��
	room_ip = di_get_ip_by_room(room);
	if(NULL ==room_ip){
		//û�з���
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

//Ӧ�����
int door_answer(char* remote_ip, DOOR_CALLBACK callback)
{
	G_DOOR_DATA_S* pDoorDt = &g_door_data;
	//CHECK_CURRENT_REMOTE_IP(pDoorDt->current_ip, remote_ip);
	md_start_audio_streaming(remote_ip, callback);
	return 0;
}


//��ʼ����
int door_make_message(char* room, DOOR_CALLBACK callback, unsigned int usr_data)
{
	return 0;
}

//ȡ������
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
		//���
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
		//Χǽ��
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
		//Χǽ��
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
		//Χǽ��
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
