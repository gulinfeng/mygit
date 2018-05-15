/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_info.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ���ý��ʵ��
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-5
*
*/
#ifndef DOOR_INFO_H
#define DOOR_INFO_H


#ifdef __cplusplus
extern "C" {
#endif




int di_load_door_info();

char*  di_get_door_ip();

char* di_get_subnet_mast();

char* di_get_gateway();

char*  di_get_server_ip();

char*  di_get_elevator_ip();


void di_set_door_ip(char* ip);
void di_set_subnet_mast(char* ip);
void di_set_gateway(char* ip);
void di_set_server_ip(char* ip);
void di_set_elevator_ip(char* ip);
void di_set_door_floor(int floor_num);
void di_set_lift_ctl(int lift_ctrl);
void di_set_elevator_ctrl(int elevator_ctrl);


/*����2 �ſڻ�
*����3 Χǽ��
*����1 ��
*/
int di_get_door_type();

void di_set_door_type(int type);

int di_get_carused();
int di_get_watch_dog();

//��ȡ���ݿ��ƿ��ر���
int di_get_lift_ctl(void);
//��ȡ��ǰ�ſڻ�����¥��
int di_get_door_floor(void);
//��ȡ�ݿ�ͨѶ��ʽ: 1�����緽ʽ   0��485��ʽ
int di_get_elevator_ctrl(void);

char* di_get_user_card();
char* di_get_mng_card();
char* di_get_sys_card();

int di_get_card_sector();
void di_set_card_sector(int sector);

int di_get_card_len();
void di_set_card_len(int len);

int di_set_card(char* user, char* mng);

void di_load_card();
void di_update_card();

void di_set_server_port(unsigned short port);

void di_set_center_port(unsigned short port);

void di_set_log_port(unsigned short port);

void di_set_alram_port(unsigned short port);


unsigned short di_get_server_port();

unsigned short di_get_center_port();

unsigned short di_get_log_port();

unsigned short di_get_alram_port();

int di_get_video_width();

int di_get_video_height();

int di_get_micval();
int di_get_dialval();
int di_get_talkval();

void di_set_micval(int val);
void di_set_dialval(int val);
void di_set_talkval(int val);
void di_save_device_cfg(int type);

//ͨ������תIP
char* di_get_ip_by_room(char* room);

//ͨ��IPת����
char* di_get_room_by_ip(char* ip_temp);

int  di_reload_ip_ini();


int di_is_black_user(char* cardid);

void di_reload_blacklist();

void di_set_lock_authentication(int authen);
int di_get_lock_authentication();
void di_set_face_enroll(int enroll);
int di_get_face_enroll();

void di_set_recogn_mode(int mode);
int di_get_recogn_mode();  
int di_get_enroll_authentic();
/**ftp ��?��3��y?Y��?��Y**/
int di_get_ftp_config(char* user, char* passwd);
char* di_get_door_unit();
int  di_set_door_unit(char* unit);

void set_face_data_updated(int flg);
int is_face_data_updated();
char* di_get_ftp_host();

void di_set_ip_reported(int flg);
int di_is_ip_reported();
#ifdef __cplusplus
}
#endif

#endif //

