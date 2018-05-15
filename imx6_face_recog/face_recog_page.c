/****implement face recog page*****/
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<assert.h>
#include<time.h>
#include "sdl_door.h"
#include "gui_utile.h"
#include "taskpool.h"

#if ((defined IMX6_FR) || (defined S500_FR))
	#define USE_FR_LIB
#endif

#ifdef USE_FR_LIB
#include "recog/face_recog.h"
#endif
#include "videos/gst_video.h"
#include "door_kernel/door_signal.h"
#include "door_kernel/door_media.h"
#include "door_kernel/taskpool.h"
#include "door_kernel/door_info.h"
#ifdef FTP_BACKUP
#include "backup/fr_backup.h"
#endif  

#define MAX_FACES_NUM       0xffffff
#define MAX_NAMES 32

#define ENROLLING_SCAN_TM     1200

#define MOVING_TEXT_TM        20000

#define ENROLLING_TIMEOUT    (80*1000)
#define ENROLLED_TIMEOUT     (3*1000)

#define DOOR_STATA_TIMEOUT    (1000)
#define COMFIRME_TIMEOUT      (2300)

#define LED_OPEN_TIMEOUT     (26)   //秒

#define ENROLL_RESULT_SUCCESS   0
#define ENROLL_RESULT_FAILED    1
#define ENROLL_RESULT_CANCEL    2
#define ENROLL_RESULT_TIMEOUT   3

#define FACE_ID_FILE  "./face_num.txt"

typedef enum _gui_stata
{
	recog_idle,
	recognizing,               //正在识别
	recognized,	               //完成识别
	enrolling,                 //正在注册
	enrolled                   //完成注册
}GUI_STATA;


typedef struct _face_info
{
	int dirty;
	int x;
	int y;
	int width;
	int height;
	unsigned long face_id;
	float confidence;
	char name[32];
}FACE_INFO;

typedef struct _management_windows
{
	int  video_pause;
	int  video_paused;
	int  enroll_counter;
	int  enroll_stata;   //  1 in box 2 out box
	int  enrolled;       // ENROLL_RESULT_SUCCESS 
	int  reset_flg;      // 重置人脸库，只有测试版才会重置
	int  weak_light_flg;  // 是否在低照度下
	int  led_opened;      // 是否打开LED
	unsigned long long int enroll_tm;  // 开始录脸和识别的时间      
	unsigned long long int enroll_pause_tm;
	unsigned long long int last_frame_update;
	int enrolling_timer_id;  
	int enrolled_timer_id;
	int unrecog_counter;
	int face_id;           // 当前录脸编号
	SDL_Surface* screen;
 	SDL_Overlay* overlay;
	SDL_Surface* video;
 	GUI_STATA stata;
 	SDL_mutex *mutex;
	unsigned long cur_enroll_id;
	FACE_INFO cache_info;
	PAGA_S*  self;
}MG_WINS;

static MG_WINS  g_mg_win = {0};

extern int create_face_authen_page();

static void fr_pg_resume_video()
{
	SDL_LockMutex(g_mg_win.mutex);
	g_mg_win.video_pause = 0;
	g_mg_win.video_paused = 0;
	SDL_UnlockMutex(g_mg_win.mutex);
}
static int fr_pg_pause_video()
{
	int old_state = 0;
	int counter = 0;
	SDL_LockMutex(g_mg_win.mutex);
	old_state = g_mg_win.video_pause;
	g_mg_win.video_pause = 1;
	g_mg_win.video_paused = 0;
	SDL_UnlockMutex(g_mg_win.mutex);
	while(1){
		SDL_Delay(20);
		if(1 == g_mg_win.video_paused){
			//printf("--fr_pg_idle_video---break-\n");
			break;
		}
		counter++;
		if(counter > 200){
			system("reboot");
		}
	}
	return old_state;
}

void set_weak_light_flg(int flg)
{
	g_mg_win.weak_light_flg = flg;
}

static void serial_face_id(int flg)
{
	FILE* fp = NULL;
	//char line[64];
	if(0 == flg){
		//获取ID
		fp = fopen(FACE_ID_FILE,"rt");
		if(NULL == fp){
			g_mg_win.face_id = 0;
			return;
		}
		fscanf(fp,"CUR_FACE_ID=%d",&g_mg_win.face_id);
		fclose(fp);
	}else{
		//保存ID
		fp = fopen(FACE_ID_FILE,"wt+");
		if(NULL == fp){
			printf("ERROR: open faceid file fialed\n");
			return;
		}
		fprintf(fp,"CUR_FACE_ID=%d\n",g_mg_win.face_id);
		fclose(fp);
	}
}

#ifdef USE_FR_LIB
void  copy_face_info(struct FRIDList* pDList );
inline void  copy_face_info(struct FRIDList* pDList )
{
	 float scale = (1.0f*SCREEN_W) / (OVERLAY_W);
	 SDL_LockMutex(g_mg_win.mutex);
	 FACE_INFO* fi = &g_mg_win.cache_info;             
	 struct FRFace* pFace = &pDList->pFace[0];         
	 fi->dirty = 0;                                    
     fi->x = pFace->x*scale;                                 
	 fi->y = pFace->y*scale;                                  
	 fi->width = pFace->width*scale;                          
	 fi->height = pFace->height*scale;                        
	 fi->confidence = pDList->pConfidence[0];           
	 fi->face_id = pDList->pRecordID[0];                 
	 strcpy(fi->name, pDList->pFName[0]);
	 SDL_UnlockMutex(g_mg_win.mutex);
	 //printf("DEBUG: face info [%d,%d,%d,%d]\n",fi->x, fi->y, fi->width, fi->height);
	 return;
} 


/***
* 0 未改变状态
* 1 改变为在框里
* 2 改变为在窗外
**/
static int check_face_in_rect(FACE_INFO* fr_face)
{
	static int last_sta = 0;
	static int counter = 0;
	int cur_sta = is_in_except_rect(fr_face->x, fr_face->y, fr_face->width, fr_face->height);
	if(0 == cur_sta){
		fr_reduce_recog_counter();
	}
	if(last_sta == cur_sta){
		counter++;
		if(counter == 2){
			counter = 0;
			if(0 == cur_sta){
				return 2;
			}
			return 1;
		}
	}else{
		last_sta = cur_sta;
		counter = 0;
	}
	return 0;
}
#endif


static  int gst_video_callback(VIDEO_STATA state, unsigned int param1,unsigned int param2)
{
	int ret;
	int event_type = UE_RECOGNIZATION;
#ifdef USE_FR_LIB
	FR_RESULT result;
#endif
	if(1 == g_mg_win.video_pause){
		SDL_LockMutex(g_mg_win.mutex);
		g_mg_win.video_paused = 1;
		SDL_UnlockMutex(g_mg_win.mutex);
		return 0;
	}
	switch(state){
		case video_error:
			break;
		case new_frame:
			switch(g_mg_win.stata)
			{
				case recog_idle:
				case recognized:
					break;
#ifdef USE_FR_LIB
				case recognizing:
					ret = fr_recognize((unsigned char *)param1, &result);
					if(0 == ret){
						if(FR_Recognized == result.stata){
							if(result.pDList) {
								copy_face_info(result.pDList);
								free(result.pDList);
							}
						}
						if(FR_FaceComfirmed == result.stata){
							if(1 == result.reserved1){
								//识别成功
								event_type = UE_USR_COMFIRMED_S;
								g_mg_win.self->usr_data[1] = result.pDList->pRecordID[0];
								strcpy(g_mg_win.self->usr_str1, result.pDList->pFName[0]);
								strcpy(g_mg_win.self->usr_str2, result.pDList->pLName[0]);
							}else if(0 == result.reserved1){
								//识别失败
								event_type = UE_USR_COMFIRMED_F;
							}
							if(result.pDList) {
								copy_face_info(result.pDList);
								free(result.pDList);
							}
							
						}
						
					}else {

						if(-3 == ret){
							if(0 == g_mg_win.reset_flg){
								g_mg_win.reset_flg = 1;
								send_page_event(UE_USR_RESET_LIB, (unsigned int)g_mg_win.self);
								return 0;
							}
						}
						SDL_LockMutex(g_mg_win.mutex);
						g_mg_win.cache_info.dirty = 1;
						SDL_UnlockMutex(g_mg_win.mutex);
					}
					send_page_event(event_type, (unsigned int)g_mg_win.self);
					break;
				case enrolling:
					event_type = UE_RECOGNIZATION;
					printf("gst_video_callback enrolling g_mg_win.cur_enroll_id =%d  \n", g_mg_win.cur_enroll_id);
					ret = fr_enroll((unsigned char *)param1, g_mg_win.cur_enroll_id, &result);
					if(0 == ret){
						if(FR_Recognized == result.stata){
							if(1 == result.reserved2){
								//离开框
								event_type = UE_USR_ENROLLING_OUT;
							}
							if(result.pDList) {
								g_mg_win.enroll_counter = result.reserved1;
								copy_face_info(result.pDList);
								ret = check_face_in_rect(&g_mg_win.cache_info);
								if(1 == ret){
									event_type = UE_USR_ENROLLING_IN; //
								}else if(2 == ret) {
									event_type = UE_USR_ENROLLING_OUT; //
								}
								free(result.pDList);
							}
						}
						if(FR_Enrolled == result.stata){
							SDL_LockMutex(g_mg_win.mutex);
							g_mg_win.stata = enrolled;
							
							SDL_UnlockMutex(g_mg_win.mutex);
							event_type = UE_USR_ENROLLED;
						}
					}else {
						SDL_LockMutex(g_mg_win.mutex);
						g_mg_win.cache_info.dirty = 1;
						SDL_UnlockMutex(g_mg_win.mutex);
					}
					send_page_event(event_type, (unsigned int)g_mg_win.self);
					break;
				case enrolled:
					event_type = UE_RECOGNIZATION;
					SDL_LockMutex(g_mg_win.mutex);
					g_mg_win.cache_info.dirty = 1;
					SDL_UnlockMutex(g_mg_win.mutex);
					send_page_event(event_type, (unsigned int)g_mg_win.self);
					break;
#endif
			}
			break;
		default:
			break;
	}
	return 0;
}

#ifdef USE_FR_LIB
static void switch_to_recoginzation();
static void  cancel_enrolling(int type);

static int timer_cancel_enrolled(int id, int interval)
{
	printf("DEBUG:timer_cancel_enrolled---entry recoginzation----\n");
	int ret = 0;
	PAGA_S* page = g_mg_win.self;
	switch_to_recoginzation();
	printf("DEBUG: timer_cancel_enrolled--\n");
	g_mg_win.enrolled_timer_id = 0;
	return 0;
}

static int timer_cancel_enrolling(int id, int interval)
{
	cancel_enrolling(ENROLL_RESULT_TIMEOUT);
	printf("DEBUG: timer_cancel_enrolling---\n");
	g_mg_win.enrolling_timer_id = 0;
	return 0;
}

static void stop_timer()
{
	if(g_mg_win.enrolling_timer_id > 0){
		task_cancel_timer(g_mg_win.enrolling_timer_id);
		g_mg_win.enrolling_timer_id  = 0;
	}
	if(g_mg_win.enrolled_timer_id > 0){
		task_cancel_timer(g_mg_win.enrolled_timer_id );
		g_mg_win.enrolled_timer_id  = 0;
	}
}

static void switch_to_enrolling()
{
    int enroll_id = -1;
	int ret;
	char name[32];
	enroll_id = g_mg_win.face_id;
	fr_pg_pause_video();
	if(recognizing == g_mg_win.stata ){
		stop_timer();
		//di_set_face_enroll(1);
		//开始录脸
		printf("DEBUG: begin to recording\n");
		md_play(4, 1);
		fr_init_encolling();
		SDL_LockMutex(g_mg_win.mutex);
		g_mg_win.stata = recog_idle;
		SDL_UnlockMutex(g_mg_win.mutex);
		if(0 == fr_setmode(FRMODE_ENROLL)){
			if(strlen(g_mg_win.self->usr_str1) <= 0){
				sprintf(name,"%d",enroll_id);
			}else{
				strcpy(name, g_mg_win.self->usr_str1);
			}
			printf("switch_to_enrolling name=%s\n",name);
			ret = fr_add_face(&g_mg_win.cur_enroll_id, name, g_mg_win.self->usr_str2);
			if(ret != 0){
				printf("ERROR: fr_add_face fialed\n");
				di_set_face_enroll(0);
				fr_setmode(FRMODE_RECOGNIZE);
				fr_init_recognization();
				SDL_LockMutex(g_mg_win.mutex);
				g_mg_win.stata = recognizing;
				g_mg_win.cache_info.dirty = 1;
				g_mg_win.reset_flg = 0;
				g_mg_win.unrecog_counter = 0;
				g_mg_win.enroll_tm = timer_now();
				SDL_UnlockMutex(g_mg_win.mutex);
				fr_pg_resume_video();
				return;
			}
			fr_init_encolling();
			SDL_LockMutex(g_mg_win.mutex);
			g_mg_win.stata = enrolling;
			g_mg_win.cache_info.dirty = 1;
			g_mg_win.reset_flg = 0;
			g_mg_win.enrolled = ENROLL_RESULT_FAILED;
			g_mg_win.enroll_stata = 1;
			g_mg_win.enroll_counter = fr_get_enroll_times();
			g_mg_win.enroll_tm = timer_now();
			g_mg_win.enroll_pause_tm = g_mg_win.enroll_tm;
			SDL_UnlockMutex(g_mg_win.mutex);
			g_mg_win.face_id++;
			serial_face_id(1);
			printf("DEBUG: add_face(%d) name:%s,%s\n",(int)g_mg_win.cur_enroll_id,g_mg_win.self->usr_str1,g_mg_win.self->usr_str2);
			g_mg_win.enrolling_timer_id = task_timer(ENROLLING_TIMEOUT, timer_cancel_enrolling);
		}else{
			assert(0);
		}
	}
	fr_pg_resume_video();
	return;
}

static void switch_to_recoginzation()
{
	fr_pg_pause_video();
	if(enrolled == g_mg_win.stata ){
		stop_timer();
		di_set_face_enroll(0);
		if(0 == fr_setmode(FRMODE_RECOGNIZE)){
			fr_init_recognization();
			SDL_LockMutex(g_mg_win.mutex);
			g_mg_win.stata = recognizing;
			g_mg_win.cache_info.dirty = 1;
			g_mg_win.reset_flg = 0;
			g_mg_win.unrecog_counter = 0;
			g_mg_win.enroll_tm = timer_now();
			SDL_UnlockMutex(g_mg_win.mutex);
		}
		
	}
	fr_pg_resume_video();
}

static void  cancel_enrolling(int type)
{
	fr_pg_pause_video();
	if(enrolling == g_mg_win.stata ){
		stop_timer();
		SDL_LockMutex(g_mg_win.mutex);
		g_mg_win.stata = enrolled;
		g_mg_win.enrolled = type;
		g_mg_win.cache_info.dirty = 1;
		SDL_UnlockMutex(g_mg_win.mutex);
		if(ENROLL_RESULT_TIMEOUT == type){
			//录脸超时
			printf("DEBUG: recording timeout id=%d\n",(int)g_mg_win.cur_enroll_id);
			md_play(6,1);
		}else if(ENROLL_RESULT_CANCEL == type){
			//录脸取消
			printf("DEBUG: recording quit id=%d\n", (int)g_mg_win.cur_enroll_id);
			md_play(5,1);
		}
		
		fr_remove_face(g_mg_win.cur_enroll_id);
		stop_timer();
		g_mg_win.cur_enroll_id = MAX_FACES_NUM;
		g_mg_win.enrolled_timer_id =  task_timer(ENROLLED_TIMEOUT, timer_cancel_enrolled);
	}
	fr_pg_resume_video();
}
#else
static int timer_cancel_enrolled(int id, int interval)
{
	return 0;
}

static int timer_cancel_enrolling(int id, int interval)
{
	return 0;
}

static void stop_timer()
{
	return;
}
#endif

extern int create_preview_page();
extern int create_face_authen_page();

static void pause_scanline(MG_WINS* pWins, int enbale)
{
	if(0 == enbale){
		pWins->enroll_pause_tm = timer_now();	
	}else{
		pWins->enroll_tm += timer_now() - pWins->enroll_pause_tm;
	}
}

static int fr_page_event(int page_event, struct _tg_page* page)
{
	int ret = 0;
	int old_state;
	char presentid[8];
	time_t cur_tm;
	time(&cur_tm);
	//printf("DEBUG: makecall_page_event %d ---\n",page_event);
	int mode = di_get_recogn_mode();
	switch(page_event)
	{
		case UE_RECOGNIZATION:
			refresh_page();
			break;
		case UE_USR_COMFIRMED_S:
			if(g_mg_win.self != current_page()){
				printf("INFO: current page not the main page\n");
				break;
			}
			if(recognizing != g_mg_win.stata){
				printf("INFO: ignore comfirmed\n");
				break;
			}
			SDL_LockMutex(g_mg_win.mutex);
			g_mg_win.stata = recognized;
			g_mg_win.unrecog_counter = 0;
			SDL_UnlockMutex(g_mg_win.mutex);
			sprintf(presentid,"%d",page->usr_data[1]);
			door_lock_open(4, page->usr_str1, page->usr_str2);
			update_operate_last_time(page);
			refresh_page();
			break;
		case UE_USR_COMFIRMED_F:
			//无法识别
			if(g_mg_win.self != current_page()){
				printf("INFO: current page not the main page\n");
				break;
			}
			if(recognizing != g_mg_win.stata){
				printf("INFO: ignore comfirmed");
				break;
			}
			g_mg_win.unrecog_counter++;
			if(0 == mode){
				if(g_mg_win.unrecog_counter < 9){
					md_play_unrecogn();
				}else if( 9 == g_mg_win.unrecog_counter){
					md_play(0, 1);
				}
			}else{
				if(1 == g_mg_win.unrecog_counter){
					md_play(15, 0);
				} else if(8 == g_mg_win.unrecog_counter){
					md_play(16, 0);
				}else if(25 == g_mg_win.unrecog_counter){
					md_play(0, 1);
				}
			}
			update_operate_last_time(page);
			refresh_page();	
			break;
		case UE_USR_ENROLLING_IN:
			//脸在框里
			printf("DEBUG: go into rectangle@@@@@@@@@@@@@@@@@@@\n");
			if(2 == g_mg_win.enroll_stata){
				pause_scanline(&g_mg_win, 1);
			}
			g_mg_win.enroll_stata = 1;
			update_operate_last_time(page);
			refresh_page();
			break;
		case UE_USR_ENROLLING_OUT:
			//脸离开框
			printf("DEBUG: go out rectangle#####################\n");
			if(1 == g_mg_win.enroll_stata){
				pause_scanline(&g_mg_win, 0);
			}
			g_mg_win.enroll_stata = 2;
			md_play(10, 0);
			update_operate_last_time(page);
			refresh_page();
			break;
		case UE_USR_ENROLL_AUTHEN:
			
			break;
		case UE_USR_ENROLLED:
			g_mg_win.enrolled = ENROLL_RESULT_SUCCESS;
			page->usr_data[7] += 1;
			page->usr_data[3] = time(NULL);
			md_play(8,1);
			stop_timer();
			g_mg_win.enrolled_timer_id =  task_timer(ENROLLED_TIMEOUT, timer_cancel_enrolled);
			refresh_page();
			break;
		case UE_USR_AUTHEN_FINISHED:
			if(0 == page->usr_data[0]){  // è??¤3é1|?òê?±e±ê??
				//êúè¨3é1|
				switch_to_enrolling();
				/*
				g_mg_win.enrolled = ENROLL_RESULT_SUCCESS;
				page->usr_data[7] += 1;
				page->usr_data[3] = time(NULL);
				md_play(8,1);
				*/
			}
			if(2 == page->usr_data[0]){
				//êúè¨è???
				g_mg_win.enrolled = ENROLL_RESULT_CANCEL;
				fr_remove_face(g_mg_win.cur_enroll_id);
				g_mg_win.cur_enroll_id = MAX_FACES_NUM;
				di_set_face_enroll(0);
				md_play(5,1);
			}
			if(3 == page->usr_data[0]){
				//êúè¨3?ê±
				g_mg_win.enrolled = ENROLL_RESULT_TIMEOUT;
				fr_remove_face(g_mg_win.cur_enroll_id);
				g_mg_win.cur_enroll_id = MAX_FACES_NUM;
				di_set_face_enroll(0);
				md_play(6,1);
			}
			//stop_timer();
			//g_mg_win.enrolled_timer_id =  task_timer(ENROLLED_TIMEOUT, timer_cancel_enrolled);
			refresh_page();
			break;
		case UE_USR_BEGIN_PREVIEW:
			create_preview_page();
			break;

		case UE_USR_WEAK_LIGHT:
			break;

#ifdef USE_FR_LIB
		case UE_USR_RESET_LIB:
			//paused_video();
			printf("DEBUG: ---UE_USR_RESET_LIB----\n");
			if(enrolling == g_mg_win.stata){
				cancel_enrolling(ENROLL_RESULT_CANCEL);
				break;
			}
			if(recognizing == g_mg_win.stata){
				old_state = fr_pg_pause_video();
				stop_timer();
				if(0 == fr_setmode(FRMODE_RECOGNIZE)){
					fr_init_recognization();
					SDL_LockMutex(g_mg_win.mutex);
					g_mg_win.stata = recognizing;
					g_mg_win.cache_info.dirty = 1;
					g_mg_win.reset_flg = 0;
					SDL_UnlockMutex(g_mg_win.mutex);
				}
				if(0 == old_state){
					// ?-à′ê?pause×′ì?￡? ???′2?ó?resume.
					fr_pg_resume_video();
				}
			}
			break;
#endif
		default:
			break;
		
	}
	return 0;
}
//static unsigned long long int g_last_time = 0;
//static unsigned int g_frame_counter = 0;
	
void drawing(MG_WINS* pWins, int usr_data1, int usr_data2)
{
	Uint32 color;
	int model = 0;
	float process;
	SDL_Rect rect;
	int frame_delay = 0;
	unsigned long long int cur_t;
	FACE_INFO* fi = &pWins->cache_info;
	rect.x = 0;
	rect.y = 0;
	rect.w = SCREEN_W;
	rect.h = SCREEN_H;
	cur_t = timer_now();
	frame_delay = 40 - (int)(cur_t - pWins->last_frame_update);
	if((frame_delay > 0)&&(frame_delay < 40)){
		clear_paint_event(pWins);
		SDL_Delay(frame_delay);
		pWins->last_frame_update = cur_t + frame_delay;
	}else{
		pWins->last_frame_update = cur_t;
	}
	/*
	g_frame_counter++;
	if(cur_t - g_last_time >= 1000){
		printf("DEBUG: video frame rate=%f\n", (float)(g_frame_counter) / (float)(cur_t - g_last_time)*1000.0);
		g_last_time = cur_t;
		g_frame_counter = 0;
	}
	*/
	switch(pWins->stata)
	{
		case recognized:
		case recognizing:
			model = 0;
			if(1 == pWins->led_opened){
				model = 1;
			}else if(1 == pWins->weak_light_flg){
				model = 2;
			}
			SDL_DisplayYUVOverlay(pWins->overlay, &rect);
			SDL_BlitSurface(pWins->video, NULL, pWins->screen, NULL);
			process = (float)((cur_t - pWins->enroll_tm) % MOVING_TEXT_TM) / (float)MOVING_TEXT_TM;
			draw_recognization(pWins->screen, model, process);
			if((0 == fi->dirty)&&((0 == pWins->weak_light_flg) || (1 == pWins->led_opened))){
				draw_face_info(pWins->screen,fi->x,fi->y,     \
						fi->width,fi->height, fi->confidence,  \
						fi->face_id, fi->name);
			}
			//SDL_Flip(pWins->screen);
			if(recognized == pWins->stata){
				SDL_Delay(600);
			}else{
				refresh_page();
			}
			break;
		case enrolling:
			if(1 != pWins->enroll_stata){
				cur_t = pWins->enroll_pause_tm;
			}
			process = (float)((cur_t - pWins->enroll_tm) % ENROLLING_SCAN_TM) / (float)ENROLLING_SCAN_TM;
			SDL_DisplayYUVOverlay(pWins->overlay, &rect);
			SDL_BlitSurface(pWins->video, NULL, pWins->screen, NULL);
			draw_record_face(pWins->screen, pWins->enroll_counter, process);
			refresh_page();
			//SDL_Flip(pWins->screen);
			break;
		case enrolled:
			SDL_DisplayYUVOverlay(pWins->overlay, &rect);
			SDL_BlitSurface(pWins->video, NULL, pWins->screen, NULL);
			draw_record_finished(pWins->screen, pWins->enrolled, pWins->cur_enroll_id);
			//SDL_Flip(pWins->screen);
			break;
		default:
			break;
	}
}

static int fr_on_init( struct _tg_page* page)
{
	Uint32 rmask = 0xff000000;
	Uint32 gmask = 0x00ff0000;
	Uint32 bmask = 0x0000ff00;
	Uint32 amask = 0x000000ff;
	Uint32 overlay_format = SDL_IYUV_OVERLAY;
	SDL_Surface* mem_surface = NULL;
	g_mg_win.cur_enroll_id = MAX_FACES_NUM;
	//SDL_Surface* screen =g_mg_win.screen;
	mem_surface = SDL_AllocSurface(SDL_SWSURFACE, SCREEN_W, SCREEN_H, 32,
	                           rmask, gmask, bmask, amask);
	g_mg_win.video = SDL_DisplayFormat(mem_surface);
	SDL_FreeSurface(mem_surface);
	if ( g_mg_win.video == NULL ) {
		printf("Couldn't Alloc Surface %s\n", SDL_GetError());
        return -2;
	}
	g_mg_win.overlay = SDL_CreateYUVOverlay(OVERLAY_W, OVERLAY_H, overlay_format, g_mg_win.video);
    if (!g_mg_win.overlay)
    {
        printf("Couldn't create overlay: %s\n", SDL_GetError());
        return -2;
    }
	g_mg_win.mutex = SDL_CreateMutex();
	if (!g_mg_win.mutex)
    {
        printf("SDL_CreateMutex fialed: %s\n", SDL_GetError());
        return -3;
    }
	set_fr_streamer(g_mg_win.overlay, gst_video_callback);
	serial_face_id(0);
	play_video();
	return 0;
}

static int fr_on_end_page( struct _tg_page* page)
{
	MG_WINS* pWins = &g_mg_win;
	if(pWins->mutex) {
		SDL_DestroyMutex(pWins->mutex);
	}
	if(pWins->overlay){
		SDL_FreeYUVOverlay(pWins->overlay);
	}
	if(pWins->video){
		SDL_FreeSurface(pWins->video);
	}
	return 0;
}

static int fr_on_enter_page( struct _tg_page* page)
{
	printf("---fr_on_enter_page----\n");
	switch_fr_streamer();
	SDL_LockMutex(g_mg_win.mutex);
	if(recognized == g_mg_win.stata || recog_idle == g_mg_win.stata){
		fr_init_recognization();
		g_mg_win.stata = recognizing;
		g_mg_win.cache_info.dirty = 1;
		g_mg_win.reset_flg = 0;
	}
	g_mg_win.unrecog_counter = 0;
	g_mg_win.enroll_tm = timer_now();
	g_mg_win.last_frame_update = g_mg_win.enroll_tm;
	g_mg_win.video_pause = 0;
	SDL_UnlockMutex(g_mg_win.mutex);
	if(1 == g_mg_win.weak_light_flg){
		enable_led();
		g_mg_win.led_opened = 1;
	}
	
	return 0;
}

static int fr_on_leave_page( struct _tg_page* page)
{
	printf("---fr_on_leave_page---\n");
	SDL_LockMutex(g_mg_win.mutex);
	g_mg_win.video_pause = 1;
	SDL_UnlockMutex(g_mg_win.mutex);
	if(1 == g_mg_win.led_opened){
		disable_led();
		g_mg_win.led_opened = 0;
	}
	return 0;
}

#ifdef FTP_BACKUP
static int ftp_update_local_data(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret = 0;
	struct _tg_page* page = (struct _tg_page*)Data1;
	ret = ftp_load_data(di_get_door_unit());
	if(ret != 0 ){
		set_face_data_updated(2);
		printf("WARNNING: ftp update local data failed\n");
	}else{
		set_face_data_updated(1);
		send_page_event(UE_USR_RESET_LIB, (unsigned int)get_main_page());
	}
	page->usr_data[2] = 0;
	return 0;
}

static int ftp_update_remote_data(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int i,num;
	char ip_buffer[16][32] = {0};
	char* ip[16];
	struct _tg_page* page = (struct _tg_page*)Data1;
	for(i = 0; i < 16; i++){
			ip[i] = ip_buffer[i];
	}
	num = ftp_update_remote(di_get_door_unit(),page->usr_data[7], ip, 16, 1);
	page->usr_data[7] = 0;
	for(i = 0; i < num; i++ ){
		if(0 != strcmp(ip[i], di_get_door_ip())){
			door_send_remote_cmd(ip[i],0xe1);
		}
	}
	return 0;
}

#endif

static int fr_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	int   run_tm;
	time(&cur_tm);
	run_tm = (unsigned int)cur_tm - page->last_time;
	if(run_tm >= LED_OPEN_TIMEOUT){
		g_mg_win.unrecog_counter = 0;
		if(1 == g_mg_win.led_opened){
			
			//大于40秒未使用自动关闭LED
			disable_led();
			g_mg_win.led_opened = 0;
			return 0;
		}
	}
	// ?üD?ftp·t???÷×?D?êy?Y
	if((0 == is_face_data_updated()) &&(0 == page->usr_data[2])&&(page->timer_counter > 2)){
		if(strlen(di_get_door_unit()) > 0){
			page->usr_data[2] = 1;
			do_task(ftp_update_local_data, (unsigned int) page, 0);
		}
	}
	
	if((page->usr_data[7] > 0) && (1 == is_face_data_updated()) && (recognizing == g_mg_win.stata)){
		run_tm = (unsigned int)cur_tm - page->usr_data[3];
		if(run_tm > 20){
			page->usr_data[3] = cur_tm;
			// í¨?aí?ò?μ￥?aμ????ú?ú?üD?êy?Y
			do_task(ftp_update_remote_data, (unsigned int)page, 0);
		}
	}
	return 0;
}


static int fr_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int ret = 0;
	MG_WINS* pWins = &g_mg_win;
	if(1 == pWins->weak_light_flg){
		if(0 == pWins->led_opened){
			enable_led();
			pWins->led_opened = 1;
			update_operate_last_time(page);
			return 0;
		}
	}
	switch(key_code)
	{
		case SDLK_F1:
		case SDLK_F2:
		case SDLK_F3:
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:
		case SDLK_F11:
#ifdef USE_FR_LIB
			if(recognizing == pWins->stata){
#else
			if(recognized == pWins->stata){
#endif
				//提示音
				//printf("key down sound 1\n");
				//play_sound(3,0);
				md_stop_play();
				if(key_code == SDLK_F11){
					create_call_page('0');
				}else{
					create_call_page('1' + key_code - SDLK_F1);
				}
			}
			if(enrolling == pWins->stata){
				//提示音
				//printf("------key down sound 2\n");
				//play_sound(8,0);
				md_play(11, 0);
			}
			if(enrolled == pWins->stata){
				//ìáê?ò?
				stop_timer();
				SDL_Delay(100);
				switch_to_recoginzation();
			}
			break;
#ifdef USE_FR_LIB
		case SDLK_F10:   // *号键
			if(enrolling == pWins->stata){
				//取消注册模式
				cancel_enrolling(ENROLL_RESULT_CANCEL);
				ret = 1;
			}else{
				if(1 == pWins->led_opened){
					disable_led();
					pWins->led_opened = 0;
				}
				md_play(0, 0);
			}
			break;
		case SDLK_F12:   // #号键
			if((recognizing == pWins->stata) && (0 == di_get_face_enroll())){
				//?D??μ?×￠2á?￡ê?
				if(g_mg_win.self){
					strcpy(g_mg_win.self->usr_str1, "");
					strcpy(g_mg_win.self->usr_str2, "");
					printf("fr_on_keydown # key : g_mg_win.self->usr_str1=%s, g_mg_win.self->usr_str2=%s\n", g_mg_win.self->usr_str1, g_mg_win.self->usr_str2);
				}
				di_set_face_enroll(1);
				if(1 == di_get_enroll_authentic()){
					create_face_authen_page();
					//printf("DEBUG:----create_face_authen_page---\n");
				}else{
					switch_to_enrolling();
					//page->usr_data[0] = 0;  // è??¤3é1|?òê?±e±ê??
					//send_page_event(UE_USR_ENROLLED, (unsigned int)page);
				}
				//send_page_event(UE_USR_ENROLL_AUTHEN, (unsigned int)g_mg_win.self);
				//switch_to_enrolling();
				ret = 1;
				break;
			}
			break;
#else
		case SDLK_F10:   // *号键
			//create_face_authen_page();
		break;
#endif
		default:
			break;
	}
	return 0;
}


static int fr_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	drawing(&g_mg_win, 0, 0);
	return 0;
}

int create_face_recog_page(SDL_Surface* screen)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	memset(&g_mg_win, 0, sizeof(MG_WINS));
	g_mg_win.cache_info.dirty = 1;
	g_mg_win.screen = screen;
#ifndef USE_FR_LIB
	g_mg_win.stata = recognized;
#endif
	g_mg_win.self = page;
	page->page_id = 0;
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_init_page = fr_on_init;
	page->on_keydown = fr_on_keydown;
	page->on_paint = fr_on_paint;
	page->on_timer = fr_on_timer;
	page->on_page_enter = fr_on_enter_page;
	page->on_page_leave = fr_on_leave_page;
	page->on_end_page = fr_on_end_page;
	page->on_page_event = fr_page_event;
	begin_page(page);
	return 0;
}



