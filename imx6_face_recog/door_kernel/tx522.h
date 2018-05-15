/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  tx522.h
* �ļ���ʶ��  
* ժ Ҫ tx522 ˢ��ʵ��
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-19
*
*/
#ifndef DOOR_TX522_H
#define DOOR_TX522_H

#ifdef __cplusplus
extern "C" {
#endif

//Communication Error
#define COMM_OK                 0x00      //������óɹ�
#define COMM_ERR                0xff      //����ͨ�Ŵ���

unsigned char TX_GetInfo(int uart, unsigned char* Info);

unsigned char TX_LoadKey(int uart);

unsigned char TX_GetCardSnr(int uart, unsigned char ReqCode, unsigned char *TagType, unsigned char *Sak, unsigned char *SnrLen, unsigned char *Snr);

unsigned char TX_ReadBlock(int uart, unsigned char Block, unsigned char *Data);

unsigned char TX_Halt(int uart);

#ifdef __cplusplus
}
#endif

#endif//
