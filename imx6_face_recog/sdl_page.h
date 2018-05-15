/*
* Copyright (c) 2018, 西谷数字
* All rights reserved.
*
* 文件名称：  sdl_page.h
* 文件标识： 
* 摘 要：     
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2018-1-09
*/

#ifndef _SDL_PAGES_H
#define _SDL_PAGES_H

#include "SDL.h"
#include "SDL_ttf.h"

#ifdef __cplusplus
extern "C" {
#endif
#define USR_MSG_KEYDOWN       SDL_KEYDOWN
#define USR_MSG_KEYUP         SDL_KEYUP
#define USR_MSG_DOOR          SDL_USEREVENT + 5
#define USR_MSG_PAINT         SDL_USEREVENT + 6

#define UE_USR_CREATE        1
#define UE_USR_INITPAGE      2
#define UE_USR_ENDPAGE       3
#define UE_USR_PAINT         4     // 不再使用
#define UE_USR_TIMER         5
#define UE_USR_SHOW_CARET    6
#define UE_USR_PAGE_ENTER    7
#define UE_USR_PAGE_LEAVE    8

#define UE_USR_PAGE_EVENT    20


#define DF_TIME_INTERVAL 300
#define MAX_INPUT_NUM   16
#define USR_MAX_STRING  64


#define PAGE_NULL  -2

typedef struct _tg_page
{
	int page_id;
	int timer_id;
	int page_stata;
	int timer_counter;
	int timer_interval;
	int page_abort;
	int cur_dex;
	int cerat_state;   
	char key_char[MAX_INPUT_NUM];
	char usr_title[USR_MAX_STRING];
	char usr_str1[USR_MAX_STRING];
	char usr_str2[USR_MAX_STRING];
	char usr_tmp_str[8][16];
	unsigned int usr_data[8];
	unsigned int last_time;
	SDL_Rect caret;
	TTF_Font* pFont;
	SDL_Surface* background;
	int (*on_keydown)(SDLKey key_code, struct _tg_page* page);
	int (*on_keyup)(SDLKey key_code, struct _tg_page* page);
	int (*on_paint)(SDL_Surface* screen, struct _tg_page* page);
	int (*on_timer)(int id, struct _tg_page* page);
	int (*on_init_page)(struct _tg_page* page);
	int (*on_end_page)(struct _tg_page* page);
	int (*on_page_enter)(struct _tg_page* page);
	int (*on_page_leave)(struct _tg_page* page);
	int (*on_page_event)(int page_event, struct _tg_page* page);
}PAGA_S;

SDL_Surface* get_screen();

SDL_Surface* get_image(int index);

TTF_Font* get_base_font();

int get_small_number_w();
int get_small_number_h();

SDL_Surface* get_opening_image(int index);

int utf_8_getchar(const char *str, int n, uint32_t* chr);

int text_line(SDL_Surface* screen, TTF_Font* font, int x, int y, char* text, SDL_Color* font_color);

int text_line_ex(int x, int y, int w, char* text, SDL_Color* font_color, int style);

int text_line_ex_font(TTF_Font* font, int x, int y, int w, char* text, SDL_Color* font_color, int style);

int number_line(SDL_Surface* screen, int x, int y, char* nums);

int number_line_center(SDL_Surface* screen, int x, int y, int w, char* nums);

int sml_number_line(SDL_Surface* screen, int x, int y, char* nums);

int sml_number_line_center(SDL_Surface* screen, int x, int y, int w, char* nums);
PAGA_S* malloc_page();
	
PAGA_S* current_page();

PAGA_S* get_current_parent_page();
PAGA_S*  get_main_page();

void remove_page(PAGA_S* page);

void begin_page(PAGA_S* page);
void end_page();

void send_page_event(int event_type, unsigned int data);

void refresh_page();

void show_page_caret(PAGA_S* page, int enbale);

int msg_process(SDL_Event* pEven);

void update_operate_last_time(PAGA_S* page);

void clear_paint_event(PAGA_S* page);

#ifdef __cplusplus
}
#endif

#endif //_SDL_DOOR_H
