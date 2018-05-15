//implement queue data

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<assert.h>
#include"blockqueue.h"

#define PUT_BLOCK    0

//´òÓ¡×®
int block_queue_noprint(const char* format, ...)
{
	return 0;
}

int block_queue_init(LPBLOCK_QUEUE_S queue, int maxsize)
{
	int ret = 0;
	assert(maxsize <= MAX_QUEUE_SIZE);
	memset(queue, 0, sizeof(BLOCK_QUEUE_S));
	queue->iMaxSize = maxsize;
	queue->iFirst = 0;
	queue->iLast= 0;
	queue->iQDataNum = 0;
	queue->abortflg = 0;
	ret = pthread_mutex_init(&queue->mutex,NULL);
	if(ret != 0)
	{
		block_queue_debug("debug:pthread_mutex_init fialed\n");
		return ret;
	}
#if PUT_BLOCK	
	ret = pthread_cond_init(&queue->condput, NULL);
	if(ret != 0)
	{
		block_queue_debug("debug:pthread_cond_init condput fialed\n");
		return ret;
	}
#endif
	ret = pthread_cond_init(&queue->condget, NULL);
	if(ret != 0)
	{
		block_queue_debug("debug:pthread_mutex_init condget fialed\n");
		return ret;
	}
	return ret;
}

int block_queue_abort(LPBLOCK_QUEUE_S queue)
{
	if(0 == queue->abortflg) 
	{
		pthread_mutex_lock(&queue->mutex);
		queue->abortflg = 1;
		pthread_mutex_unlock(&queue->mutex);
		pthread_cond_signal(&queue->condget);
#if PUT_BLOCK
		pthread_cond_signal(&queue->condput);
#endif
	}
	return 0;
}

int block_queue_get_nums(LPBLOCK_QUEUE_S queue)
{
	int ret = 0;
	pthread_mutex_lock(&queue->mutex);
	ret = queue->iQDataNum;
	pthread_mutex_unlock(&queue->mutex);
	return ret;
}

int block_queue_put(LPBLOCK_QUEUE_S queue, unsigned int data, unsigned int param1, unsigned int param2, unsigned int param3)
{
	int ret = 0;
	Q_DATA_S* lpDt; 
	pthread_mutex_lock(&queue->mutex);
	for(;;)
	{
		if(queue->abortflg)
		{
			data = 0;
			ret = -1;
			break;
		}
		if(queue->iQDataNum < queue->iMaxSize)
		{
			lpDt = &queue->QueueDt[queue->iLast];
			lpDt->lpQData = data;
			lpDt->Param1 = param1;
			lpDt->Param2 = param2;
			lpDt->Param3 = param3;
			queue->iQDataNum++;
			ret = queue->iQDataNum;
			queue->iLast++;
			if(queue->iLast >= MAX_QUEUE_SIZE)
			{
				queue->iLast = 0;
			}
			pthread_cond_signal(&queue->condget);
			break;
		}
		else if(queue->iQDataNum >= queue->iMaxSize)
		{
#if PUT_BLOCK
			pthread_cond_wait(&queue->condput, &queue->mutex);
#else
		ret = 0;
		break;	
#endif
		}
	}
	pthread_mutex_unlock(&queue->mutex);
	return ret;
}

unsigned int block_queue_get(LPBLOCK_QUEUE_S queue, unsigned int* outparam1, unsigned int* outparam2, unsigned int* outparam3)
{
	unsigned int data = 0;
	Q_DATA_S* lpDt; 
	pthread_mutex_lock(&queue->mutex);
	for(;;)
	{
		if(queue->abortflg)
		{
			data = 0;
			break;
		}
		if(queue->iQDataNum > 0)
		{
			lpDt = &queue->QueueDt[queue->iFirst];
			data = lpDt->lpQData;
			(*outparam1) = lpDt->Param1;
			(*outparam2) = lpDt->Param2;
			(*outparam3) = lpDt->Param3;
			queue->iQDataNum--;
			queue->iFirst++;
			if(queue->iFirst >= MAX_QUEUE_SIZE)
			{
				queue->iFirst = 0;
			}
#if  PUT_BLOCK
			pthread_cond_signal(&queue->condput);
#endif
			break;
		}
		else if(queue->iQDataNum <= 0)
		{
			pthread_cond_wait(&queue->condget, &queue->mutex);
		}
	}
	pthread_mutex_unlock(&queue->mutex);
	return data;
}

int block_queue_flush(LPBLOCK_QUEUE_S queue, QDataFree dtfree)
{
	int i = 0;
	Q_DATA_S* lpDt; 
	if(queue->iLast >= queue->iFirst)
	{
		for(i = queue->iFirst; i < queue->iLast; i++)
		{
			lpDt = &queue->QueueDt[i];
			dtfree(lpDt->lpQData);
		}
	}
	else
	{
		for(i = queue->iFirst; i < MAX_QUEUE_SIZE; i++)
		{
			lpDt = &queue->QueueDt[i];
			dtfree(lpDt->lpQData);
		}
		for(i = 0; i < queue->iLast; i++)
		{
			lpDt = &queue->QueueDt[i];
			dtfree(lpDt->lpQData);
		}
	}
	return 0;
}

int block_queue_destroy(LPBLOCK_QUEUE_S queue)
{
	queue->abortflg = 1;
	pthread_mutex_destroy(&queue->mutex);
#if PUT_BLOCK
	pthread_cond_destroy(&queue->condput);
#endif
	pthread_cond_destroy(&queue->condget);
	
	return 0;
}



