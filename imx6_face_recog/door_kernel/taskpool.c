/*********implement task pool********/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "taskpool.h"
#include "blockqueue.h"
#include <sys/time.h>


#define TYPE_DO_WORK  1
#define TYPE_DESTORY_WORKER 2

#define MAX_TIMER  16

typedef struct t_timer_context
{
	int timer_flg;
	unsigned int timer_stata[MAX_TIMER];
	unsigned int timer_ck_flg[MAX_TIMER];
	unsigned int interval[MAX_TIMER];
	timer_callback callback[MAX_TIMER];
	unsigned long long int start_time[MAX_TIMER];
	pthread_mutex_t mutex;
}TIMER_CONTEXT;

static pthread_t s_task_proess = (pthread_t)NULL;
static BLOCK_QUEUE_S s_task_queue;
static int s_max_worker = 0;
static THREAD_CONTEXT s_threads[MAX_WORKER_NUMS];

static TIMER_CONTEXT s_timer;

static void* task_working(void* param)
{
	int i;
	int nRet;
	pTASK_PARAM pData;
	THREAD_CONTEXT* pContext = (THREAD_CONTEXT*)param;
	block_queue_debug("INFO:-task_working--is running--(%u)\n", (unsigned int)pContext->thread_id);
	pthread_mutex_lock(&pContext->mutex_process);
	pContext->status_time = time(NULL);
	pContext->abort = 0;
	pthread_mutex_unlock(&pContext->mutex_process);
	for(; ;)
	{
		if(pContext->abort == 1)
		{
			pthread_mutex_lock(&pContext->mutex_process);
			if(pContext->pData)
			{
				free(pContext->pData);
				pContext->pData = NULL;
			}
			pthread_mutex_unlock(&pContext->mutex_process);
			break;
		}
		pthread_mutex_lock(&pContext->mutex_process);
		pContext->cond_flg = 1;
		pthread_cond_wait(&pContext->cond_process, &pContext->mutex_process);
		pContext->status = thread_running;
		pthread_mutex_unlock(&pContext->mutex_process);
		pContext->cond_flg = 0;
		if(pContext->abort == 1)
		{
			pthread_mutex_lock(&pContext->mutex_process);
			if(pContext->pData)
			{
				free(pContext->pData);
				pContext->pData = NULL;
			}
			pthread_mutex_unlock(&pContext->mutex_process);
			break;
		}
		pContext->status_time = time(NULL);
		pData = pContext->pData;
		if(pData->pre_processing)
		{
			pData->pre_processing(0, pData->Data1, pData->Data2, pData->Data3);
		}
		for(i = 0; i < pData->try_times; i++)
		{
			nRet = pData->processing(pData->Data1, pData->Data2, pData->Data3);
			if(1 == nRet)
			{
				break;
			}
		}
		if(pData->post_processing)
		{
			pData->post_processing(nRet, pData->Data1, pData->Data2, pData->Data3);
		}
		pthread_mutex_lock(&pContext->mutex_process);
		pContext->status_time = time(NULL);
		pContext->status = thread_idle;
		free(pContext->pData);
		pContext->pData = NULL;
		pthread_mutex_unlock(&pContext->mutex_process);
	}
	block_queue_debug("DEBUG:-task_working--pthread_exit-(%u)-\n", (unsigned int)pContext->thread_id);
	pthread_exit((void*)"exit");
	return 0;
}
static int create_worker(THREAD_CONTEXT* pContext)
{
	int nRet = 0;
	if((pContext->status == thread_init) &&(pContext->thread_id == (pthread_t)NULL))
	{
		nRet = pthread_mutex_init(&pContext->mutex_process,NULL);
		if(nRet != 0)
		{
			block_queue_debug("ERROR:-pthread_mutex_init mutex_process fialed\n");
			return nRet;
		}
		nRet = pthread_cond_init(&pContext->cond_process, NULL);
		if(nRet != 0)
		{
			pthread_mutex_destroy(&pContext->mutex_process);
			block_queue_debug("ERROR:-pthread_cond_init cond_process fialed\n");
			return nRet;
		}
		//create
		pContext->cond_flg = 0;
		nRet = pthread_create(&pContext->thread_id, NULL,(void*)task_working, (void*)pContext);
		if(nRet != 0)
		{
			pthread_mutex_destroy(&pContext->mutex_process);
			pthread_cond_destroy(&pContext->cond_process);
			pContext->status = thread_dead;
			pContext->thread_id = (pthread_t)NULL;
			block_queue_debug ("ERROR:-Create task_working Fail!\n");
			return nRet;
		}
		
	}
	return nRet;
}

static void try_destroying_workers()
{
	int i = 0;
	THREAD_CONTEXT* pContext;
	for(i = 0; i < s_max_worker; i++)
	{
		pContext = &s_threads[i];
		if((pContext->status == thread_quit) && (pContext->thread_id != (pthread_t)NULL))
		{
			pthread_mutex_lock(&pContext->mutex_process);
			pContext->abort = 1;
			pthread_mutex_unlock(&pContext->mutex_process);
			pthread_cond_signal(&pContext->cond_process);
			pthread_join(pContext->thread_id, NULL);
			//destory
			pthread_mutex_lock(&pContext->mutex_process);
			pContext->status = thread_dead;
			pContext->thread_id = (pthread_t)NULL;
			pthread_mutex_unlock(&pContext->mutex_process);
			pthread_mutex_destroy(&pContext->mutex_process);
			pthread_cond_destroy(&pContext->cond_process);
		}
	}
	return;
}

static void do_work(THREAD_CONTEXT* pContext, pTASK_PARAM pData)
{
	int counter = 0;
	assert(pData);
	pthread_mutex_lock(&pContext->mutex_process);
	if(pContext->pData){
		free(pContext->pData);
	}
	pContext->pData = pData;
	pthread_mutex_unlock(&pContext->mutex_process);
#if 0
	if(pData->Data1 == 0)
	{
		pData->Data1 = (unsigned int)pContext->thread_id;
	}
#endif
	
	for(; ;)
	{
		if(pContext->cond_flg == 1)
		{
			pthread_cond_signal(&pContext->cond_process);
			break;
		}
		counter++;
		if(counter >= 5)
		{
			assert(0);
		}
		usleep(100000);
	}
	return;
}

static THREAD_CONTEXT* get_idle_thread()
{
	int i = 0;
	for(i = 0; i < s_max_worker; i++)//0-15
	{
		if(s_threads[i].status == thread_idle)
		{
			pthread_mutex_lock(&s_threads[i].mutex_process);
			s_threads[i].status = thread_ready;
			pthread_mutex_unlock(&s_threads[i].mutex_process);
			return	&s_threads[i];
		}
		if(s_threads[i].status == thread_dead)
		{
			//pthread_mutex_lock(&s_threads[i].mutex_process);
			s_threads[i].status = thread_init;
			//pthread_mutex_unlock(&s_threads[i].mutex_process);
			return	&s_threads[i];
		}
	}
	return NULL;
}

static void* task_processing_loop(void* param)
{
	int ret;
	unsigned int param1, param2, param3;
	unsigned int task_id = 0;
	pTASK_PARAM  pData;
	THREAD_CONTEXT* pContext;
	block_queue_debug("DEBUG:-task_processing_loop--thread running ---pthread id = %u---\n",  (unsigned int)pthread_self());
	LPBLOCK_QUEUE_S lpQueue = (LPBLOCK_QUEUE_S)param;
	for(; ;)
	{
		pData = (pTASK_PARAM)block_queue_get(lpQueue, &task_id, &param2, &param3);
		if(0 == pData)
		{
			block_queue_debug("ERROE:-queue_get-task_id=0--\n");
			break;
		}
		switch(task_id)
		{
			case TYPE_DO_WORK:
				for(; ;)
				{
					pContext = get_idle_thread();
					if(pContext)
					{
						ret = create_worker(pContext);
						if(ret != 0)
						{
							block_queue_debug("ERROE:--create worker fialed--\n");
							usleep(100000);	
							continue;
						}
						do_work(pContext, pData);
						break;
					}
					//queue_debug("INFO: threading wait for idle\n");
					try_destroying_workers();
					usleep(100000);	
				}
				break;
			case TYPE_DESTORY_WORKER:
				try_destroying_workers();
				break;
			default:
				break;
		}
		
	}
	block_queue_debug("DEBUG:--task_processing_loop--pthread_exit--\n");
	pthread_exit((void*)"exit");
	return 0;
}

static void init_thread_context()
{
	int i = 0;
	s_max_worker = 0;
	memset(s_threads, 0, sizeof(s_threads));
	for(i = 0; i < MAX_WORKER_NUMS; i++)
	{
		s_threads[i].thread_id = (pthread_t)NULL;
		s_threads[i].status = thread_dead;
	}
}


int swatch_taskpool()
{
	int i = 0;
	int destoryflg = 0;
	time_t cur_time;
	if((pthread_t)NULL == s_task_proess)
	{
		return 1;
	}
	for(i = 0; i < s_max_worker; i++)
	{
		if(s_threads[i].thread_id)
		{
			if(s_threads[i].status == thread_idle)
			{
				cur_time = time(NULL);
				if((cur_time - s_threads[i].status_time) >= WORKER_LIFE_PERIOD)
				{
					//一直空闲就退出线程
					pthread_mutex_lock(&s_threads[i].mutex_process);
					s_threads[i].status_time = cur_time;
					s_threads[i].status = thread_quit;
					pthread_mutex_unlock(&s_threads[i].mutex_process);
					block_queue_debug ("INFO:--worker idle for long time so quited---adress=%u\n",(unsigned int)s_threads[i].thread_id);
					destoryflg = 1;
				}
			}
	/*
			if(s_threads[i].status == thread_running)
			{
				cur_time = time(NULL);
				assert(s_threads[i].pData);
				if(s_threads[i].pData->time_out > 0)
				{
					if((cur_time - s_threads[i].status_time) >= s_threads[i].pData->time_out)
					{
						//运行超时，强制退出
						cur_time++;
					}
				}
			}
*/
		}
	}
	if(1 == destoryflg)
	{
		block_queue_put(&s_task_queue, (unsigned int)1, (unsigned int)TYPE_DESTORY_WORKER, 0, 0);
	}
	return 0;
}

//int put_task();

int create_taskpool(int tasknum)//16
{
	int nRet = -1;
	if(tasknum <= 0)
	{
		return nRet;
	}
	if((pthread_t)NULL == s_task_proess)
	{
		init_thread_context();
		block_queue_init(&s_task_queue, 64);
		nRet = pthread_create(&s_task_proess, NULL,(void*)task_processing_loop, (void*)&s_task_queue);
		if(nRet!=0)
		{
			block_queue_debug ("ERROR:-Create task_processing Fail!\n");
		}
		s_max_worker = tasknum <= MAX_WORKER_NUMS ? tasknum : MAX_WORKER_NUMS;
	}
	memset(&s_timer, 0, sizeof(TIMER_CONTEXT));
	nRet = pthread_mutex_init(&s_timer.mutex, NULL);
	if(nRet != 0)
	{
		block_queue_debug("ERROR:-pthread_mutex_init s_timer.mutex fialed\n");
	}
	return nRet;
}

TASK_PARAM* malloc_task_param()
{
	pTASK_PARAM param = (pTASK_PARAM)malloc(sizeof(TASK_PARAM));
	if(param)
	{
		memset(param, 0, sizeof(TASK_PARAM));
		param->pre_processing = NULL;
		param->post_processing = NULL;
		param->processing = NULL;
		param->try_times = 1;
		return param;
	}
	return NULL;
}

int put_task_by_param(pTASK_PARAM param)
{
	if(s_task_proess)
	{
		block_queue_put(&s_task_queue, (unsigned int)param, (unsigned int)TYPE_DO_WORK, 0, 0);
	}
	return 0;
}
int cancel_task(pTASK_PARAM ident)
{
	return 0;
}

int terminate_alltask()
{
	return 0;
}

void destroy_taskpool()
{
	terminate_alltask();
	if(s_task_proess)
	{
		block_queue_abort(&s_task_queue);
		block_queue_destroy(&s_task_queue);
		pthread_join(s_task_proess, NULL);
		s_task_proess = (pthread_t)NULL;
	}
	if(&(s_timer.mutex)){
		pthread_mutex_destroy(&s_timer.mutex);
	}
	memset(&s_timer, 0, sizeof(TIMER_CONTEXT));
	return;
}

unsigned long long int timer_now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long long int)((tv.tv_sec * 1000000 + tv.tv_usec)/1000);
}

static int add_timer(int interval, timer_callback callback)
{
	int i;
	int timer_id = -1;
	TIMER_CONTEXT* pContext = &s_timer;
	pthread_mutex_lock(&pContext->mutex);
	for(i = 0; i < MAX_TIMER; i++){
		if(0 == pContext->timer_stata[i]){
			timer_id = i;
			pContext->timer_stata[i] = 1;
			pContext->timer_ck_flg[i] = 0;
			pContext->interval[i] = interval;
			pContext->callback[i] = callback;
			pContext->start_time[i] = timer_now();
			break;
		}
	}
	pthread_mutex_unlock(&pContext->mutex);
	return timer_id;
}
static void remove_timer(int timer_id)
{
	TIMER_CONTEXT* pContext = &s_timer;
	if((timer_id >= 0) &&(timer_id < MAX_TIMER)){
		pthread_mutex_lock(&pContext->mutex);
		pContext->timer_stata[timer_id] = 0;
		pContext->timer_ck_flg[timer_id] = 0;
		pContext->interval[timer_id] = 0;
		pContext->callback[timer_id] = NULL;
		pContext->start_time[timer_id] = 0;
		pthread_mutex_unlock(&pContext->mutex);
	}
}

static timer_callback get_timer_callback(int timer_id)
{
	//int i;
	timer_callback ck = NULL;
	TIMER_CONTEXT* pContext = &s_timer;
	if((timer_id >= 0) &&(timer_id < MAX_TIMER)){
		pthread_mutex_lock(&pContext->mutex);
		if(1 == pContext->timer_stata[timer_id]){
			ck =pContext->callback[timer_id];
			pContext->timer_ck_flg[timer_id] = 2;
		}
		pthread_mutex_unlock(&pContext->mutex);
	}
	return ck;
}

static void timer_continue(int timer_id)
{
	TIMER_CONTEXT* pContext = &s_timer;
	if((timer_id >= 0) &&(timer_id < MAX_TIMER)){
		pthread_mutex_lock(&pContext->mutex);
		if(1 == pContext->timer_stata[timer_id]){
			pContext->timer_ck_flg[timer_id] = 0;
			pContext->start_time[timer_id] = timer_now();
		}
		pthread_mutex_unlock(&pContext->mutex);
	}
}

static int sys_timer_callback( unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	int ret = 0;
	timer_callback callback = NULL;
	int timer_id = (int) Data1;
	if((timer_id < 0) &&(timer_id >= MAX_TIMER)){
		return 0;
	}
	callback = get_timer_callback(timer_id);
	if(callback){
		ret = callback(timer_id,s_timer.interval[timer_id]);
		if(0 == ret){
			//退出定时器
			remove_timer(timer_id);
			return 0;
		}
		//继续回调
		timer_continue(timer_id);
	}else{
		remove_timer(timer_id);
	}
	return 0;
}

static int timer_process(unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	int i;
	int timer_num =0;
	int do_ck = 0;
	struct timeval tempval;
	pTASK_PARAM pParam = NULL;
	TIMER_CONTEXT* pContext = &s_timer;
	unsigned long long int now = 0;
	printf("INFO: start timer---\n");
	for(; ;)
	{
		timer_num = 0; 
		now = timer_now();
		pthread_mutex_lock(&pContext->mutex);
		for(i = 0; i < MAX_TIMER; i++){
			do_ck = 0;
			if(1 == pContext->timer_stata[i]){
				timer_num++;
				if(0 == pContext->timer_ck_flg[i]){
					if((unsigned int)(now - pContext->start_time[i]) > pContext->interval[i]){
						pContext->timer_ck_flg[i] = 1;
						do_ck = 1;
					}
				}
			}
			if(1 == do_ck){
				pParam = malloc_task_param();
				if(pParam)
				{
					pParam->Data1 = (unsigned int)i;
					pParam->processing = (task_processing)sys_timer_callback;
					put_task_by_param(pParam);
				}
			}
		}
		if(0 == timer_num){
			pContext->timer_flg = 0;
		}
		pthread_mutex_unlock(&pContext->mutex);
		if(0 == timer_num){
			break;
		}
		tempval.tv_sec = 0;
	    tempval.tv_usec = 100000;
	    select(0, NULL, NULL, NULL, &tempval);
	}
	return 0;
}

int task_timer(int interval, timer_callback callback)
{
	pTASK_PARAM pParam = NULL;
	TIMER_CONTEXT* pContext = &s_timer;
	if((pthread_t)NULL == s_task_proess)
	{
		return -1;
	}
	pthread_mutex_lock(&pContext->mutex);
	if(0 == pContext->timer_flg){
		pContext->timer_flg = 1;
		pParam = malloc_task_param();
		if(pParam)
		{
			pParam->processing = (task_processing)timer_process;
			put_task_by_param(pParam);
		}
	}
	pthread_mutex_unlock(&pContext->mutex);
	return add_timer(interval, callback);
}
void task_cancel_timer(int timer_id)
{
	int remove_flg = 0;
	TIMER_CONTEXT* pContext = &s_timer;
	if((timer_id >= 0) &&(timer_id < MAX_TIMER)){
		pthread_mutex_lock(&pContext->mutex);
		// ?????????,??????
		if(2 != pContext->timer_ck_flg[timer_id]){
			remove_flg = 1;
		}
		pthread_mutex_unlock(&pContext->mutex);
		if(1 == remove_flg){
			remove_timer(timer_id);
		}
	}
	return;
}

int do_task(task_processing processing, unsigned int  Data1, unsigned int  Data2)
{
	pTASK_PARAM pParam = NULL;
	pParam = malloc_task_param();
	if(pParam)
	{
		pParam->Data1 = Data1;
		pParam->Data2 = Data2;
		pParam->processing = (task_processing)processing;
		return put_task_by_param(pParam);
	}
	return -1;
}

//end
