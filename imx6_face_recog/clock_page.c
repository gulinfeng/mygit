/****implement clock page*****/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "sdl_door.h"
#include "gui_utile.h"
#include "taskpool.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"

//?冽??膩?曄內???蝚虫葡??RESOURCE_CLOCK_PAGE.text[] ?啁?銝剔?雿?蔭
typedef enum{
	VOICE_PROMPT_ADDR = 0,//"霂剝??內霂瑟?#?琿"
	YEAR_MON_DAY_ADDR = 1,
	WEEK_ADDR = 2,
}TEXT_ADDR_ENUM;

#define CLOCK_PAGE_PIC_NUM  4
#define CLOCK_IMAGE         "./pic/clock.jpg"
#define CLOCK_HOUR_IMAGE    "./pic/clock_hour2.png"
#define CLOCK_MIN_IMAGE     "./pic/clock_min2.png"
#define CLOCK_SEC_IMAGE     "./pic/clock_sec.png"


#define CLOCK_PAGE_FONT_NUM 	3
#define CLOCK_FONT_FULLPATH 	"./simsun.ttf"
#define CLOCK_FONT_SIZE1    	21
#define CLOCK_YEAR_FONT_SIZE    18
#define CLOCK_WEEK_SIZE    		18


#define CLOCK_PAGE_TEXT_NUM 3
#define CONST_CLOCK_TEXT1   "语音提示请按#号键"
const char *WEEK_TEXT_BUFF[7] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};


#define CONST_CLOCK_TEXT1_INIT_X 320
#define CONST_CLOCK_TEXT1_INIT_Y 210

#define CONST_CLOCK_TIME_YEAR_X 119
#define CONST_CLOCK_TIME_YEAR_Y 144

#define CONST_CLOCK_TIME_WEAK_X 136
#define CONST_CLOCK_TIME_WEAK_Y 167

//clock_on_timer 500ms
#define CLOCK_TIME_INTERVAL 500

typedef struct _clock_page_resource
{
	SDL_Surface* screen;
	SDL_Surface* pic_surface[CLOCK_PAGE_PIC_NUM]; //image surface
	TTF_Font*    font[CLOCK_PAGE_FONT_NUM];	// fonts
	SDL_Surface* text[CLOCK_PAGE_TEXT_NUM]; //text surface 
	SDL_Rect     text_offset[CLOCK_PAGE_TEXT_NUM]; //text position
	PAGA_S*      self;
	int 		 get_system_clock_timer_id;
}RESOURCE_CLOCK_PAGE;

static RESOURCE_CLOCK_PAGE g_ui_clock_resource = {0};
static double system_sec = 0, system_min = 0, system_hour = 0;//angle of time

//1000ms
static int timer_task_get_system_time(int id, int interval)
{
	clock_get_system_time(&system_hour, &system_min, &system_sec);
	return 1;
}

//find the center of screen
SDL_Rect get_screen_center(SDL_Surface *screen, SDL_Surface *image, double zoom)
{
	SDL_Rect rect;
	//rect.x=  (screen->w - image->w ) / 2;
	//rect.y=  (screen->h - image->h ) / 2;
	//printf("screen->w=%d screen->h=%d image->w=%d image->h=%d  \n", screen->w, screen->h, image->w,image->h);
	rect.x = (320 - image->w)/2 ;
	rect.y = (240 - image->h)/2 ;
	return rect;
}

//change the angle of second hand
SDL_Surface *clock_change_angle_sec(SDL_Surface *screen, SDL_Surface*image ,double *angle)
{
	SDL_Surface* change = NULL ;
	SDL_Rect rect;

	/*if(*angle <= -360){
		*angle += 360;
	}

	*angle = *angle - 6.0;
*/
	change = rotozoomSurfaceXY(image, *angle, 0.1, 0.1, SMOOTHING_OFF);

	rect = get_screen_center(screen, change , 1);

	SDL_BlitSurface(change, NULL , screen , &rect);
	SDL_FreeSurface(change);
	return change;
}

SDL_Surface *clock_change_angle_min(SDL_Surface *screen, SDL_Surface*image, double *angle, double sec)
{
	SDL_Surface* change = NULL;
	SDL_Rect rect;

	//秒针周一圈 分周走 六度
/*	if((sec== 0) || (sec == -360)){
		if(*angle <= -360){
			*angle += 360;
		}
		*angle= *angle -6;
	}
	*/
	change = rotozoomSurfaceXY(image, *angle, 0.1, 0.1, SMOOTHING_OFF);
	rect = get_screen_center(screen, change, 1);

	SDL_BlitSurface(change, NULL , screen , &rect);
	SDL_FreeSurface(change);
	return change;
}

 

SDL_Surface *clock_change_angle_hour(SDL_Surface *screen, SDL_Surface*image ,double *angle, double min)
{
	SDL_Surface* change = NULL ;
	SDL_Rect rect;

	//分针周一圈 走 六度
	/*if(min == 360 || min == 60 || min == 120 || min == 180 || min == 240 || min == 300){
		if(*angle < -360){
			* angle += 360;
		}
		*angle= *angle - 5;
	}*/
	change = rotozoomSurfaceXY(image, *angle, 0.1, 0.1, SMOOTHING_OFF);
	rect = get_screen_center(screen, change, 1);

	SDL_BlitSurface(change, NULL , screen , &rect);
	SDL_FreeSurface(change);
	return change;
}

static void update_time_text(struct tm *time_now)
{
	RESOURCE_CLOCK_PAGE* pclock = &g_ui_clock_resource;
	SDL_Surface* text_tmp = NULL;
	SDL_Color font_color = { 160, 160, 160 };
	char year_buf[12] = {0};
	char week_buf[10] = {0};//strlen(WEEK_TEXT_BUFF[j]) ,result is 9
	int  tmyear = 0;

	if(time_now != NULL){
		if(time_now->tm_year > 100){
			tmyear = time_now->tm_year - 100;
		}
		else{
			printf("file=%s, func=%s, line=%d, today->tm_year is < 100 err! \n", __FILE__, __FUNCTION__, __LINE__);
			tmyear = 18;
		}
		sprintf(year_buf, "20%02d-%02d-%02d", tmyear, 1+time_now->tm_mon, time_now->tm_mday);
		sprintf(week_buf, "%s", WEEK_TEXT_BUFF[time_now->tm_wday]);
	}
	else{
		printf("err update_time_text param is NULL \n");
		return;
	}

	text_tmp = pclock->text[YEAR_MON_DAY_ADDR];
	pclock->text[YEAR_MON_DAY_ADDR] = TTF_RenderText_Solid(pclock->font[YEAR_MON_DAY_ADDR], 
															   year_buf, font_color);
	assert(pclock->text[YEAR_MON_DAY_ADDR]);
	if(text_tmp != NULL){
		//release the old text memory
		SDL_FreeSurface( text_tmp );
	}
	pclock->text_offset[YEAR_MON_DAY_ADDR].x = CONST_CLOCK_TIME_YEAR_X;
	pclock->text_offset[YEAR_MON_DAY_ADDR].y = CONST_CLOCK_TIME_YEAR_Y;

	text_tmp = pclock->text[WEEK_ADDR];
	pclock->text[WEEK_ADDR] = TTF_RenderUTF8_Blended(pclock->font[WEEK_ADDR], 
													 week_buf, font_color);
	assert(pclock->text[WEEK_ADDR]);
	if(text_tmp != NULL){
		//release the old text memory
		SDL_FreeSurface( text_tmp );
	}
	pclock->text_offset[WEEK_ADDR].x = CONST_CLOCK_TIME_WEAK_X;
	pclock->text_offset[WEEK_ADDR].y = CONST_CLOCK_TIME_WEAK_Y;
}

//get the system time , change the angle by time
void clock_get_system_time(double *hour, double*min, double *sec)
{
	static int year_old = 0, month_old = 0, day_old = 0, week_old = 0;
	struct tm *now;
	time_t time_tmp = time(NULL);
	now= localtime( &time_tmp );

	*hour = (double)now->tm_hour;
	*min = (double)now->tm_min;
	*sec = (double)now->tm_sec;
	//printf("The System Time Now : %d : %d : %d \n",(int)now->tm_hour,(int)now->tm_min,(int)now->tm_sec);

	*sec = (*sec) * (-6);
	*min = (*min) * (-6);
	*hour = (*hour) * (-30) + (*min)/12;

	//detect the day is change or not
	if( (now->tm_year != year_old) || (now->tm_mon != month_old) || \
		(now->tm_mday != day_old)  || (now->tm_wday != week_old)){
		year_old = now->tm_year;
		month_old = now->tm_mon;
		day_old = now->tm_mday;
		week_old = now->tm_wday;
		printf("update text: The System Time Now: 20%d-%d-%d %d:%d:%d week=%d \n",year_old-100, month_old+1, day_old, (int)now->tm_hour,(int)now->tm_min,(int)now->tm_sec, week_old);
		update_time_text(now);
	}
}


// used in 2 functions.  1:main.c get_time_callback()  2.sdl_door.c get_time_callback()
void send_clock_page_event(int event_type)
{
	if(g_ui_clock_resource.self){
		printf("file=%s, func=%s, line=%d, send event to update clock \n", __FILE__, __FUNCTION__, __LINE__);
		send_page_event(UE_USR_CLOCK_UPDATE, g_ui_clock_resource.self);
	}
	else{
		printf("ERROR file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	}
}

static void get_time_text(void)
{
	RESOURCE_CLOCK_PAGE* pclock = &g_ui_clock_resource;
    struct tm *now;
	time_t time_tmp = time(NULL);
	
	now = localtime( &time_tmp );
	
	update_time_text(now);
}

//put the source to destination surface
void apply_surface( int x, int y, SDL_Surface* source, SDL_Surface* destination , SDL_Rect* clip  )
{
	SDL_Rect offset;
    offset.x = x;
    offset.y = y;
    SDL_BlitSurface( source, clip, destination, &offset );
}

static int clock_page_event(int page_event, struct _tg_page* page)
{
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	switch(page_event)
	{
		case UE_USR_CLOCK_UPDATE:
			printf("clock_page_event UE_USR_CLOCK_UPDATE is comming \n");
			get_time_text();
			break;

		default:
			printf("clock_page_event unknow event \n");
			break;
	}
	return 0;
}

static int clock_on_end_page( struct _tg_page* page)
{
	int i = 0;
	RESOURCE_CLOCK_PAGE* pclock = &g_ui_clock_resource;
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);

	//release the memory
	if(pclock->screen != NULL){
		SDL_FreeSurface( pclock->screen );
	}
	for(i=0; i<CLOCK_PAGE_PIC_NUM; i++){
		if(pclock->pic_surface[i] != NULL){
			SDL_FreeSurface( pclock->pic_surface[i] );
		}
	}
	for(i=0; i<CLOCK_PAGE_FONT_NUM; i++){
		if(pclock->font[i] != NULL){
			TTF_CloseFont( pclock->font[i] );
		}
	}
	for(i=0; i<CLOCK_PAGE_TEXT_NUM; i++){
		if(pclock->text[i] != NULL){
			SDL_FreeSurface( pclock->text[i] );
		}
	}
	return 0;
}

static int clock_on_leave_page( struct _tg_page* page)
{
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	task_cancel_timer(g_ui_clock_resource.get_system_clock_timer_id);
	return 0;
}

static int clock_on_enter_page( struct _tg_page* page)
{
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);

	//open get system timer task
	g_ui_clock_resource.get_system_clock_timer_id = task_timer(1000, timer_task_get_system_time);
	//printf("get_system_clock_timer_id = %d \n ", g_ui_clock_resource.get_system_clock_timer_id);
	
	return 0;
}


static int clock_on_timer(int id, struct _tg_page* page)
{
	RESOURCE_CLOCK_PAGE* pclock = &g_ui_clock_resource;
	static unsigned char refresh_count =0, voice_prompt_move_count=0;
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);

	g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].x = g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].x - 1;
	if(g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].x < -150){
		g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].x = CONST_CLOCK_TEXT1_INIT_X;
	}
	
	refresh_page();

	return 0;
}

//500ms
static int clock_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	RESOURCE_CLOCK_PAGE* pclock = &g_ui_clock_resource;
	int i=0;

	clock_change_angle_hour(screen, pclock->pic_surface[1], &system_hour, system_min);
	clock_change_angle_min(screen, pclock->pic_surface[2], &system_min, system_sec);
	clock_change_angle_sec(screen, pclock->pic_surface[3], &system_sec);
	
	for(i=0; i<CLOCK_PAGE_TEXT_NUM; i++){
		apply_surface(pclock->text_offset[i].x, 
				  	pclock->text_offset[i].y,
				  	pclock->text[i], screen, NULL);
	}

	return 0;
}


static int clock_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int ret = 0;
	//printf("file=%s, func=%s, line=%d  key_code=%d \n", __FILE__, __FUNCTION__, __LINE__,key_code);
	/*MG_WINS* pWins = &g_mg_win;
	if(1 == pWins->weak_light_flg){
		if(0 == pWins->led_opened){
			enable_led();
			pWins->led_opened = 1;
			update_operate_last_time(page);
			return 0;
		}
	}*/

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
			//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
			
			md_stop_play();
			if(key_code == SDLK_F11){
				create_call_page('0');
			}else{
				create_call_page('1' + key_code - SDLK_F1);
			}
			break;
		case SDLK_F12:   // #?琿
			md_play_main();
			
			//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
			break;
		case SDLK_F10:   // *?琿
			create_main_menu_page();
			//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
			break;
		default:
			//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
			break;
	}
	return 0;
}

static int clock_on_init( struct _tg_page* page)
{
	SDL_Surface* pTemp = NULL;
	SDL_Color font_color = { 255, 255, 255 };
	Uint32 colorkey;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);

	pTemp = IMG_Load(CLOCK_IMAGE);
	if(!pTemp){
		/* image failed to load */
		printf("IMG_Load: %s  file=%s, func=%s, line=%d \n", IMG_GetError(), __FILE__, __FUNCTION__, __LINE__);
		return 1;
	}
	g_ui_clock_resource.pic_surface[0] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);

	pTemp = IMG_Load(CLOCK_HOUR_IMAGE);
	if(!pTemp){
		/* image failed to load */
		printf("IMG_Load: %s  file=%s, func=%s, line=%d \n", IMG_GetError(), __FILE__, __FUNCTION__, __LINE__);
		return 1;
	}
	g_ui_clock_resource.pic_surface[1] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	colorkey = SDL_MapRGB( g_ui_clock_resource.pic_surface[1]->format, 0x00, 0x00, 0x00 );
    SDL_SetColorKey( g_ui_clock_resource.pic_surface[1], SDL_SRCCOLORKEY, colorkey );

	pTemp = IMG_Load(CLOCK_MIN_IMAGE);
	if(!pTemp){
		/* image failed to load */
		printf("IMG_Load: %s  file=%s, func=%s, line=%d \n", IMG_GetError(), __FILE__, __FUNCTION__, __LINE__);
		return 1;
	}
	g_ui_clock_resource.pic_surface[2] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	colorkey = SDL_MapRGB( g_ui_clock_resource.pic_surface[2]->format, 0x00, 0x00, 0x00 );
    SDL_SetColorKey( g_ui_clock_resource.pic_surface[2], SDL_SRCCOLORKEY, colorkey );

	pTemp = IMG_Load(CLOCK_SEC_IMAGE);
	if(!pTemp){
		/* image failed to load */
		printf("IMG_Load: %s  file=%s, func=%s, line=%d \n", IMG_GetError(), __FILE__, __FUNCTION__, __LINE__);
		return 1;
	}
	g_ui_clock_resource.pic_surface[3] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	//映射关键色
	colorkey = SDL_MapRGB( g_ui_clock_resource.pic_surface[3]->format, 0xFF, 0xFF, 0xFF );
    SDL_SetColorKey( g_ui_clock_resource.pic_surface[3], SDL_SRCCOLORKEY, colorkey );
	

	//set background image
	page->background = g_ui_clock_resource.pic_surface[0];

	//open and set font size
	g_ui_clock_resource.font[VOICE_PROMPT_ADDR] = TTF_OpenFont(CLOCK_FONT_FULLPATH, CLOCK_FONT_SIZE1);
	g_ui_clock_resource.font[YEAR_MON_DAY_ADDR] = TTF_OpenFont(CLOCK_FONT_FULLPATH, CLOCK_YEAR_FONT_SIZE);
	g_ui_clock_resource.font[WEEK_ADDR] = TTF_OpenFont(CLOCK_FONT_FULLPATH, CLOCK_WEEK_SIZE);
	if ( g_ui_clock_resource.font[VOICE_PROMPT_ADDR] == NULL || \
		 g_ui_clock_resource.font[YEAR_MON_DAY_ADDR] == NULL || \
		 g_ui_clock_resource.font[WEEK_ADDR] == NULL  ) {
		printf("Couldn't load %d pt font from %s: %s\n",
					CLOCK_FONT_SIZE1, CLOCK_FONT_FULLPATH, SDL_GetError());
	}

	g_ui_clock_resource.text[VOICE_PROMPT_ADDR] = TTF_RenderUTF8_Blended(g_ui_clock_resource.font[0], 
																		CONST_CLOCK_TEXT1, font_color);
	assert(g_ui_clock_resource.text[VOICE_PROMPT_ADDR]);

	g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].x = CONST_CLOCK_TEXT1_INIT_X;
	g_ui_clock_resource.text_offset[VOICE_PROMPT_ADDR].y = CONST_CLOCK_TEXT1_INIT_Y;

	get_time_text();


	return 0;
}

int create_clock_page(SDL_Surface* screen)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	memset(&g_ui_clock_resource, 0, sizeof(RESOURCE_CLOCK_PAGE));
	g_ui_clock_resource.screen = screen;
	g_ui_clock_resource.self = page;
	page->page_id = 0;
	page->timer_interval = CLOCK_TIME_INTERVAL;
	page->on_init_page = clock_on_init;
	page->on_keydown = clock_on_keydown;
	page->on_paint = clock_on_paint;
	page->on_timer = clock_on_timer;

	page->on_page_enter = clock_on_enter_page;
	page->on_page_leave = clock_on_leave_page;
	page->on_end_page = clock_on_end_page;
	page->on_page_event = clock_page_event;
	begin_page(page);
	return;
}

