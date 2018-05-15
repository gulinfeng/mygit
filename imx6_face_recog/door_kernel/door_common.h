/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_common.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ�ͨ��ͷ�ļ�
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-4
*
*/
#ifndef DOOR_COMMON_H
#define DOOR_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum door_error
{
	memery_err,
	card_err,
	video_err,
	audio_err,
	net_err,
	timeout
}DOOR_ERROR_E;

/****** �ſڻ��Խ�״̬, �ڲ�ʹ��*********/
typedef enum door_stata
{
	door_idle,
	door_calling,
	door_preview,
	door_message
}DR_STATA_E;



/******�ſڻ������ص�******/

//����
#define DR_SYS_ERROR         10

//�����
#define DR_BE_PREVIEW        11

//����
#define DR_BE_ALARM          12 

//������
#define DR_BE_RING           13


/**����״̬**/
#define DR_LOCK_OPENED       14
#define DR_LOCK_CLOSED       15

/**ˢ������״̬**/
#define DR_CARDREAD_ERROR         16

/*�����ļ��������*/
#define DR_FINISH_LOAD_CFG      17

/*��֪ͨ������������*/
#define DR_NOTIFY_FR_UPDATE        18
/******�ſڻ������ص�******/
#define DR_OPERATE_SUCCESS               20    //�����ɹ�
#define DR_OPERATE_FAILD                 21    //����ʧ��
#define DR_STARTED                       22
#define DR_STOPED                        23

#define DR_AUDIO_PROGRESS                24        //�շ���Ƶ���ݽ���  ��ʱ��֧��
#define DR_VIDEO_PROGRESS                25        //�շ���Ƶ���ݽ���  ��ʱ��֧��

//�����ȡ��
#define DR_PREVIEW_CANCELED             DR_STOPED

/**����״̬**/
//#define DR_CALL_RING                   20
#define DR_CALL_DIAL                   26
#define DR_CALL_BUSY                   27
#define DR_CALL_ESTABLISHED            DR_STARTED
#define DR_CALL_CANCELED               DR_STOPED
#define DR_CALL_NO_ROOM                29


/**��ƵԤ��״̬**/
//#define DR_VIDEO_STREAM_BEGIN       DR_BEGIN
//#define DR_VIDEO_STREAM_END         DR_END

/**����״̬**/
#define DR_MESSAGE_BEGIN            DR_STARTED
#define DR_MESSAGE_END              DR_STOPED



/*****�ڲ�ʹ��******/

//�޴���
#define DR_NO_ERROR              0

//����Ͽ�
#define DR_NET_DISCONNECTED      -1      //����Ͽ�

#define DR_RECV_START             1

#define DR_RECV_CONTINUE          2     //����δ������

#define DR_RECV_COMPLETED         3    //������������


/***** �ڲ�ʹ��*********/
/*******�ص���������********/
/*���лص������в�������ʱ����������
*/
//�û��ص�
/**
* state        �ص�״̬
* param        ״̬��Ӧ����
* usr_data     �û��Լ�����
**/
typedef int (*DOOR_CALLBACK)(int state, unsigned int param, unsigned int usr_data);


/**�������ݰ����ڲ�ʹ��**/
struct tcppacket{
    unsigned int   state;
    time_t         ltime;
    char  roomnum[7];
    char  password[5];
    char  title[20];
    char  ip[1024];
};

struct door_cmd_dt;

//������Ӧ�ص����ڲ�ʹ��
typedef int (*NET_RESPONES_CK)(int rcv_state, struct tcppacket* packet, struct door_cmd_dt* cmddt);

/*�û�����������ݣ��ڲ�ʹ��*/
typedef struct door_cmd_dt
{
	//unsigned int state;
	unsigned int port;
	unsigned int reserve1;
	unsigned int reserve2;
	char   room[8];
	char   address[32];
	DOOR_CALLBACK    usr_ck;
	NET_RESPONES_CK  net_respones_ck; 
}DOOR_CMD_DT_S;

/**�ݿز���**/
//�ݿ�ͨѶö��
typedef enum{
	ELEVATOR_COMMUNICATION_RS485 = 0,//ʹ��485��ʽ
	ELEVATOR_COMMUNICATION_NET = 1   //ʹ�����緽ʽ
}ELEVATOR_COMMUNICATION_SWITCH;

//�������ģ�鷢�����ݵ�ʱ����Ҫ������ˢ�����Ż�������ͨѶ���ŵ�
typedef enum{
	OPEN_DOOR_BY_CARD = 0,   //ʹ�ÿ�������
	OPEN_DOOR_BY_NET  = 1    //ʹ������������,��ͨ������0x11�����ŵ�
}ELEVATOR_CARD_OR_NET;

typedef struct rs485packet
{
	int door_open_flag;// ˢ������  ����  �յ�0x11����
	int door_floor; //�ſڻ�����¥��
	char room[8];
	char send_buff[8];//rs485���͵����ݰ�
};


#ifdef __cplusplus
}
#endif

#endif //
