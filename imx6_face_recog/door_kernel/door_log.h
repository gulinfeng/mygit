/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_card.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ�ˢ��ģ��ӿ�
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-19
*
*/
#ifndef DOOR_LOG_H
#define DOOR_LOG_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_OPRATE_SUCCESS  1
#define LOG_OPRATE_FAILED   0

void dr_log_enable(int enable);
//ˢ����־
void dr_log_read_card(int result, char* card_id, char* room);


//������־
void dr_log_call(int result, char* room, char* ztid);

//�������Ŀ���
void dr_log_center_open(int result);

//���ڻ�����
void dr_log_room_open(int result, char* room);


#ifdef __cplusplus
}
#endif

#endif //DOOR_LOG_H

