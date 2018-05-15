/**** implement sdl page ****/
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<assert.h>
#include<time.h>
#include "SDL_image.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"
#include "gui_utile.h"
#include "taskpool.h"
#include "sdl_page.h"

#define PCHARWIDTH		22
#define PCHARHEIGHT		28

#define SCHARWIDTH		17
#define SCHARHEIGHT		22

#define FONT_SIZE        18

//#define  CLOCK		"pic/clock.jpg"
#define  MAIN			"pic/main.jpg"
#define  CALL			"pic/call.jpg"
#define  MAKECALL		"pic/makecall.jpg"
#define  PAGEERR	    "pic/makecallerr.jpg"
#define  POPEN			"pic/open.jpg"
#define  POPENERR		"pic/openerr.jpg"
#define  CHANGE			"pic/change.jpg"
//#define  SAVEAV		"pic/saveav.jpg"
//#define  SAVEAVING	"pic/saveaving.jpg"
#define  TEST			"pic/testback.jpg"
#define  LKE            "pic/linkerr.gif" 
#define  LNK            "pic/link.gif"
#define  SVE            "pic/servererr.gif"
#define  SVR            "pic/server.gif" 

#define  IMG_CARD       "pic/card.jpg"

#define NUMBER_IMG      "./28w.bmp"

#define DF_CARET_W     1
#define DF_CARET_H     SCHARHEIGHT

#define DOOR_PIC_NUM      16
#define MAX_PAGE_NUM       8
#define MAX_OPENING_IMG    7

typedef struct _tg_page_stack
{
	int  cur_dex;
	PAGA_S* pages[MAX_PAGE_NUM];

}PAGE_STACK_S;

typedef struct _tg_resource
{
	float number_small_w;
	float number_small_h;
	float number_factor_x;
	float number_factor_y;
	TTF_Font* base_font[4];
	SDL_Surface* screen;
	SDL_Surface* number_font;
	SDL_Surface* number_small_font;
	SDL_Surface* surface[DOOR_PIC_NUM];
	SDL_Surface* opening[MAX_OPENING_IMG];
}RESOURCE_S;


static PAGE_STACK_S g_page_stack = {0};
static RESOURCE_S g_ui_resource = {0};

#define ROUND(x)  (int)(x + 0.5)

SDL_Surface* get_screen()
{
	return g_ui_resource.screen;
}

SDL_Surface* get_image(int index)
{
	if((0 <= index)&&(index < DOOR_PIC_NUM)){
		return g_ui_resource.surface[index];
	}
	return NULL;
}

SDL_Surface* get_opening_image(int index)
{
	if((0 <= index)&&(index < MAX_OPENING_IMG)){
		return g_ui_resource.opening[index];
	}
	return NULL;
}

int get_small_number_w()
{
	return ROUND(g_ui_resource.number_small_w);
}

int get_small_number_h()
{
	return ROUND(g_ui_resource.number_small_h);
}

TTF_Font* get_base_font()
{
	return g_ui_resource.base_font[0];
}
	
int text_line(SDL_Surface* screen, TTF_Font* font, int x, int y, char* text, SDL_Color* font_color)
{
	SDL_Rect DstRect;
	SDL_Surface* sf_text = TTF_RenderUTF8_Blended(font, text, *font_color);
	DstRect.x = x;
	DstRect.y = y;
	DstRect.w = sf_text->w;
	DstRect.h = sf_text->h;
	SDL_BlitSurface(sf_text,NULL, screen, &DstRect);
	SDL_FreeSurface(sf_text);
	return (DstRect.x + sf_text->w);
}

int text_line_ex_font(TTF_Font* font, int x, int y, int w, char* text, SDL_Color* font_color, int style)
{
	int text_w, text_h;
	if(0 == style){
		//left
		return text_line(g_ui_resource.screen,font, x,y,text,font_color);
	}else if(1 == style){
		//center
		TTF_SizeUTF8(font, text, &text_w, &text_h);
		return text_line(g_ui_resource.screen,font, x + 0.5*(w - text_w),y,text,font_color);
	}else if(2 == style){
	    //right
	    TTF_SizeUTF8(font, text, &text_w, &text_h);
		return text_line(g_ui_resource.screen,font, (x + w - text_w),y,text,font_color);
	}
	return x;
}

int text_line_ex(int x, int y, int w, char* text, SDL_Color* font_color, int style)
{
	return text_line_ex_font(g_ui_resource.base_font[0], x,y,w,text,font_color,style);
}


static uint32_t getchar_value(const char *str, int ch_len)
{
    char* pt;
    uint32_t ch = 0xFFFD;
    int left = ch_len;
    if (ch_len == 6) {
        ch = (uint32_t)(str[0] & 0x01); }
    else if (ch_len == 5) {
        ch = (uint32_t)(str[0] & 0x03); }
    else if ( ch_len == 4) {
        ch = (uint32_t)(str[0] & 0x07); }
    else if ( ch_len == 3) {
        ch = (uint32_t)(str[0] & 0x0F); }
    else if ( ch_len == 2) {
        ch = (uint32_t)(str[0] & 0x1F); }
    else if ( ch_len == 1) {
        ch = (uint32_t)str[0]; }
    left = ch_len - 1;
    pt = (char*)str + 1;
    while (left > 0) {
        ch <<= 6;
        ch |=  (pt[0] & 0x3F);
        pt += 1;
        left -= 1;
    }
    return ch;
}

int utf_8_getchar(const char *str, int n, uint32_t* chr)
{
    unsigned char c_v = 0;
    if (n < strlen(str)) {
        chr[0] = 0xFFFD;
        return -1;
    }
    c_v = str[0];
    if (c_v < 128) {
        chr[0] = getchar_value(str, 1);
        return 1;
	}
    if (c_v < 192) {
        return 0;
    }
    if (c_v < 224) {
        chr[0] = getchar_value(str, 2);
        return 2;
    }
    if (c_v < 240) {
        chr[0] = getchar_value(str, 3);
        return 3;
    }
    if (c_v < 248) {
        chr[0] = getchar_value(str, 4);
        return 4;
    }
    if (c_v < 252) {
        chr[0] = getchar_value(str, 5);
        return 5;
    }
    if (c_v < 254) {
        chr[0] = getchar_value(str, 6);
        return 6;
    }
    return 0;
}

#ifdef TEXT_RECTANGLE
int text_rectangle(SDL_Surface* screen, TTF_Font* font, int x, int y, int w, int h, char* text, SDL_Color font_color, int style)
{
	int i;
	int max_chr_w, max_chr_h;
	int text_w, text_h;
	int text_len = 0;
	int line_num;
	int one_line_char = 0;
	int line_chars[8] = {0};
	char* pTem;
	SDL_Rect DstRect;
	SDL_Surface* sf_text;
	char* text_buf = NULL;
	TTF_SizeUTF8(font, text, &text_w, &text_h);
	if(text_w < w){
		sf_text = TTF_RenderUTF8_Blended(font, text, font_color);
		if(0 == style){
			//左对齐
			DstRect.x = x;
			DstRect.y = y;
			DstRect.w = sf_text->w;
			DstRect.h = sf_text->h;
		}
		if(1 == style){
			//剧中
			
			DstRect.w = sf_text->w;
			DstRect.h = sf_text->h;
			DstRect.x = x + 0.5*(w - DstRect.w);
			DstRect.y = y + 0.5*(h - DstRect.h);
		}
		SDL_BlitSurface(sf_text, NULL, screen, &DstRect);
		return 0;
	}
	/*
	TTF_SizeUTF8(font,"中",&max_chr_w, &max_chr_h);
	text_len = strlen(text);
	text_buf = (char*)malloc(text_len + 1);
	if(!text_buf){
		printf("malloc text buffer failed\n");
		return 0;
	}
	memset(text_buf, 0, text_len + 1);
	one_line_char = w / max_chr_w;
	for(i = 0; i < one_line_char; i++){
		
	}
	*/
	return 0;
}
#endif

static int get_num_index(char c)
{
	if('*' == c){
		return 10;
	}
	if('.' == c){
		return 11;
	}
	if(('0' <= c) && (c <= '9')){
		return (c - '0');
	}
	return -1;
}



int inner_number_line(SDL_Surface* screen, int x, int y, char* nums, int font)
{
	char* pNum = nums;
	int index = 0;
	float caret = 0.0;
	float char_w;
	float char_h;
	SDL_Rect src_rect;
	SDL_Rect dest_rect;
	SDL_Surface* pNum_sf = g_ui_resource.number_font;
	char_w = (float)PCHARWIDTH;
	char_h = (float)PCHARHEIGHT;
	if(0 != font){
		pNum_sf = g_ui_resource.number_small_font;
		char_w = g_ui_resource.number_small_w;
		char_h = g_ui_resource.number_small_h;
	}
	caret = (float)x;
	src_rect.y = 0;
	src_rect.w = ROUND(char_w);
	src_rect.h = ROUND(char_h);
	dest_rect.w = src_rect.w;
	dest_rect.h = src_rect.h;
	dest_rect.y = y;
	while(*pNum != '\0'){
		index = get_num_index(*pNum);
		if(index > -1){
			src_rect.x = ROUND(index*char_w);
			dest_rect.x = ROUND(caret);
			SDL_BlitSurface(pNum_sf,&src_rect, screen, &dest_rect);
			caret += char_w;
		}
		pNum++;
	}
	return ROUND(caret);
}

int number_line(SDL_Surface* screen, int x, int y, char* nums)
{
	return inner_number_line(screen, x, y, nums, 0);
}

int number_line_center(SDL_Surface* screen, int x, int y, int w, char* nums)
{
	int char_len = strlen(nums);
	int center_x = x + 0.5*(w - char_len*PCHARWIDTH);
	return number_line(screen,center_x, y, nums);
}

int sml_number_line(SDL_Surface* screen, int x, int y, char* nums)
{
	int mod_y = y + 0.5*(PCHARWIDTH - ROUND(g_ui_resource.number_small_h));
	return inner_number_line(screen, x, mod_y, nums, 1);
}

int sml_number_line_center(SDL_Surface* screen, int x, int y, int w, char* nums)
{
	int char_len = strlen(nums);
	int center_x = x + 0.5*(w - char_len*SCHARWIDTH);
	int mod_y = y + 0.5*(PCHARWIDTH - ROUND(g_ui_resource.number_small_h));
	return sml_number_line(screen,center_x, mod_y, nums);
}

PAGA_S* malloc_page()
{
	PAGA_S* page = (PAGA_S*) malloc(sizeof(PAGA_S));
	if(NULL == page){
		printf("create_page failed\n");
		return page;
	}
	memset(page, 0, sizeof(PAGA_S));
	page->caret.w = DF_CARET_W;
	page->caret.h = DF_CARET_H;
	page->timer_id = -1;
	return page;
}
static void push_page(PAGA_S* page)
{
	g_page_stack.pages[g_page_stack.cur_dex] = page;
	g_page_stack.cur_dex++;
	return;
}
/*
void remove_page(PAGA_S* page)
{
	int i = 0;
	
	for(i = 0; i < g_page_stack.cur_dex; i++)
	{
		if(page == g_page_stack.pages[i]){
			memmov(&g_page_stack.pages[i], &g_page_stack.pages[i + 1], g_page_stack.cur_dex - i);
			g_page_stack.cur_dex--;
			break;
		}
	}
}
*/
PAGA_S* current_page()
{
	if(g_page_stack.cur_dex < 1){
		return NULL;
	}
	return g_page_stack.pages[g_page_stack.cur_dex - 1];
}

PAGA_S* get_current_parent_page()
{
	if(g_page_stack.cur_dex < 2){
		return NULL;
	}
	return g_page_stack.pages[g_page_stack.cur_dex - 2];
}
PAGA_S*  get_main_page()
{
	return g_page_stack.pages[0];
}

static void pop_page()
{
	
	g_page_stack.pages[g_page_stack.cur_dex] = NULL;
	g_page_stack.cur_dex--;
	return;
}

void clear_paint_event(PAGA_S* page)
{
	int found, i;
	SDL_Event events[10];
	do{
		found = SDL_PeepEvents(events, 10, SDL_GETEVENT, SDL_EVENTMASK(USR_MSG_PAINT));
		//printf("DEBUG:remove USR_MSG_PAINT :%d\n",found);
	}while (found >= 10);
}

static void destroy_page(PAGA_S* page)
{
	//清楚SDLevent队列中的USR_MSG_DOOR
	int found, i;
	SDL_Event events[16];
	do{
		found = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_EVENTMASK(USR_MSG_DOOR));
		for(i = 0; i < found; i++){
			if(page == (PAGA_S*)(events[i].user.data1)){
				printf("INFO: remove USR_MSG_DOOR: %d\n",events[i].user.code);
			}else{
				SDL_PushEvent(&events[i]);
			}
		}
	}while (found >= 16);
	free(page);
	pop_page();
	return;
}

void show_page_caret(PAGA_S* page, int enbale)
{
	page->cerat_state = (0 == enbale) ?  0 : 1;
}

void send_page_event(int event_type, unsigned int data)
{
	SDL_Event event;
	event.type = USR_MSG_DOOR;
	event.user.code = event_type;
	event.user.data1= (void*)data;
	SDL_PushEvent(&event);
}

void begin_page(PAGA_S* page)
{
	send_page_event(UE_USR_CREATE, (unsigned int)page);
}
void end_page()
{
	send_page_event(UE_USR_ENDPAGE, (unsigned int)current_page());
}

void refresh_page()
{
	SDL_Event event;
	event.type = USR_MSG_PAINT;
	event.user.code = UE_USR_PAINT;
	event.user.data1= (void*)current_page();
	SDL_PushEvent(&event);
	//send_page_event(UE_USR_PAINT, (unsigned int)current_page());
}

static void refresh(PAGA_S* page)
{
	SDL_Event event;
	event.type = USR_MSG_PAINT;
	event.user.code = UE_USR_PAINT;
	event.user.data1= (void*)page;
	SDL_PushEvent(&event);
}

static int timer_cb(int id, int interval)
{
	PAGA_S* page = current_page();
	if(page){
		send_page_event(UE_USR_TIMER, (unsigned int)page);
	}
	return 1;
}

static void start_timer(PAGA_S* page)
{
	if(page->timer_interval <= 0 && page->cerat_state > 0){
		page->timer_interval = DF_TIME_INTERVAL;
	}
	if(page->timer_interval > 0){
		page->timer_id = task_timer(page->timer_interval,timer_cb);
	}
	page->timer_counter = 0;
}

static int msg_door_process(SDL_Event* pEven)
{
	int ret = 0;
	//time_t cut_tm;
	PAGA_S* page = NULL;
	switch(pEven->user.code)
	{
		case UE_USR_CREATE:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				PAGA_S* last_page =current_page();
				send_page_event(UE_USR_PAGE_LEAVE, (unsigned int)last_page);
				send_page_event(UE_USR_INITPAGE, (unsigned int)page);
			}
			break;

		case UE_USR_INITPAGE:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				push_page(page);
				if(page->on_init_page){
					ret = page->on_init_page(page);
				}
				send_page_event(UE_USR_PAGE_ENTER, (unsigned int)page);
			}
			break;

		case UE_USR_PAGE_ENTER:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				if(1 == page->page_abort){
					send_page_event(UE_USR_ENDPAGE, (unsigned int)page);
					break;
				}
				page->last_time = (unsigned int)time(NULL);
				if(page->on_page_enter){
					ret = page->on_page_enter(page);
				}
				start_timer(page);
				//send_page_event(UE_USR_PAINT, (unsigned int)page);
				refresh(page);
			}
			break;

		case UE_USR_PAGE_LEAVE:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				task_cancel_timer(page->timer_id);
				if(page->on_page_leave){
					ret = page->on_page_leave(page);
				}
			}
			break;

		case UE_USR_ENDPAGE:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				task_cancel_timer(page->timer_id);
				if(page->on_end_page){
					ret = page->on_end_page(page);
				}
				printf("destroy page---id=%d-\n", page->page_id);
				destroy_page(page);
				page = current_page();
				if(NULL == page){
					ret = PAGE_NULL;
					break;
				}
				send_page_event(UE_USR_PAGE_ENTER, (unsigned int)page);
			}
			break;

		case UE_USR_PAINT:
			break;

		case UE_USR_SHOW_CARET:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				int x1,x2,y1,y2;
				Uint32 color = make_rgba(255,255,255,255);
				if(1 == page->cerat_state){
					color = make_rgba(50,50,50,255);
				}
				x1 = page->caret.x;
				x2 = page->caret.x + page->caret.w;
				y1 = page->caret.y;
				y2 = page->caret.y + page->caret.h;
				rectangleColor(g_ui_resource.screen,x1,y1,x2,y2,color);
				SDL_Flip(g_ui_resource.screen);
			}
			break;

		case UE_USR_TIMER:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				if(page->on_timer){
					ret = page->on_timer(page->timer_id, page);
					page->timer_counter++;
					if(0 == page->timer_counter % 2){
						if(page->cerat_state > 0){
							if(1 == page->cerat_state){
								page->cerat_state = 2;
							}else if(2 == page->cerat_state){
								page->cerat_state = 1;
							}
							send_page_event(UE_USR_SHOW_CARET, (unsigned int)page);
						}
					}
				}
			}
			break;
		default:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				if(page->on_page_event){
					page->on_page_event(pEven->user.code, page);
				}
			}
			break;
	}
	return ret;
}

void update_operate_last_time(PAGA_S* page)
{
	page->last_time = (unsigned int)time(NULL);
}

int msg_process(SDL_Event* pEven)
{
	int ret = -1;
	PAGA_S* page = NULL;
	//printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	switch (pEven->type)
    {	
    	
		case USR_MSG_KEYDOWN:
			page = current_page();
			if(page){
				ret = 0;
				if(page->on_keydown){
					ret = page->on_keydown(pEven->key.keysym.sym,page);
				}
				page->last_time = (unsigned int)time(NULL);
			}
			break;

		case USR_MSG_KEYUP:
			page = current_page();
			if(page){
				ret = 0;
				if(page->on_keyup){
					ret = page->on_keyup(pEven->key.keysym.sym,page);
				}
			}
			break;

		case USR_MSG_PAINT:
			page = (PAGA_S*)pEven->user.data1;
			if(page){
				if(page->background){
					SDL_BlitSurface(page->background, NULL, g_ui_resource.screen, NULL);
				}
				if(page->on_paint){
					ret = page->on_paint(g_ui_resource.screen, page);
				}
				SDL_Flip(g_ui_resource.screen);
				if(page->cerat_state > 0){
					send_page_event(UE_USR_SHOW_CARET, (unsigned int)page);
				}
			}
			break;

		case USR_MSG_DOOR:
			//printf("DEBUG: recieving usr msg door-code=%d-\n",pEven->user.code);
			ret = msg_door_process(pEven);
			break;

    }
	return ret;
}

int load_door_resource(SDL_Surface* screen)
{
	int i;
	char str[16];
	SDL_Surface* pTemp = NULL;
	SDL_Surface* tmp_sf_2 = NULL;
	assert(screen);
	g_ui_resource.screen = screen;
	g_ui_resource.base_font[0] = get_font(0); // TTF_OpenFont(FONT_FULLPATH, FONT_SIZE);
	if ( g_ui_resource.base_font[0] == NULL ) {
		printf("ERROR: get font fialed\n");
	}
	g_ui_resource.base_font[1] = get_font(1);
	g_ui_resource.base_font[2] = get_font(2);

	for(i=0;i<7;i++)
	{
		sprintf(str,"pic/opening_%d.jpg",i+1);
		pTemp = IMG_Load(str);
		if(!pTemp)
		{
			/* image failed to load */
			printf("IMG_Load: pic/opening_ %s\n", IMG_GetError());
			printf(" load  %s  failed \n", str);
			return 13;
		}
		g_ui_resource.opening[i]= SDL_DisplayFormat(pTemp);
		SDL_FreeSurface(pTemp);	
	}

	pTemp = IMG_Load(CALL);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/call.jpg %s\n", IMG_GetError());
		return 1;
	}
	g_ui_resource.surface[0] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(MAKECALL);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/makecall.jpg %s\n", IMG_GetError());
		return 2;
	}
	g_ui_resource.surface[1] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(PAGEERR);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/makecallerr.jpg %s\n", IMG_GetError());
		return 3;
	}
	g_ui_resource.surface[2] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(POPEN);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/open.jpg %s\n", IMG_GetError());
		return 4;
	}
	g_ui_resource.surface[3] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(POPENERR);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/openerr.jpg %s\n", IMG_GetError());
		return 5;
	}
	g_ui_resource.surface[4] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(CHANGE);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/change.jpg %s\n", IMG_GetError());
		return 6;
	}
	g_ui_resource.surface[5] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(TEST);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/testback.jpg %s\n", IMG_GetError());
		return 7;
	}
	float f_x =   320.0 / (float)pTemp->w;
	float f_y =   240.0 / (float)pTemp->h;
	tmp_sf_2 = zoomSurface(pTemp, f_x, f_y, SMOOTHING_ON);
	SDL_FreeSurface(pTemp);
	g_ui_resource.surface[6] = SDL_DisplayFormat(tmp_sf_2);
	SDL_FreeSurface(tmp_sf_2);
	
	pTemp = IMG_Load(LKE);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/linkerr.gif %s\n", IMG_GetError());
		return 8;
	}
	g_ui_resource.surface[7] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(LNK);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/link.gif %s\n", IMG_GetError());
		return 9;
	}
	g_ui_resource.surface[8] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(SVE);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/servererr.gif %s\n", IMG_GetError());
		return 10;
	}
	g_ui_resource.surface[9] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	pTemp = IMG_Load(SVR);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/server.gif %s\n", IMG_GetError());
		return 11;
	}
	g_ui_resource.surface[10] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);

	pTemp = IMG_Load(IMG_CARD);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/card.jpg %s\n", IMG_GetError());
		return 11;
	}
	g_ui_resource.surface[11] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);

	pTemp = IMG_Load(MAIN);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: pic/main.jpg %s\n", IMG_GetError());
		return 13;
	}
	g_ui_resource.surface[12] = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	int numner_sml_w;
	int numner_sml_h;
	g_ui_resource.number_small_w = 0.0;
	g_ui_resource.number_small_h = 0.0;
	g_ui_resource.number_factor_x = ((float)SCHARWIDTH) / ((float)PCHARWIDTH); 
	g_ui_resource.number_factor_y = ((float)SCHARHEIGHT) / ((float)PCHARHEIGHT);
	zoomSurfaceSize(12*PCHARWIDTH,12*PCHARHEIGHT, g_ui_resource.number_factor_x, g_ui_resource.number_factor_y,\
		&numner_sml_w, &numner_sml_h);
	g_ui_resource.number_small_w = numner_sml_w / 12.0;
	g_ui_resource.number_small_h = numner_sml_h / 12.0;
	printf("--number_small:(%f,%f)--\n", g_ui_resource.number_small_w, g_ui_resource.number_small_h);
	
	pTemp= IMG_Load(NUMBER_IMG);
	if(!pTemp)
	{
		/* image failed to load */
		printf("IMG_Load: ./28w.bmp %s\n", IMG_GetError());
		return 12;
	}
	tmp_sf_2 = zoomSurface(pTemp, g_ui_resource.number_factor_x, g_ui_resource.number_factor_y, SMOOTHING_ON);
	g_ui_resource.number_small_font = SDL_DisplayFormat(tmp_sf_2);
	SDL_FreeSurface(tmp_sf_2);
	g_ui_resource.number_font = SDL_DisplayFormat(pTemp);
	SDL_FreeSurface(pTemp);
	
	return 0;
}

void unload_door_resource()
{
	int i = 0;
	for(i = 0; i < DOOR_PIC_NUM; i++){
		if(g_ui_resource.surface[i]){
			SDL_FreeSurface(g_ui_resource.surface[i]);
		}
	}
	for(i = 0; i < 7; i++){
		if(g_ui_resource.opening[i]){
			SDL_FreeSurface(g_ui_resource.opening[i]);
		}
	}
	SDL_FreeSurface(g_ui_resource.number_font);
	SDL_FreeSurface(g_ui_resource.number_small_font);
	return;
}

//end

