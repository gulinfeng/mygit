/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_signal.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ�˽������ʵ��
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-4
*
*/
#ifndef DOOR_SIGNAL_H
#define DOOR_SIGNAL_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Open_Door
{
	int  type;
	char infor1[16];   
	char infor2[16];
}OPEN_DR_INFO_S;
int door_start(DOOR_CALLBACK callback);

void door_stop();


//�������Ź�
int door_watchdog_enable();


//��ȡdoor��ǰstata
DR_STATA_E door_get_cur_stata();


//�ر����У�����Ԥ�����Խ�������
int door_close_every();

//���е���
int call_elevator(char* elevator_ip,  unsigned int data_buf, 
						DOOR_CALLBACK callback, unsigned int usr_data, 
						ELEVATOR_CARD_OR_NET open_flag);



/******�ſڻ���������********/
int door_lock_open(int type, char* id, char* room);


//��ʼ����
/* room ���� 00Ϊ��������
*  callback ����״̬�ص� 
*/
int door_make_call(char* room, DOOR_CALLBACK callback, unsigned int usr_data);


/*
//Ӧ��Ԥ��
int door_allow_preview( DOOR_CALLBACK callback);
//ȡ��Ԥ��
int door_cancel_preview();
*/

//��ʼ����
/* room ����
*/
int door_make_message(char* room, DOOR_CALLBACK callback, unsigned int usr_data);
//ȡ������
int door_cancel_message();

//
int door_get_time(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_room_ip_list(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_ic_cord(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_black_list(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_passwd(DOOR_CALLBACK callback, unsigned int usr_data);

int door_post_rest_passwd(char* room, DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_port_info(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_door_unit(DOOR_CALLBACK callback, unsigned int usr_data);

int door_send_remote_cmd(char* host, unsigned int cmd);
#ifdef __cplusplus
}
#endif

#endif //


