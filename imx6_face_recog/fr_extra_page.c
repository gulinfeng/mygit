/****implement face recog extra page*****/
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<assert.h>
#include<time.h>
#include "sdl_door.h"
#include "gui_utile.h"
#include "SDL_gfxPrimitives.h"
#include "door_kernel/door_signal.h"
#include "door_kernel/door_media.h"
#include "videos/gst_video.h"

#define CARD_IMG_W  300
#define CARD_IMG_H  160

#define LEFT_MARGIN         (50)
#define INPUT_LINE_LEN      (SCREEN_W - 2*LEFT_MARGIN)

#define INPUT_LINE_Y0  50
#define INPUT_LINE_Y   130

#define SHOW_TEXT1   "刷卡认证失败"
#define SHOW_TEXT2   "预览"
#define SHOW_TEXT3   "请按任意键退出"

#define SHOW_TEXT4   "录制人脸,请先刷卡认证"
#define SHOW_TEXT5   "退出请按*键"
#define SHOW_TEXT6   "请输入房号:"
#define SHOW_TEXT7   "确认请按#键"
#define SHOW_TEXT8   "删除前一位请按*键"


int create_input_room_page();

static int fa_on_init(struct _tg_page* page)
{
	di_set_lock_authentication(1);
	md_play(17,1);
	page->usr_data[0] = 90;
	page->usr_data[1] = time(NULL);
	page->usr_data[2] = 1;
	strcpy(page->usr_str1, "");  // 卡号
	strcpy(page->usr_str2, "");  // 如果是管理卡就输入房号
	return 0;
}

static int fa_on_end(struct _tg_page* page)
{
	struct _tg_page* mainpage = get_main_page();
	if(mainpage){
		mainpage->usr_data[0] = page->usr_data[2];
		mainpage->usr_data[6] = page->usr_data[7];  // 卡类型
		strcpy(mainpage->usr_str1, page->usr_str1);  // 卡号
		strcpy(mainpage->usr_str2, page->usr_str2);  // 如果是管理卡就输入房号
		send_page_event(UE_USR_AUTHEN_FINISHED, (unsigned int) mainpage);
	}
	di_set_lock_authentication(0);
	return 0;
}

static int fa_on_page_event(int page_event, struct _tg_page* page)
{
	switch(page_event)
	{
		case UE_USR_AUTHEN_SECCESS:
			//授权成功
			page->usr_data[2] = 0;
			if((0 == page->usr_data[7])||( 1 == page->usr_data[7])){
				// 如果是管理卡就输入房号
				page->page_abort = 1; //自动退出
				create_input_room_page();
			}else{
				end_page();
			}
			break;
		case UE_USR_AUTHEN_FAILED:
			//授权失败
			page->usr_data[2] = 1;
			md_play(9, 1);
			break;
	}
	return 0;
}

static int fa_on_timer(int id, struct _tg_page* page)
{
	unsigned int run_tm;
	time_t cur_tm;
	time(&cur_tm);
	run_tm = (unsigned int)cur_tm - page->usr_data[1];
	if(run_tm >= 90){
		//大于90秒超时自动退出页面
		page->usr_data[2] = 3;
		end_page();
		return 0;
	}
	page->usr_data[0] = 90 - run_tm;
	refresh_page();
	return 0;
}

static int fa_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	//int i;
	//int finish_flg;
	switch(key_code)
	{
		case SDLK_F10:   // *号键
			page->usr_data[2] = 2;
			end_page();
			break;
		default:
			md_play(7,1);
			break;
	}
	return 0;
}

static int fa_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	char text[32];
	Uint32 color;
	SDL_Surface* pIcon;
	SDL_Rect dst_rect;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	pIcon = get_image(11);
	color = SDL_MapRGB(screen->format, 0, 0, 0);
	SDL_FillRect(screen, NULL, color);
	
	dst_rect.w = CARD_IMG_W;
	dst_rect.h = CARD_IMG_H;
	dst_rect.x = 0.5*(SCREEN_W - CARD_IMG_W);
	dst_rect.y = 0;
	SDL_BlitSurface(pIcon, NULL, screen, &dst_rect);
	text_line_ex_font(get_font(1), 0, CARD_IMG_H, SCREEN_W, SHOW_TEXT4, &ft_color,  1);
	ft_color.r = 120;
	ft_color.g = 120;
	ft_color.b = 120;
	text_line_ex(0, SCREEN_H - 30, SCREEN_W - 10, SHOW_TEXT5, &ft_color,  2);
	ft_color.r = 255;
	ft_color.g = 120;
	ft_color.b = 80;
	sprintf(text,"%d", page->usr_data[0]);
	text_line_ex_font(get_font(2),0, 10, SCREEN_W - 10, text, &ft_color,  2);
	return 0;
}

int create_face_authen_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 8;
	page->pFont = get_base_font();
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_init_page = fa_on_init;
	page->on_timer = fa_on_timer;
	page->on_paint = fa_on_paint;
	page->on_keydown = fa_on_keydown;
	page->on_end_page = fa_on_end;
	page->on_page_event = fa_on_page_event;
	begin_page(page);
	return 0;
}

static int ir_on_init(struct _tg_page* page)
{
	int charat_h, charat_w;
	md_play(18,1);
	TTF_SizeUTF8(get_font(2), "101", &charat_w, &charat_h);
	page->usr_data[7] = charat_h;
	page->usr_data[1] = time(NULL);
	page->usr_data[0] = 60;
	page->caret.h = charat_h - 1;
	page->caret.y = INPUT_LINE_Y - 6 - page->caret.h;
	page->caret.x = 0.5*SCREEN_W;
	show_page_caret(page, 1);
	return 0;
}

static int ir_on_timer(int id, struct _tg_page* page)
{
	unsigned int run_tm;
	time_t cur_tm;
	time(&cur_tm);
	run_tm = (unsigned int)cur_tm - page->usr_data[1];
	if(run_tm >= 60){
		//大于90秒超时自动退出页面
		PAGA_S* parent =  get_current_parent_page();
		parent->usr_data[2] = 3;
		end_page();
		return 0;
	}
	page->usr_data[0] = 60 - run_tm;
	refresh_page();
	return 0;
}

static int ir_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int cur_dex = 0;
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
			cur_dex = page->cur_dex;
			if(cur_dex < MAX_INPUT_NUM - 5){
				page->key_char[cur_dex] = '1' + (key_code - SDLK_F1);
				page->cur_dex++;
				cur_dex = page->cur_dex;
				page->key_char[cur_dex] = '\0';
				//刷新页面
				refresh_page();
			}
			break;
		case SDLK_F11:
			cur_dex = page->cur_dex;
			if(cur_dex < MAX_INPUT_NUM - 5){
				page->key_char[cur_dex] = '0';
				page->cur_dex++;
				cur_dex = page->cur_dex;
				page->key_char[cur_dex] = '\0';
				//刷新页面
				refresh_page();
			}
			break;
		case SDLK_F10:   // *号键
			page->cur_dex--;
			cur_dex = page->cur_dex;
			if(cur_dex >= 0){
				page->key_char[cur_dex] = '\0';
				//刷新页面
				refresh_page();
			}else{
			    page->cur_dex = 0;
			}
			break;
		case SDLK_F12:   // #号键
			if(page->cur_dex > 0){
				PAGA_S* parent =  get_current_parent_page();
				strcpy(parent->usr_str2, page->key_char);
				end_page();
			}
			break;
		default:
			break;
	}
	return 0;
}

static int ir_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int text_x;
	char text[32];
	Uint32 color;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	color = SDL_MapRGB(screen->format, 0, 0, 0);
	SDL_FillRect(screen, NULL, color);
	text_line_ex_font(get_font(1), LEFT_MARGIN - 20, INPUT_LINE_Y0 , SCREEN_W, SHOW_TEXT6, &ft_color,  0);

	color = make_rgba(255, 255,255, 255);
	hlineColor(screen,LEFT_MARGIN -20, (LEFT_MARGIN + INPUT_LINE_LEN), INPUT_LINE_Y, color);
	//hlineColor(screen,LEFT_MARGIN, (LEFT_MARGIN + INPUT_LINE_LEN), INPUT_LINE_Y+1, color);
	if(page->cur_dex > 0){
		sprintf(text,"%s",page->key_char);
		text_x = text_line_ex_font(get_font(2), LEFT_MARGIN, (INPUT_LINE_Y - page->usr_data[7] - 5), (SCREEN_W - 2*LEFT_MARGIN), text, &ft_color,  1);
		page->caret.x = text_x + 1;
	}
	
	ft_color.r = 120;
	ft_color.g = 120;
	ft_color.b = 120;
	text_line_ex(LEFT_MARGIN - 20, INPUT_LINE_Y + 10, 0, SHOW_TEXT8, &ft_color,  0);
	text_line_ex(LEFT_MARGIN - 20, INPUT_LINE_Y + 38, 0, SHOW_TEXT7, &ft_color,  0);
	
	ft_color.r = 255;
	ft_color.g = 120;
	ft_color.b = 80;
	sprintf(text,"%d", page->usr_data[0]);
	text_line_ex_font(get_font(2),0, 10, SCREEN_W - 10, text, &ft_color,  2);

	return 0;
}

int create_input_room_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 9;
	page->pFont = get_base_font();
	page->timer_interval = 200;	
	page->on_init_page = ir_on_init;
	page->on_timer = ir_on_timer;
	page->on_paint = ir_on_paint;
	page->on_keydown = ir_on_keydown;
	begin_page(page);
	return 0;
}

/*

static int preview_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}

static int preview_on_enter(struct _tg_page* page)
{
	return 0;
}

static int preview_on_leave(struct _tg_page* page)
{
	return 0;
}
*/

static int preview_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	page->usr_data[0] = 120 - ((unsigned int)cur_tm - page->last_time);
	refresh_page();
	return 0;
}

static int preview_on_init(struct _tg_page* page)
{
	page->last_time = time(NULL);
	page->usr_data[0] = 120;
	//play_video();
	return 0;
}

static int preview_on_page_event(int page_event, struct _tg_page* page)
{
	if(UE_USR_END_PREVIEW == page_event){
		end_page();
	}
	return 0;
}

static int preview_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int ret;
	ret = door_close_every();
	if(0 != ret){
		end_page();
	}
	return 0;
}

static int preview_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	char text[32];
	Uint32 color;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	color = SDL_MapRGB(screen->format, 0, 0, 0);
	SDL_FillRect(screen, NULL, color);
	text_line_ex_font(get_font(2), 0, 100, SCREEN_W, SHOW_TEXT2, &ft_color,  1);
	ft_color.r = 255;
	ft_color.g = 0;
	ft_color.b = 0;
	text_line_ex(0, 150, SCREEN_W, SHOW_TEXT3, &ft_color, 1);
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	sprintf(text,"%d", page->usr_data[0]);
	text_line_ex_font(get_font(2),0, 10, SCREEN_W - 10, text, &ft_color,  2);
	return 0;
}

int create_preview_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 8;
	page->pFont = get_font(0);
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_timer = preview_on_timer;
	page->on_init_page = preview_on_init;
	page->on_paint = preview_on_paint;
	page->on_keydown = preview_on_keydown;
	page->on_page_event = preview_on_page_event;
	begin_page(page);
	return 0;
}

//end

