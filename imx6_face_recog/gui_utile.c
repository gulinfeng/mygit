#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "taskpool.h"
#include "gui_utile.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_image.h"


#define IMG_FACE_BOX   "./pic/facebox.png"
#define IMG_FACE_SCAN_LINE  "./pic/scanline.png"
#define FONT_FULLPATH  "./simsun.ttf"
#define FONT_SIZE1 18
#define FONT_SIZE2 24
#define FONT_SIZE3 30


#define TOP_MARGIN             5
#define BOTTOM_MARGIN          30
#define FACE_CIRCLE_RADIO      100 
#define BODY_W                 300 
#define BODY_H                 120 

#define FACE_BOX_W             230
#define FACE_BOX_H             230
#define FACE_BOX_X             0.5*(SCREEN_W - FACE_BOX_W)
#define FACE_BOX_Y             0

#define FACE_SCAN_LINE_W      180
#define FACE_SCAN_LINE_H      60
#define FACE_SCAN_LINE_X      0.5*(SCREEN_W - FACE_SCAN_LINE_W)

#define EXCEPT_RECT_X1         50
#define EXCEPT_RECT_Y1         5
#define EXCEPT_RECT_X2         280
#define EXCEPT_RECT_Y2         230

#define SCAN_BOX_X   25
#define SCAN_BOX_Y   26
#define SCAN_BOX_W   FACE_SCAN_LINE_W
#define SCAN_BOX_H   172

#define ENROLLED_TEXT_BASE_Y   100

#define CONST_TEXT1   "录制人脸请按#键,呼叫管理中心请按00#,呼叫业主请按房号加#键。"
#define CONST_TEXT2   "人脸录制"
#define CONST_TEXT3   "录制完成"
#define CONST_TEXT4   "录制退出"
#define CONST_TEXT5   "录制超时"
#define CONST_TEXT6   "未匹配"

#define CONST_TEXT7   "夜间模式"

#define CONST_TEXT8   "请按任意键打开灯光!"   //夜间模式下
#define CONST_TEXT9   "夜间模式下请重新录制人脸!"
#define CONST_TEXT10  "请把脸对准框里"

#define CONST_TEXT11  "授权失败"
#define CONST_TEXT12  "请检查是否连接网络和正确配置网关."

#define MARGIN_LEFT   4
#define MARGIN_RIGHT  6

static TTF_Font* g_font[3] = {0};

static SDL_Surface*  g_text_sf[10] ={0};

static SDL_Surface*  g_png[2] = {0};

static int g_mv_tx_w = 0;
static int g_mv_tx_h = 0;

TTF_Font* get_font(int index)
{
	if(index < 0 && index > 2){
		return NULL;
	}
	return g_font[index];
}

int init_gui_utile()
{	
	int i;
	SDL_Color font_color;
	SDL_Surface* pTemp = NULL;
	
	/* Initialize the TTF library */
	if ( TTF_Init() < 0 ) {
		fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
		return -1;
	}
	g_font[0] = TTF_OpenFont(FONT_FULLPATH, FONT_SIZE1);
	if ( g_font[0] == NULL ) {
		printf("Couldn't load %d pt font from %s: %s\n",
					FONT_SIZE1, FONT_FULLPATH, SDL_GetError());
	}
	g_font[1] = TTF_OpenFont(FONT_FULLPATH, FONT_SIZE2);
	if ( g_font[1] == NULL ) {
		printf("Couldn't load %d pt font from %s: %s\n",
					FONT_SIZE2, FONT_FULLPATH, SDL_GetError());
	}
	g_font[2] = TTF_OpenFont(FONT_FULLPATH, FONT_SIZE3);
	if ( g_font[2] == NULL ) {
		printf("Couldn't load %d pt font from %s: %s\n",
					FONT_SIZE3, FONT_FULLPATH, SDL_GetError());
	}
	
	pTemp = IMG_Load(IMG_FACE_BOX);
	if(!pTemp)
	{
		printf("IMG_Load ./pic/facebox.png : %s\n", IMG_GetError());
	}
	g_png[0] = SDL_DisplayFormatAlpha(pTemp);
	SDL_FreeSurface(pTemp);

	pTemp = IMG_Load(IMG_FACE_SCAN_LINE);
	if(!pTemp)
	{
		printf("IMG_Load:./pic/scanline.png  %s\n", IMG_GetError());
	}
	g_png[1] = SDL_DisplayFormatAlpha(pTemp);
	SDL_FreeSurface(pTemp);

	
	font_color.r = 255;
	font_color.g = 255;
	font_color.b = 255;
	
	g_text_sf[0] = TTF_RenderUTF8_Blended(g_font[0], CONST_TEXT7, font_color);
	assert(g_text_sf[0]);

	TTF_SizeUTF8(g_font[0], CONST_TEXT1, &g_mv_tx_w, &g_mv_tx_h);

	g_mv_tx_w += SCREEN_W;
	
	g_text_sf[1] = TTF_RenderUTF8_Blended(g_font[0], CONST_TEXT2, font_color);
	assert(g_text_sf[1]);
	
	font_color.r = 0;
	font_color.g = 0;
	font_color.b = 255;
	g_text_sf[2] = TTF_RenderUTF8_Blended(g_font[1], CONST_TEXT3, font_color);
	assert(g_text_sf[2]);

	font_color.r = 255;
	font_color.g = 0;
	font_color.b = 0;
	g_text_sf[3] = TTF_RenderUTF8_Blended(g_font[1], CONST_TEXT4, font_color);
	assert(g_text_sf[3]);
	g_text_sf[4] = TTF_RenderUTF8_Blended(g_font[1], CONST_TEXT5, font_color);
	assert(g_text_sf[4]);

	font_color.r = 255;
	font_color.g = 255;
	font_color.b = 255;
	
	//g_text_sf[5] = TTF_RenderUTF8_Blended(g_font[0], CONST_TEXT7, font_color);
	//assert(g_text_sf[5]);

/*	
	g_text_sf[6] = TTF_RenderUTF8_Blended(g_font[1], CONST_TEXT8, font_color);
	assert(g_text_sf[6]);

	font_color.r = 255;
	font_color.g = 120;
	font_color.b = 80;
	
	g_text_sf[7] = TTF_RenderUTF8_Blended(g_font[0], CONST_TEXT9, font_color);
	assert(g_text_sf[7]);
*/	
	return 0;
}

void uninit_gui_utile()
{
	int i;
	for(i = 0; i < 3; i++){
		if(g_font[i]){
			TTF_CloseFont(g_font[i]);
		}
	}
	for(i = 0; i < 2; i++){
		if(g_png[i]){
			SDL_FreeSurface(g_png[i]);
		}
	}
	
	for(i = 0; i < 10; i++){
		if(g_text_sf[i]){
			SDL_FreeSurface(g_text_sf[i]);
		}
	}
	
	TTF_Quit();
	return;
}

int is_in_except_rect(int x, int y, int w, int h)
{
	if((EXCEPT_RECT_X1 < x)&&( (x + w) < EXCEPT_RECT_X2)) {
		if((EXCEPT_RECT_Y1 < y)&&(y + h) < EXCEPT_RECT_Y2){
			return 1;
		}
	}
	return 0;
}

Uint32 make_rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a;
}

void draw_outline(SDL_Surface* screen, Uint32 color)
{
	//Uint32 color = 0;
	//color = make_rgba(255,0,0,255);
	circleColor(screen, 160, TOP_MARGIN + FACE_CIRCLE_RADIO, FACE_CIRCLE_RADIO - 1, color);
	circleColor(screen, 160, TOP_MARGIN + FACE_CIRCLE_RADIO, FACE_CIRCLE_RADIO, color);
	//ellipseColor(screen, 160, TOP_MARGIN + 2*FACE_CIRCLE_RADIO + BODY_H/2, BODY_W/2 - 1, BODY_H/2 - 1, color);
	ellipseColor(screen, 160, TOP_MARGIN + 2*FACE_CIRCLE_RADIO + BODY_H/2, BODY_W/2, BODY_H/2, color);
	//roundedRectangleColor(screen, (WIDTH - BODY_W)/2,TOP_MARGIN + 2*FACE_CIRCLE_RADIO, (WIDTH - BODY_W)/2 + BODY_W, \
	//TOP_MARGIN + 2*FACE_CIRCLE_RADIO + BODY_H, 20, color);
}

void draw_scan_line(SDL_Surface *screen, int y)
{
	SDL_Rect DstRect;
	DstRect.x = FACE_SCAN_LINE_X;
	DstRect.y = y;
	DstRect.w = FACE_SCAN_LINE_W;
	DstRect.h = FACE_SCAN_LINE_H;
	SDL_BlitSurface(g_png[1],NULL, screen,&DstRect);
}

static float in_out_cubic(float process)
{
	float p = 2*process;
	if(p < 1.0){
		return (0.5 * p * p * p);
	}
	p -= 2.0;
    return 0.5 * (p * p * p + 2.0);
}

void draw_record_face(SDL_Surface *screen,  int counter, float process)
{
	int scan_y;
	SDL_Color font_color;
	SDL_Rect DstRect;
	SDL_Rect ClipRect;
	SDL_Surface* ctr_sf = NULL;
	char str_cnt[8];
/*	
	DstRect.x = MARGIN_LEFT;
	DstRect.y = MARGIN_LEFT;
	DstRect.w = g_text_sf[1]->w;
	DstRect.h = g_text_sf[1]->h;
	SDL_BlitSurface(g_text_sf[1],NULL, screen,&DstRect);
*/

	sprintf(str_cnt,"%d", counter);
	font_color.r = 255;
	font_color.g = 120;
	font_color.b = 80;
	ctr_sf = TTF_RenderUTF8_Blended(g_font[2], str_cnt, font_color);
	if(ctr_sf){
		DstRect.w = ctr_sf->w;
		DstRect.h = ctr_sf->h;
		DstRect.x =  SCREEN_W - MARGIN_RIGHT - DstRect.w;
		DstRect.y =  MARGIN_RIGHT;
		SDL_BlitSurface(ctr_sf,NULL, screen,&DstRect);
		SDL_FreeSurface(ctr_sf);
	}

	DstRect.x = FACE_BOX_X;
	DstRect.y = FACE_BOX_Y;
	DstRect.w = FACE_BOX_W;
	DstRect.h = FACE_BOX_H;
	SDL_BlitSurface(g_png[0],NULL, screen,&DstRect);

	ClipRect.x = FACE_BOX_X + SCAN_BOX_X;
	ClipRect.y = FACE_BOX_Y + SCAN_BOX_Y - 2;
	ClipRect.w = SCAN_BOX_W;
	ClipRect.h = SCAN_BOX_H + 2;
	SDL_SetClipRect(screen, &ClipRect);
	
	scan_y =  FACE_BOX_Y + SCAN_BOX_Y - 2 + SCAN_BOX_H*(1.0 - in_out_cubic(process));
	DstRect.x = FACE_SCAN_LINE_X;
	DstRect.y = scan_y;
	DstRect.w = FACE_SCAN_LINE_W;
	DstRect.h = FACE_SCAN_LINE_H;
	SDL_BlitSurface(g_png[1],NULL, screen,&DstRect);

	SDL_SetClipRect(screen, NULL);

	text_line_ex(0, SCREEN_H - 22, SCREEN_W, (char*)CONST_TEXT10, &font_color, 1);
	return;
}

void draw_licence_error(int code)
{
	/*
	char str_error[32];
	switch(code)
	{
		case -1:
			break;
		case -2:
			break;
		case -3:
			break;
		default:
			break;
	}
	*/
	SDL_Color font_color;
	font_color.r = 255;
	font_color.g = 120;
	font_color.b = 80;
	text_line_ex_font(g_font[2], 0, 82, SCREEN_W, (char*)CONST_TEXT11, &font_color, 1);

	font_color.r = 135;
	font_color.g = 135;
	font_color.b = 135;
	text_line_ex( 0, 130, SCREEN_W, (char*)CONST_TEXT12, &font_color, 1);
	return;
}

void draw_process_move_text(int model, float process)
{
	int x,y;
	SDL_Color font_color;
	font_color.r = 255;
	font_color.g = 120;
	font_color.b = 80;
	y = SCREEN_H - 20;
	if(model > 0){
		font_color.r = 255;
		font_color.g = 255;
		font_color.b = 255;
		y = MARGIN_LEFT;
	}
	x = SCREEN_W - (int)(process*g_mv_tx_w);
	text_line_ex(x, y, 0, (char*)CONST_TEXT1, &font_color, 0);
}

void draw_recognization(SDL_Surface *screen, int model, float process)
{
	SDL_Rect DstRect;
	SDL_Color font_color;
	if(model > 0){
		font_color.r = 255;
		font_color.g = 120;
		font_color.b = 80;
		text_line_ex(0, SCREEN_H - 30, SCREEN_W, (char*)CONST_TEXT9, &font_color, 1);
	}
	if(2 == model){
		DstRect.w = g_text_sf[0]->w;
		DstRect.h = g_text_sf[0]->h;
		DstRect.x =  (SCREEN_W - DstRect.w) / 2;
		DstRect.y =  75;
		SDL_BlitSurface(g_text_sf[0], NULL, screen, &DstRect);
		
		font_color.r = 255;
		font_color.g = 255;
		font_color.b = 255;
		text_line_ex_font(g_font[1], 0, 100, SCREEN_W, (char*)CONST_TEXT8, &font_color, 1);
	}
	draw_process_move_text(model, process);
	return;
}

void draw_record_finished(SDL_Surface *screen, int ret_id, int id)
{
	SDL_Color font_color;
	SDL_Rect DstRect;
	SDL_Surface* ctr_sf = NULL;
	SDL_Surface* const_sf = NULL;
	char str_cnt[16];
	switch(ret_id)
	{
		case 0:    //完成
			sprintf(str_cnt,"%d", id);
			font_color.r = 255;
			font_color.g = 255;
			font_color.b = 255;
			ctr_sf = TTF_RenderUTF8_Blended(g_font[2], str_cnt, font_color);
			if(ctr_sf){
				DstRect.w = ctr_sf->w;
				DstRect.h = ctr_sf->h;
				DstRect.x =  (SCREEN_W - DstRect.w) / 2;
				DstRect.y =  (ENROLLED_TEXT_BASE_Y + 20 - DstRect.h);
				SDL_BlitSurface(ctr_sf,NULL, screen,&DstRect);
				SDL_FreeSurface(ctr_sf);
			}
			const_sf = g_text_sf[2];
			DstRect.w = const_sf->w;
			DstRect.h = const_sf->h;
			DstRect.y = ENROLLED_TEXT_BASE_Y + 25;
			break;
			
		case 2:    //退出
			const_sf = g_text_sf[3];
			DstRect.w = const_sf->w;
			DstRect.h = const_sf->h;
			DstRect.y = ENROLLED_TEXT_BASE_Y;
			break;

		case 3:    //超时
			const_sf = g_text_sf[4];
			DstRect.w = const_sf->w;
			DstRect.h = const_sf->h;
			DstRect.y = ENROLLED_TEXT_BASE_Y;
			break;
	}
	
	DstRect.x =  (SCREEN_W- DstRect.w) / 2;
	if(const_sf){
		SDL_BlitSurface(const_sf,NULL, screen,&DstRect);
	}
	return;
}


void draw_face_info(SDL_Surface *screen, int x1,int y1, int width, int height, float confidence, unsigned long id, char* name)
{
	int fail_flg;
	int max_width;
	Uint32 color;
	SDL_Color font_color;
	SDL_Rect DstRect;
	SDL_Surface* text_sf[3];
	char str_cnt[32];
	text_sf[0] = NULL;
	text_sf[1] = NULL;
	text_sf[2] = NULL;
	fail_flg = 0;
	max_width = 0;
	font_color.r = 0;
	font_color.g = 255;
	font_color.b = 0;
	color = make_rgba(0,255,0,255);
	if(((int)id) < 0){
		fail_flg = 1;
		font_color.r = 255;
		font_color.g = 0;
		font_color.b = 0;
		color = make_rgba(255,0,0,255);
	}
	rectangleColor(screen, x1, y1, x1 + width, y1 + height, color);
	sprintf(str_cnt,"%d", (int)id);
	if(fail_flg){
		sprintf(str_cnt,"%s", CONST_TEXT6);
	}
	text_sf[0] = TTF_RenderUTF8_Blended(g_font[0], str_cnt, font_color);
	if(text_sf[0]){
		max_width = text_sf[0]->w;
	}
	if(0 == fail_flg){
		sprintf(str_cnt,"%.2f", confidence);
		text_sf[1] = TTF_RenderUTF8_Blended(g_font[0], str_cnt, font_color);
		if(text_sf[1]){
			max_width = text_sf[1]->w > max_width ?  text_sf[1]->w : max_width;
		}
		if(name){
			if(strlen(name) > 0){
				sprintf(str_cnt,"%s", name);
				text_sf[2] = TTF_RenderUTF8_Blended(g_font[0], str_cnt, font_color);
				if(text_sf[2]){
					max_width = text_sf[2]->w > max_width ?  text_sf[2]->w : max_width;
				}
			}
		}
	}
	fail_flg = (x1 + width + 2 + max_width) >  SCREEN_W ? 1 : 0;
	if(text_sf[0]){
		DstRect.w = text_sf[0]->w;
		DstRect.h = text_sf[0]->h;
		DstRect.y = y1;
		if( 0 == fail_flg ){
			DstRect.x = x1 + width + 2;
		}else {
			DstRect.x = x1 - DstRect.w;
		}
		SDL_BlitSurface(text_sf[0],NULL, screen,&DstRect);
		SDL_FreeSurface(text_sf[0]);
	}
	if(text_sf[1]){
		DstRect.w = text_sf[1]->w;
		DstRect.h = text_sf[1]->h;
		DstRect.y = y1 + text_sf[0]->h + 2;
		if( 0 == fail_flg ){
			DstRect.x = x1 + width + 2;
		}else {
			DstRect.x = x1 - DstRect.w;
		}
		SDL_BlitSurface(text_sf[1],NULL, screen,&DstRect);
		SDL_FreeSurface(text_sf[1]);
	}
	if(text_sf[2]){
		DstRect.w = text_sf[2]->w;
		DstRect.h = text_sf[2]->h;
		DstRect.y = y1 + 2*text_sf[0]->h + 4;
		if( 0 == fail_flg ){
			DstRect.x = x1 + width + 2;
		}else {
			DstRect.x = x1 - DstRect.w;
		}
		SDL_BlitSurface(text_sf[2],NULL, screen,&DstRect);
		SDL_FreeSurface(text_sf[2]);
	}
	return 0;
}


