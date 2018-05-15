/*
* Copyright (c) 2013, 西谷数字
* All rights reserved.
*
* 文件名称：  taskpool.h
* 文件标识：  线程池
* 摘 要：     
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2015-2-09
*/

#ifndef _TASK_POOL_H
#define _TASK_POOL_H

#include<stdio.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_LIFE_PERIOD     300   // 五分钟  (5*60) s
#define MAX_WORKER_NUMS  16


typedef int (*task_processing)(unsigned int Data1, unsigned int Data2, unsigned int Data3);
typedef void (*task_callback)(int process_ret, unsigned int Data1, unsigned int Data2, unsigned int Data3);
typedef int (*timer_callback)(int id, int interval);

typedef enum threadstatus
{
	thread_init = 0,
	thread_ready,
	thread_running,
	thread_idle,
	thread_quit,
	thread_dead,
}THREAD_STATUS;

typedef struct taskparam
{
	unsigned int  Data1;
	unsigned int  Data2;
	unsigned int  Data3;
	unsigned int time_out;
	unsigned int try_times;
	task_callback pre_processing;
	task_callback post_processing;
	task_callback timeout_processing;
	task_processing processing;
}TASK_PARAM, *pTASK_PARAM;

typedef struct threadcontext
{
	int abort;
	int cond_flg;
	pthread_t  thread_id;
	THREAD_STATUS status;
	time_t status_time;
	pthread_cond_t cond_process;
    pthread_mutex_t mutex_process;
	pTASK_PARAM  pData;
}THREAD_CONTEXT;

int create_taskpool(int tasknum);
int swatch_taskpool();

int do_task(task_processing processing, unsigned int  Data1, unsigned int  Data2);

//int put_task();
TASK_PARAM* malloc_task_param();
int put_task_by_param(pTASK_PARAM param);
int cancel_task(pTASK_PARAM ident);
int terminate_alltask();

void destroy_taskpool();

/**低精度定时器**/
/* interval 定时器时间间隔 单位毫秒，最低精度100ms
*  callback 定时回调 返回0自动取消定时器
*  返回 定时器ID, 返回-1为创建失败
*  定时器最多设置个数为12
*/
int task_timer(int interval, timer_callback callback);
void task_cancel_timer(int timer_id);

unsigned long long int timer_now();
#ifdef __cplusplus
}
#endif

#endif // _TASK_POOL_H

