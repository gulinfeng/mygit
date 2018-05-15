/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  tx522.h
* 文件标识：  
* 摘 要 tx522 刷卡实现
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-19
*
*/
#ifndef DOOR_TX522_H
#define DOOR_TX522_H

#ifdef __cplusplus
extern "C" {
#endif

//Communication Error
#define COMM_OK                 0x00      //命令调用成功
#define COMM_ERR                0xff      //串行通信错误

unsigned char TX_GetInfo(int uart, unsigned char* Info);

unsigned char TX_LoadKey(int uart);

unsigned char TX_GetCardSnr(int uart, unsigned char ReqCode, unsigned char *TagType, unsigned char *Sak, unsigned char *SnrLen, unsigned char *Snr);

unsigned char TX_ReadBlock(int uart, unsigned char Block, unsigned char *Data);

unsigned char TX_Halt(int uart);

#ifdef __cplusplus
}
#endif

#endif//
