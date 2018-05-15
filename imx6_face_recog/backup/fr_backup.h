/*
* Copyright (c) 2018, ��������
* All rights reserved.
*
* �ļ����ƣ�  fr_backup.h
* �ļ���ʶ��  
* ժ Ҫ �������ݱ��ݵ�������
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2018-3-26
*
*/
#ifndef FR_DATA_BACKUP_H
#define FR_DATA_BACKUP_H

#define FTP_PATH   "/home/wffrdb" 

#ifdef __cplusplus
extern "C" {
#endif

void ftp_set_config(char* host, char* user, char* passwd, char* fr_path);

int ftp_report_deviceip(char* unit, char* dv_ip);

int ftp_update_local(char* unit);

int ftp_update_remote(char* unit,  int face_num, char* doorip[], int maxlen, int backup_flg);

int ftp_load_data(char* unit);

int ftp_upload_data(char* unit, int force);

int ftp_connect();

void ftp_close();

#ifdef __cplusplus
}
#endif

#endif //FR_DATA_BACKUP_H

