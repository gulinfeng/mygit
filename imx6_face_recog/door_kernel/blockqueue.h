/*
* Copyright (c) 2013, 西谷数字
* All rights reserved.
*
* 文件名称：  blockqueue.h
* 文件标识：  阻塞队列
* 摘 要：     
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2013-8-19
*/

#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include<stdio.h>
#include<unistd.h>
#include<pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_QUEUE_SIZE 516

typedef struct tagQData
{
	unsigned int  Param1;
	unsigned int  Param2;
	unsigned int  Param3;
	unsigned int  lpQData;
}Q_DATA_S;

typedef struct tagBlockQueue
{
	int iFirst;
	int iLast;
	int iMaxSize;
	int iQDataNum;
	int abortflg; 
	pthread_mutex_t mutex;
	pthread_cond_t condput;
	pthread_cond_t condget;
	Q_DATA_S QueueDt[MAX_QUEUE_SIZE];
	
}BLOCK_QUEUE_S, *LPBLOCK_QUEUE_S;



typedef void (* QDataFree)(unsigned int data);

int block_queue_noprint(const char* format, ...);


#if 1
#define block_queue_debug  printf
#else
#define sdkdebug  queue_noprint
#endif


int block_queue_init(LPBLOCK_QUEUE_S queue, int maxsize);

int block_queue_abort(LPBLOCK_QUEUE_S queue);

int block_queue_put(LPBLOCK_QUEUE_S queue, unsigned int data, unsigned int param1, unsigned int param2, unsigned int param3);

unsigned int block_queue_get(LPBLOCK_QUEUE_S queue, unsigned int* outparam1, unsigned int* outparam2, unsigned int* outparam3);

int block_queue_flush(LPBLOCK_QUEUE_S queue, QDataFree dtfree);

int block_queue_destroy(LPBLOCK_QUEUE_S queue);

int block_queue_get_nums(LPBLOCK_QUEUE_S queue);

#ifdef __cplusplus
}
#endif

#endif //_BLOCK_QUEUE_H


