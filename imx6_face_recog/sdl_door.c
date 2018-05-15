/****基于SDL的门口机界面*****/
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<assert.h>
#include <time.h>
#include "SDL_ttf.h"
#include "SDL_image.h"
#include "SDL_gfxPrimitives.h"
#include "gui_utile.h"
#include "sdl_door.h"
#include "door_kernel/door_signal.h"
#include "door_kernel/door_info.h"
#include "door_kernel/door_log.h"
#include "door_kernel/taskpool.h"
#include "recog/face_recog.h"
#ifdef FTP_BACKUP
#include "backup/fr_backup.h"
#endif        

#define BUILD_DT "180502"

#define  DF_PAGE_TIME_OUT        30 


static int g_linkstatus = 0;
static int g_isconnected = 0;
//static int g_creating = 0;

int create_makecall_page(char* room);

int create_set_ip_page(int type);

int create_opendoor_page(int result);

int create_tips_page(char* tips);

int create_info_page();
int create_update_page();

int create_mode_set_page();
int create_config_set_page();
int create_elevator_ctrl_cfg_page();
int create_cfg_page();
int create_card_sector_cfg_page(void);
int create_door_floor_cfg_page();
int create_unknow_function_page(void);


void set_door_net_state(int isconnected)
{
	g_isconnected = isconnected;
	
}
void set_door_link_state(int linkstate)
{
	g_linkstatus = linkstate;
}
#ifdef FTP_BACKUP
static int put_ftp_data(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret = 0;
	if(strlen(di_get_door_unit()) > 0){
		ret = ftp_upload_data(di_get_door_unit(), 1);
		if(ret != 0 ){
			create_tips_page("上传数据失败!");
			printf("WARNNING: ftp upload data failed\n");
			return 0;
		}else{
			set_face_data_updated(1);
		}
		create_tips_page("上传数据完成!");
		return 0;
	}
	create_tips_page("无法获取单元ID");
	return 0;
}

#endif
static int call_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}
static int call_on_keydown(SDLKey key_code, struct _tg_page* page)
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
			md_stop_play();
			cur_dex = page->cur_dex;
			if(cur_dex < MAX_INPUT_NUM - 5){
				page->key_char[cur_dex] = '1' + (key_code - SDLK_F1);
				page->cur_dex++;
				cur_dex = page->cur_dex;
				page->key_char[cur_dex] = '\0';
				//刷新页面
				refresh_page();
			}
			//printf("DEBUG: call_on_keydown-key_str=%s----\n",page->key_char);
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
			//printf("DEBUG: call_on_keydown-key_str=%s---\n",page->key_char);
			break;
		case SDLK_F10:   // *号键
			page->cur_dex--;
			cur_dex = page->cur_dex;
			if(cur_dex >= 0){
				page->key_char[cur_dex] = '\0';
				//刷新页面
				refresh_page();
			}else{
			    //退出页面
				end_page();
			}
			break;
		case SDLK_F12:   // #号键
			if(page->cur_dex > 0)
			{
#if ((defined IMX6_FR) || (defined S500_FR)) 
				if(0 == strcmp(page->key_char, "9991318")){
					//door_get_door_unit(NULL,0);
					// 推送本地数据到ftp服务器
					do_task(put_ftp_data, (unsigned int) page, 0);
					create_tips_page("正在上传数据***");
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
#endif
				if(0 == strcmp(page->key_char, "9991301")){
					create_door_floor_cfg_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991309")){
					create_config_set_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
#if ((defined IMX6_FR) || (defined S500_FR))  
				if(0 == strcmp(page->key_char, "9991319")){
					create_mode_set_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
#endif
				if(0 == strcmp(page->key_char, "9991329")){
					create_card_sector_cfg_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991339")){
					create_elevator_ctrl_cfg_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991349")){
					create_info_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991359")){
					create_update_page();
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991369")){
					create_set_ip_page(0);
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991379")){
					create_set_ip_page(1);
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991389")){
					create_set_ip_page(2);
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				if(0 == strcmp(page->key_char, "9991399")){
					create_set_ip_page(3);
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					break;
				}
				//呼叫
				if(page->cur_dex < 6){
					create_makecall_page(page->key_char);
					memset(page->key_char,0,MAX_INPUT_NUM);
					page->cur_dex = 0;
					page->page_abort = 1;
				}
			}else{
				end_page();
			}
			break;
		default:
			break;
	}
	return 0;
}
static int call_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	//int caret = 0;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	text_line_ex(100,33, 0,"请输入房号，按#号键结束",&ft_color, 0);
	text_line_ex(100,58, 0,"呼叫管理中心，00+#号键", &ft_color,0);
	text_line_ex(30,155, 0, "删除前一位请按*号键",&ft_color, 0);
	number_line(get_screen(), 35, 114, page->key_char);
	//printf("DEBUG: call_on_paint-key_str=%s-caret=%d---\n",page->key_char, caret);
	return 0;
}

int create_call_page(char input)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 1;
	if(('0' <= input) && ('9' >= input)){
		page->cur_dex = 1;
		page->key_char[0] = input;
	}
	page->pFont = get_base_font();
	page->background = get_image(0);
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_keydown = call_on_keydown;
	page->on_paint = call_on_paint;
	page->on_timer = call_on_timer;
	begin_page(page);
	return 0;
}

static int makecall_process(int state, unsigned int param, unsigned int usr_data)
{
	PAGA_S* page = (PAGA_S*)usr_data;
	if(DR_OPERATE_FAILD == state){
		send_page_event(MSG_NET_ERROR, (unsigned int)page);
		printf("MAKE_CALL: can not connect--(%d)\n", (int)param);
	}else if(DR_CALL_DIAL == state){
		send_page_event(MSG_CALL_DIALED, (unsigned int)page);
		printf("MAKE_CALL: wait for answer--\n");
	}else if(DR_CALL_BUSY == state){
		send_page_event(MSG_CALL_BUSY, (unsigned int)page);
		printf("MAKE_CALL: busy now\n");
	}else if(DR_CALL_ESTABLISHED == state){
		send_page_event(MSG_CALL_ENSTABLISHED, (unsigned int)page);
		char* ip = (char*)param;
		printf("MAKE_CALL: call establlished ip=%s\n",ip);
	}else if(DR_CALL_CANCELED == state){
		send_page_event(MSG_CALL_CANCEL, (unsigned int)page);
		printf("MAKE_CALL: cancel\n");
	}else if(DR_CALL_NO_ROOM == state){
		char* room =(char*)param;
		printf("MAKE_CALL: room:%s is not existed\n",room);
	}
	return 0;
}

static int makecall_on_init( struct _tg_page* page)
{
	int ret;
	int connect_to = 1;
	page->page_stata = 0;
	if(0 == strcmp("00", page->usr_str1)){
		page->page_stata = 1;
	}
	ret = door_make_call(page->usr_str1, makecall_process,(unsigned int)page);
	if(-2 == ret){
		// 没有房号
		snprintf(page->usr_str2, USR_MAX_STRING, "无%s房号,请重新输入.", page->usr_str1);
		send_page_event(MSG_NO_ROOM, (unsigned int)page);
		return 0;
	}
	//正在链接中
	page->usr_data[3] = connect_to;
	return 0;
}

static int makecall_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	int   run_tm;
	int page_event = page->usr_data[0];
	int connect_to = page->usr_data[3];
	if(1 == connect_to){
		refresh_page();
		return 0;
	}
	if(MSG_CALL_BUSY == page_event || MSG_NET_ERROR == page_event\
		|| MSG_NO_ROOM == page_event)
	{
		time(&cur_tm);
		run_tm = (unsigned int)cur_tm - page->usr_data[1];
		if(run_tm >= page->usr_data[2]){
			// 超时
			if(-1 == door_close_every()){
				end_page();
			}
		}
		return 0;
	}
	if(MSG_CALL_DIALED == page_event || MSG_CALL_ENSTABLISHED == page_event) {
		time(&cur_tm);
		run_tm = (unsigned int)cur_tm - page->usr_data[1];
		if(run_tm >= page->usr_data[2]){
			// 超时挂断
			if(-1 == door_close_every()){
				end_page();
			}
		}else {
			snprintf(page->key_char, MAX_INPUT_NUM, "%d", (page->usr_data[2] - run_tm));
			refresh_page();
		}
	}
	return 0;
}
static int makecall_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	if((page->timer_counter > 3) && (0 == page->usr_data[3])){
		// 挂断
		if(-1 == door_close_every()){
			end_page();
		}
	}
	return 0;
}
static char* s_strInfo[4] = {"[-]呼叫连接中**","[\\]呼叫连接中****","[|]呼叫连接中******","[/]呼叫连接中********"};


static void text_mutil_lines(int x, int y, char* text, SDL_Color *ft_color)
{
	int line, line_len,i;
	int couter;
	int chr_len;
	int chr_val;
	char temp[4][64];
	char* pStr = text;
	int len = strlen(text);
	memset(temp, 0 ,sizeof(temp));
	line = 0;
	line_len = 0;
	couter = 0;
	while(len > 0){
		chr_len = utf_8_getchar(pStr + line_len, len, &chr_val);
		if(chr_len < 1){
			break;
		}
		len -= chr_len;
		line_len += chr_len;
		couter++;
		if(couter >= 10){
			memcpy(temp[line], pStr, line_len);
			line++;
			pStr = pStr + line_len;
			line_len = 0;
			couter = 0;
			if(line > 3){
				break;
			}
		}
	}
	memcpy(temp[line], pStr, line_len);
	line++;
	for(i = 0; i < line; i++)
	{
		text_line_ex(x,(y + i*25),0,temp[i], ft_color, 0);
	}
	return;
}

static int makecall_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	SDL_Color ft_color;
	int page_event = page->usr_data[0];
	int connect_to = page->usr_data[3];
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == connect_to){
		text_line_ex(100, 40, 0, s_strInfo[page->timer_counter % 4], &ft_color, 0);
		number_line_center(get_screen(), 150, 120, 90, "30");
		return 0;
	}
	switch(page_event)
	{
		case MSG_NO_ROOM:
			text_mutil_lines(100, 35, page->usr_str2, &ft_color);
			break;
			
		case MSG_NET_ERROR:
			text_line_ex(100,45,0,"网络连接故障",&ft_color, 0);
			text_line_ex(120,165,0,"请稍后再试",&ft_color, 0);
			break;
			
		case MSG_CALL_BUSY:
			if(0 == page->page_stata){ 
				text_line_ex(100,45,0,"用户正在通话中",&ft_color, 0);
			}else{ //管理中心
				text_line_ex(100,45,0,"管理中心正在通话",&ft_color, 0);
			}
			text_line_ex(120,165,0,"请稍后重拨",&ft_color, 0);
			break;
		case MSG_CALL_DIALED:   //  
			if(0 == page->page_stata){ 
				text_line_ex(100,33,0,"正在呼叫中...",&ft_color, 0);
			}else{ //管理中心
			
			}	text_line_ex(100,33,0,"正在呼叫管理中心",&ft_color, 0);
			text_line_ex(100,58,0,"呼叫限时30秒",&ft_color, 0);
			number_line_center(get_screen(), 150, 120, 90, page->key_char);
			break;
		case MSG_CALL_ENSTABLISHED:
			if(0 == page->page_stata){ 
				text_line_ex(100,33,0,"正在通话中...",&ft_color, 0);
			}else{ //管理中心
				text_line_ex(100,33,0,"正在和管理中心通话",&ft_color, 0);
			}
			text_line_ex(120,58,0,"通话限时2分钟",&ft_color, 0);
			number_line_center(get_screen(), 150, 120, 90, page->key_char);
			break;
	}
	return 0;
}

static int makecall_page_event(int page_event, struct _tg_page* page)
{
	time_t cur_tm;
	page->usr_data[0] = page_event;
	time(&cur_tm);
	page->usr_data[1] = (unsigned int)cur_tm;
	printf("DEBUG: makecall_page_event %d ---\n",page_event);
	page->usr_data[3] = 0; 
	switch(page_event)
	{
		case MSG_NO_ROOM:
			page->background = get_image(2);
			page->usr_data[2] = 2; //超时时间
			refresh_page();
			break;
			
		case MSG_NET_ERROR:
			page->background = get_image(2);
			page->usr_data[2] = 2; //超时时间
			refresh_page();
			break;
		case MSG_CALL_BUSY:
			page->background = get_image(2);
			page->usr_data[2] = 3; //超时时间
			refresh_page();
			break;
		case MSG_CALL_CANCEL:
			if(page == current_page()){
				end_page();
			}else{
				page->page_abort = 1;
			}
			break;
		case MSG_CALL_DIALED:   //  
			page->background = get_image(1);
			page->usr_data[2] = 30; // 等待30秒超时
			refresh_page();
			break;
		case MSG_CALL_ENSTABLISHED:
			page->background = get_image(1);
			page->usr_data[2] = 120; // 通话120秒超时
			refresh_page();
			break;
	}
	return 0;
}

int create_makecall_page(char* room)  // 室内机或管理中心
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	strncpy(page->usr_str1, room, USR_MAX_STRING);
	page->page_id = 2;
	page->pFont = get_base_font();
	page->background = get_image(1);
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_init_page = makecall_on_init;
	page->on_keydown = makecall_on_keydown;
	page->on_paint = makecall_on_paint;
	page->on_timer = makecall_on_timer;
	page->on_page_event = makecall_page_event;
	begin_page(page);
	return 0;
}

static int split_ip_str(char dest[][16], char* ip)
{
	int i = 0;
	char split[32];
	strcpy(split, ip);
	char *s = strtok(split, ".");    
	while(s != NULL) 
	{
		strncpy(dest[i], s, 16);
		s = strtok(NULL, (const char*)".");
		i++;
	}
	return i;
}

static int get_caret_x(struct _tg_page* page)
{
	int start_x = 30;
	int ip_sec_w = 68;
	int ip_dex,sec_len;
	ip_dex = page->usr_data[0];
	sec_len = strlen(page->usr_tmp_str[ip_dex]);
	start_x = (start_x + ip_dex*ip_sec_w + 51 - 0.5*(51 - sec_len*get_small_number_w()) + 1);
	printf("caret ip_dex=%d, sec_len=%d, start_x=%d -----\n",ip_dex, sec_len, start_x);
	return start_x;
}

extern int GetNetlinkStatus();
extern void SetNetWork();

#ifdef FTP_BACKUP
static int get_door_unit_callback(int state, unsigned int param, unsigned int usr_data)
{
	int ret = 0;
	char* dr_unit = (char*)param;
	di_set_door_unit("");
	di_set_ip_reported(0);
	if(state == DR_OPERATE_SUCCESS){
		if(strlen(dr_unit) > 0) {
			di_set_door_unit(dr_unit);
			// report ip 
			ret = ftp_report_deviceip(dr_unit, di_get_door_ip());
			if(0 == ret){
				di_set_ip_reported(1);
			}
		}else{
			printf("ERROR: recieved door unit is NULL\n");
		}
	}
	return 0;
}
#endif
static int get_time_callback(int state, unsigned int param, unsigned int usr_data)
{
	//HWND hWnd = (HWND) usr_data;
	int ret;
	set_door_net_state(0);
	if(state == DR_OPERATE_SUCCESS){
		set_door_net_state(1);
		//send event to update clock time
		//send_clock_page_event(UE_USR_CLOCK_UPDATE);
#ifdef FTP_BACKUP
		if(strlen(di_get_door_unit()) <= 0 ){
			//获取单元ID
			door_get_door_unit(get_door_unit_callback, 0);
		}else{
			ret = ftp_report_deviceip(di_get_door_unit(), di_get_door_ip());
			if(0 == ret){
				di_set_ip_reported(1);
			}
		}
#endif
	}
	refresh_page();
	return 0;
}

static int setip_on_init(struct _tg_page* page)
{
	int type = page->page_stata;
	page->usr_data[0] = 0;
	memset(page->usr_tmp_str, 0 ,sizeof(page->usr_tmp_str));
	page->caret.y = 116;
	switch(type)
	{
		case 0: // IP地址
			page->usr_data[0] = split_ip_str(page->usr_tmp_str, di_get_door_ip());
			break;
		case 1: // 网关地址
			page->usr_data[0] = split_ip_str(page->usr_tmp_str, di_get_gateway());
			break;
		case 2: // 服务器地址
			page->usr_data[0] = split_ip_str(page->usr_tmp_str, di_get_server_ip());
			break;
		case 3: // elevator 地址
			page->usr_data[0] = split_ip_str(page->usr_tmp_str, di_get_elevator_ip());
			break;
	}
	page->usr_data[0] = page->usr_data[0] > 0 ? page->usr_data[0] - 1 : 0;
	page->caret.x = get_caret_x(page);
	show_page_caret(page, 1);
	set_door_net_state(0);
	g_linkstatus = GetNetlinkStatus();
	if(g_linkstatus){
		door_get_time(get_time_callback, (unsigned int)page);
	}
	return 0;
}

static int setip_on_end( struct _tg_page* page)
{
	int result = 0;
	int type = page->page_stata;
	char ip_str[32] = {0};
	char user[32] = {0};
	char passwd[32] = {0};
	memset(ip_str, 0, 32);
	if(1 == page->usr_data[7]){
		return 0;
	}
	sprintf(ip_str,"%s.%s.%s.%s", page->usr_tmp_str[0],page->usr_tmp_str[1],\
		page->usr_tmp_str[2],page->usr_tmp_str[3]);
	switch(type)
	{
		case 0: // IP地址
			if(0 != strcmp(ip_str, di_get_door_ip())){
				result = 1;
				di_set_door_ip(ip_str);
				SetNetWork();
			}
			break;
		case 1: // 网关地址
			if(0 != strcmp(ip_str, di_get_gateway())){
				result = 1;
				di_set_gateway(ip_str);
				SetNetWork();
			}
			break;
		case 2: // 服务器地址
			if(0 != strcmp(ip_str, di_get_server_ip())){
				result = 1;
				di_set_server_ip(ip_str);
#ifdef FTP_BACKUP
				di_get_ftp_config(user, passwd);
				ftp_set_config(di_get_ftp_host(), user, passwd, FTP_PATH);
#endif
			}
			door_get_port_info(NULL,0);
			break;
		case 3: // elevator 地址
			if(0 != strcmp(ip_str, di_get_elevator_ip())){
				result = 1;
				di_set_elevator_ip(ip_str);
			}
			break;
	}
	if(1 == result){
		printf("set ip = %s\n",ip_str);
		create_tips_page("网络地址设置成功");
		di_save_device_cfg(2);
#ifdef FTP_BACKUP
		set_face_data_updated(0);
		//获取单元ID
		door_get_door_unit(get_door_unit_callback, 0);
#endif
	}else{
		create_tips_page("网络地址未更改");
	}
	return 0;
}

static int setip_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		page->usr_data[7] = 1;
		end_page();
	}
	return 0;
}
static int setip_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int ip_dex = page->usr_data[0];
	int cur_sec_dex = strlen(page->usr_tmp_str[ip_dex]); 
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
			if(cur_sec_dex >= 3){
				return 0;
			}
			if(key_code == SDLK_F11){
				page->usr_tmp_str[ip_dex][cur_sec_dex] =  '0';
			}else {
				page->usr_tmp_str[ip_dex][cur_sec_dex] =  '1' + (key_code - SDLK_F1);
			}
			page->usr_tmp_str[ip_dex][cur_sec_dex + 1] =  '\0';
			if (cur_sec_dex + 1 >= 3){
				if(page->usr_data[0] >= 3){
					break;
				}
				page->usr_data[0]++;
			}
			break;
		case SDLK_F10:   // *号键
			if (cur_sec_dex - 1 < 0){
				if(0 == page->usr_data[0]){
					page->usr_data[7] = 1;
					end_page();
					return 0;
				}
				page->usr_data[0]--;
				ip_dex = page->usr_data[0];
				cur_sec_dex = strlen(page->usr_tmp_str[ip_dex]); 
			}
			page->usr_tmp_str[ip_dex][cur_sec_dex - 1] =  '\0';
			break;
		case SDLK_F12:   // #号键
			if(0 == cur_sec_dex){
				break;
			}
			if(ip_dex == 3){
				//输入完成
				end_page();
				return 0;
			}
			page->usr_data[0]++;
			break;
		default:
			break;
	}
	/*
	int i;
	for(i = 0; i < 4; i++)
	{
		printf("usr tmp str = %s---\n",page->usr_tmp_str[i]);
	}*/
	page->caret.x = get_caret_x(page);
	refresh_page();
	return 0;
}
static int setip_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i=0;
	int num_x=0;
	int num_y=0;
	int start_x = 30;
	int ip_sec_w = 68;
	char* pstr = NULL;
	SDL_Surface* pIcon = NULL;
	SDL_Color ft_color;
	SDL_Rect dest_rect;
	//int small_char_w = get_small_number_w();
	int type = page->page_stata;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	text_line_ex(100, 35, 0, "删除前一位请按*号键", &ft_color, 0);
	text_line_ex(100, 58, 0, "完成本网段请按#号键", &ft_color, 0);
	switch(type)
	{
		case 0: // IP地址
			pstr = "本机IP地址：";
			break;
		case 1: // 网关地址
			pstr = "网关地址：";
			break;
		case 2: // 服务器地址
			pstr = "服务器IP地址：";
			break;
		case 3: // elevator ip
			pstr = "梯控模块IP地址：";
			break;
	}
	text_line_ex(30, 86, 0, pstr, &ft_color, 0);
	num_y = 116;
	for(i=1;i<4;i++)
	{
		num_x = start_x + i*ip_sec_w - 17;
		sml_number_line(screen, num_x, num_y, ".");
	}
	for(i=0;i<4;i++)
	{
		num_x = start_x + i*ip_sec_w;
		sml_number_line_center(screen, num_x, num_y, 3*17, page->usr_tmp_str[i]);
	}
	text_line_ex(65,163,0,"本地连接",&ft_color, 0);
	text_line_ex(65,193,0,"服务器连接",&ft_color, 0);
	dest_rect.w = 25;
	dest_rect.h = 22;
	dest_rect.x = 35;
	dest_rect.y = 160;
	pIcon = get_image(7);
	if(g_linkstatus){
		pIcon = get_image(8);
	}
	SDL_BlitSurface(pIcon,NULL, screen, &dest_rect);

	dest_rect.y = 190;
	pIcon = get_image(9);
	if(g_isconnected){
		pIcon = get_image(10);
	}
	SDL_BlitSurface(pIcon,NULL, screen, &dest_rect);
	return 0;
}

int create_set_ip_page(int type)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 3;
	page->page_stata = type;
	page->pFont = get_base_font();
	page->background = get_image(5);
	page->timer_interval = 200;
	page->on_init_page = setip_on_init;
	page->on_end_page = setip_on_end;
	page->on_keydown = setip_on_keydown;
	page->on_paint = setip_on_paint;
	page->on_timer = setip_on_timer;
	begin_page(page);
	return 0;
}

static int card_sector_cfg_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if(1 == page->usr_data[2]){
		end_page();
		return 0;
	}
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}
static int card_sector_cfg_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	char sector_num[3] = {0,0,0};
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == page->usr_data[2]){
		//ft_color.r = 255;
		//ft_color.g = 0;
		//ft_color.b = 0;
		//text_line_ex_font(get_font(1),10,100, 0, "设置成功，请稍等****", &ft_color, 0);
		return 0;
	}
	text_line_ex(100, 35, 0, "删除前一位请按*号键", &ft_color, 0);
	text_line_ex(100, 58, 0, "完成修改请按#号键", &ft_color, 0);
	text_line_ex(30, 86, 0, "门口机刷卡扇区(0-15)：", &ft_color, 0);
	
	if(page->usr_data[0] < 10){
		sector_num[0] = page->usr_data[0] + '0';
	}
	else{
		sector_num[0] = page->usr_data[0]/10 + '0';
		sector_num[1] = page->usr_data[0]%10 + '0';
	}
	//display the sector num
	sml_number_line_center(screen, 30, 114, 3*17, sector_num);
	
	return 0;
}

static int card_sector_cfg_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	unsigned char val_tmp = 0;
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
			page->usr_data[1] ++;
			if(page->usr_data[1] > 2){
				page->usr_data[1] = 2;
			}

			if(key_code == SDLK_F11){//SDLK_F11=292
				val_tmp = 0;
			}
			else{
				val_tmp = (key_code - SDLK_F1) + 1;//SDLK_F1=282 SDLK_F2=283 ...
			}
			
			if(page->usr_data[1] < 2){
				page->usr_data[0] = val_tmp;
			}
			else{
				if(page->usr_data[0] >= 10){
					page->usr_data[0] = (page->usr_data[0]/10)*10 + val_tmp;
				}
				else{
					page->usr_data[0] = page->usr_data[0]*10 + val_tmp;
				}
			}
			//limit   range is 0-15
			if(page->usr_data[0] >= 15){
				page->usr_data[0] = 15;
			}
			//printf("  num    page->usr_data[0] = %d page->usr_data[1] = %d \n", page->usr_data[0], page->usr_data[1]);
			break;
		case SDLK_F10:   // *号键
			if(page->usr_data[1] > 0){
				page->usr_data[1] --;
				page->usr_data[0] = page->usr_data[1] * (page->usr_data[0]/10);
			}
			//printf("   *     page->usr_data[0] = %d page->usr_data[1] = %d \n", page->usr_data[0], page->usr_data[1]);
			break;
		case SDLK_F12:   // #号键
			if( page->usr_data[0] == di_get_card_sector() ){
				end_page();
				break;
			}
			di_set_card_sector(page->usr_data[0]);
			di_save_device_cfg(1);
			page->usr_data[2] = 1;
			refresh_page();
			//printf("   #     page->usr_data[0] = %d page->usr_data[1] = %d \n", page->usr_data[0], page->usr_data[1]);
			break;
		default:
			break;
	}

	//page->caret.x = get_caret_x(page);
	refresh_page();
	return 0;
}

int create_card_sector_cfg_page(void)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 12;
	page->pFont = get_base_font();
	page->background = get_image(5);
	page->timer_interval = 200;
	page->on_keydown = card_sector_cfg_on_keydown;
	page->on_paint = card_sector_cfg_on_paint;
	page->on_timer = card_sector_cfg_on_timer;
	page->usr_data[0] = di_get_card_sector();
	page->usr_data[1] = page->usr_data[0] > 9 ? 2 : 1;//len
	page->usr_data[2] = 0;
	//printf("   create     page->usr_data[0] = %d page->usr_data[1] = %d \n", page->usr_data[0], page->usr_data[1]);
	begin_page(page);
	return 0;
}

static int od_on_init( struct _tg_page* page)
{
	if(0 != page->page_stata){
		page->background = get_opening_image(0);
	}
	return 0;
}

static int od_on_leave_page(struct _tg_page* page)
{
	page->page_abort = 1;
	return 0;
}

static int od_on_timer(int id, struct _tg_page* page)
{
	if(1 == page->usr_data[1]){
		//重置页面
		page->usr_data[1] = 0;
		refresh_page();
		return 0;
	}
	if(page->timer_counter == 1){
		refresh_page();
		return 0;
	}
	if(page->timer_counter == page->usr_data[0] + 1){
		end_page();
		return 0;
	}
	
	return 0;
}

static int od_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i = 0;
	SDL_Surface* pbmp;
	if(1 == page->usr_data[1]){
		//重置页面
		page->usr_data[1] = 0;
		refresh_page();
		return 0;
	}
	if((0 != page->page_stata) && (page->timer_counter > 0)){
		for(i = 1; i < 7; i++){
			if(1 == page->usr_data[1]){
				//重置页面
				page->usr_data[1] = 0;
				refresh_page();
				return 0;
			}
			pbmp = get_opening_image(i);
			SDL_BlitSurface(pbmp,NULL, screen, NULL);
			SDL_Flip(screen);
			SDL_Delay(60);
			if(1 == page->usr_data[1]){
				//重置页面
				page->usr_data[1] = 0;
				refresh_page();
				return 0;
			}
		}
	}
	return 0;
}

int create_opendoor_page(int result)
{
	PAGA_S* page = current_page();
	if(page){
		if(4 == page->page_id){
			//重置页面
			page->page_stata = result;
			page->usr_data[1] = 1;
			page->background = get_image(4);
			page->timer_counter = 1;
			if(0 != page->page_stata){
				page->background = get_opening_image(0);
			}
			return 0;
		}
	}
	page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 4;
	page->pFont = get_base_font();
	page->background = get_image(4);
	page->timer_interval = 90;
	page->page_stata = result;
	page->usr_data[0] = 7;
	page->usr_data[1] = 0;
	if(0 == result){
		page->usr_data[0] = 5;
	}
	page->on_init_page = od_on_init;
	page->on_page_leave = od_on_leave_page;
	page->on_timer = od_on_timer;
	page->on_paint = od_on_paint;
	begin_page(page);
	return 0;
}

static int info_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT >> 1){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}
static int info_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	if(page->timer_counter > 1){
		end_page();
	}
	return 0;
}
static int info_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	char version[64]={0};
	char doorid[64]={0};
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	sprintf(version,"版本：1.2(alsa) Build%s", BUILD_DT);
#if ((defined IMX6_FR) || (defined S500_FR)) 
	text_line_ex(10, 10, 0, "人脸识别门口机", &ft_color, 0);
#elif ((defined IMX6_CLOCK) || (defined S500_CLOCK))  
	text_line_ex(10, 10, 0, "S500门口机", &ft_color, 0);
#endif
	text_line_ex(10, 40, 0, "浙江西谷数字股份有限公司", &ft_color, 0);
	text_line_ex(10, 60, 0, version, &ft_color, 0);
	text_line_ex(10, 80, 0, "门禁设备：新IC卡2.0", &ft_color, 0);
#if ((defined IMX6_FR) || (defined S500_FR)) 
	if(strlen(di_get_door_unit()) > 0){
		sprintf(doorid,"单元编号: %s", di_get_door_unit());
	}else{
		strcpy(doorid, "单元编号: 无法获取");
	}
	text_line_ex(10, 100, 0, doorid, &ft_color, 0);
	switch(is_face_data_updated()){
		case 0:
			text_line_ex(10, 120, 0, "人脸数据：未同步", &ft_color, 0);
			break;
		case 1:
			text_line_ex(10, 120, 0, "人脸数据：已同步", &ft_color, 0);
			break;
		case 2:
			text_line_ex(10, 120, 0, "人脸数据：无法同步", &ft_color, 0);
			break;
	}
#endif
	
	//text_line_ex(10, 120, 0, "SDL版本：1.2", &ft_color, 0);
	text_line_ex(10, 160, 0, "返回请按*号键", &ft_color, 0);
	return 0;

}

int create_info_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 5;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = info_on_timer;
	page->on_paint = info_on_paint;
	page->on_keydown = info_on_keydown;
	begin_page(page);
	return 0;
}


static int tips_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= 2){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}
static int tips_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	if(page->timer_counter > 1){
		end_page();
	}
	return 0;
}
static int tips_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	text_mutil_lines(100, 48, page->usr_title, &ft_color);
	return 0;
}

int create_tips_page(char* tips)
{
	PAGA_S* page = current_page();
	if(page && (7 == page->page_id)){
		strncpy(page->usr_title, tips, USR_MAX_STRING);
		page->last_time = time(NULL);
		refresh_page();
		return 0;
	}
	page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 7;
	strncpy(page->usr_title, tips, USR_MAX_STRING);
	page->pFont = get_base_font();
	page->background = get_image(2);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = tips_on_timer;
	page->on_paint = tips_on_paint;
	page->on_keydown = tips_on_keydown;
	begin_page(page);
	return 0;
}

static int update_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	if(1 == page->usr_data[1]){
		
		if(page->timer_counter - page->usr_data[3] > 2){
			page->usr_data[1] = 2;
			refresh_page();
		}
	}
	if(1 == page->usr_data[2]){
		
		if(page->timer_counter - page->usr_data[7] > 2){
			page->usr_data[2] = 2;
			refresh_page();
		}
	}
	if(0 == strcmp(page->usr_str1, "")){
		if(strlen(di_get_door_unit()) <= 0){
			strcpy(page->usr_str1, "无单元号");
			refresh_page();
		}
	}
	if(0 == strcmp(page->usr_str1, "无单元号")){
		if(strlen(di_get_door_unit()) > 0){
			strcpy(page->usr_str1, "");
			refresh_page();
		}
	}
	return 0;
}

static int fetch_result1(int state, unsigned int param, unsigned int usr_data)
{
	//int i, finish;
	PAGA_S* page = (PAGA_S*)usr_data;
	if(DR_NET_DISCONNECTED == state || DR_OPERATE_FAILD== state){
		page->usr_data[1] = 3;
	}
	if(DR_OPERATE_SUCCESS == state)
	{
		page->usr_data[1] = 2;
	}
	refresh_page();
	return 0;
}
static int fetch_result2(int state, unsigned int param, unsigned int usr_data)
{
	//int i, finish;
	PAGA_S* page = (PAGA_S*)usr_data;
	if(DR_NET_DISCONNECTED == state || DR_OPERATE_FAILD == state){
		page->usr_data[2] = 3;
	}
	if(DR_OPERATE_SUCCESS == state)
	{
		page->usr_data[2] = 2;
	}
	refresh_page();
	return 0;
}

#ifdef FTP_BACKUP
static int load_ftp_data(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret = 0;
	PAGA_S* page = (PAGA_S*)Data1;
	page->usr_data[0] = 3;
	if(strlen(di_get_door_unit()) > 0){
		ret = ftp_load_data(di_get_door_unit());
		if(ret == 0){
			set_face_data_updated(1);
			page->usr_data[0] = 2;
			send_page_event(UE_USR_RESET_LIB, (unsigned int)get_main_page());
		}else{
			set_face_data_updated(2);
		}
	}
	refresh_page();
	return 0;
}

#endif

static int update_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int i;
	int finish_flg;
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[4] = 0 == page->usr_data[4] ? 1 : 0;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[5] = 0 == page->usr_data[5] ? 1 : 0;
			refresh_page();
			break;
		case SDLK_F3:
			page->usr_data[6] = 0 == page->usr_data[6] ? 1 : 0;
			refresh_page();
			break;
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			if (page->usr_data[4]==0 && page->usr_data[5]==0 && page->usr_data[6]==0){
				finish_flg=2;
				break;
			}
			finish_flg = 0;
			for(i=0;i<3;i++)
			{
				if(page->usr_data[i] > 1)
					finish_flg++;
			}
			if(finish_flg >= 3){
				end_page();
				break;
			}
			if(page->usr_data[4]==1)
			{
#ifdef FTP_BACKUP
				//人脸数据获取
				page->usr_data[0]=1;
				do_task(load_ftp_data, (unsigned int) page, 0);
#else
				page->usr_data[0]=2;
#endif
			}
			
			if(page->usr_data[5]==1)
			{
				page->usr_data[1]=1;
				page->usr_data[3] = page->timer_counter;
				door_get_room_ip_list(fetch_result1,(unsigned int)page);
			}
			
			if(page->usr_data[6]==1)
			{
				page->usr_data[2]=1;
				page->usr_data[7] = page->timer_counter;
				door_get_ic_cord(fetch_result2,(unsigned int)page);
			}
			refresh_page();
			break;
		default:
			break;
	}
	return 0;
}
static int update_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i;
	int finish_flg;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	finish_flg = 0;
	for(i =0; i < 3; i++){
		if(1 == page->usr_data[i]){
			finish_flg = 1;
			break;
		}
	}
	if(1 == finish_flg)
	{
		text_line_ex(10, 60, 0, "更新中***,请等待", &ft_color, 0);
	}
	else
	{
		text_line_ex(10, 10, 0, "配置文件初始化设置", &ft_color, 0);
		text_line_ex(10, 40, 0, "1：申请人脸数据", &ft_color, 0);
		text_line_ex(10, 60, 0, "2：申请IP地址表", &ft_color, 0);
		text_line_ex(10, 80, 0, "3：初始化门禁编码", &ft_color, 0);
		if(strlen(page->usr_str1) > 0){
			ft_color.r = 255;
			ft_color.g = 0;
			ft_color.b = 0;
			text_line_ex(200, 40, 0, page->usr_str1, &ft_color, 0);
			ft_color.r = 255;
			ft_color.g = 255;
			ft_color.b = 255;
		}else {
			if(page->usr_data[4]==0) {
				text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
			}else if(1 == page->usr_data[4]) {
				text_line_ex(200, 40, 0, "[√]", &ft_color, 0);
	        } 
		}
		if(page->usr_data[5]==0)
			text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
        else
			text_line_ex(200, 60, 0, "[√]", &ft_color, 0);
		if(page->usr_data[6]==0)
			text_line_ex(200, 80, 0, "[  ]", &ft_color, 0);
        else
			text_line_ex(200, 80, 0, "[√]", &ft_color, 0);
		for(i=0;i<3;i++)
		{
			if(page->usr_data[i]==2){
				text_line_ex(250,40+20*i, 0, "更新完毕", &ft_color, 0);
			}else if(3 == page->usr_data[i]){
				text_line_ex(250,40+20*i, 0, "无法更新", &ft_color, 0);
			}
		}
	}
	text_line_ex(10,120, 0, "选择或取消请按相应键盘数字", &ft_color, 0);
	text_line_ex(10,160, 0, "更新请按#号键", &ft_color, 0);
	text_line_ex(10,180, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

static int update_on_init(struct _tg_page* page)
{
	strcpy(page->usr_str1, "");
	set_door_net_state(0);
	g_linkstatus = GetNetlinkStatus();
	if(g_linkstatus){
		door_get_time(get_time_callback, (unsigned int)page);
	}
}
int create_update_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 8;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_init_page = update_on_init;
	page->on_timer = update_on_timer;
	page->on_paint = update_on_paint;
	page->on_keydown = update_on_keydown;
	begin_page(page);
	return 0;
}

static int elevator_ctrl_cfg_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if(1 == page->usr_data[2]){


		end_page();
		return 0;
	}
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}

	return 0;
}
static int elevator_ctrl_cfg_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i;
	int finish_flg;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == page->usr_data[2]){
		ft_color.r = 255;
		ft_color.g = 0;
		ft_color.b = 0;
		text_line_ex_font(get_font(1),10,100, 0, "设置成功，请稍等****", &ft_color, 0);
		//page->usr_data[3] = 1;
		return 0;
	}
	text_line_ex(10, 20, 0, "本机是否使用梯控系统", &ft_color, 0);
	text_line_ex(10, 40, 0, "1：不使用", &ft_color, 0);
	text_line_ex(10, 60, 0, "2：使用", &ft_color, 0);
	if(page->usr_data[0] == 0){
		text_line_ex(200, 40, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
    }else{
    	text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[√]", &ft_color, 0);
    }
	text_line_ex(10, 100, 0, "梯控系统连接方式", &ft_color, 0);
	text_line_ex(10, 120, 0, "3：TCP/IP", &ft_color, 0);
	text_line_ex(10, 140, 0, "4：RS485", &ft_color, 0);
	if(page->usr_data[1] == ELEVATOR_COMMUNICATION_NET){
		text_line_ex(200, 120, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 140, 0, "[  ]", &ft_color, 0);
    }else{
    	text_line_ex(200, 120, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 140, 0, "[√]", &ft_color, 0);
    }
	
	text_line_ex(10,160, 0, "选择请按相应键盘数字。", &ft_color, 0);
	text_line_ex(10,180, 0, "确认请按#号键", &ft_color, 0);
	text_line_ex(10,200, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

static int elevator_ctrl_cfg_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int i;
	int finish_flg;
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[0] = 0;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[0] = 1;
			refresh_page();
			break;
		case SDLK_F3:
			page->usr_data[1] = ELEVATOR_COMMUNICATION_NET;
			refresh_page();
			break;
		case SDLK_F4:
			page->usr_data[1] = ELEVATOR_COMMUNICATION_RS485;
			refresh_page();
			break;
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			if( ( page->usr_data[0] == di_get_lift_ctl() ) && ( page->usr_data[1] == di_get_elevator_ctrl() ) ){
				end_page();
				break;
			}
			di_set_lift_ctl(page->usr_data[0]);
			di_set_elevator_ctrl(page->usr_data[1]);
			di_save_device_cfg(1);
			page->usr_data[2] = 1;
			refresh_page();
			break;
		default:
			break;
	}
	return 0;
}

int create_elevator_ctrl_cfg_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 10;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = elevator_ctrl_cfg_on_timer;
	page->on_paint = elevator_ctrl_cfg_on_paint;
	page->on_keydown = elevator_ctrl_cfg_on_keydown;
	page->usr_data[0] = di_get_lift_ctl();
	page->usr_data[1] = di_get_elevator_ctrl();
	page->usr_data[2] = 0;
	//page->usr_data[3] = 0;
	begin_page(page);
	return 0;
}
static int cfg_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if(1 == page->usr_data[2]){


		end_page();
		return 0;
	}
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}

	return 0;
}

static int cfg_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i;
	int finish_flg;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == page->usr_data[2]){
		ft_color.r = 255;
		ft_color.g = 0;
		ft_color.b = 0;
		text_line_ex_font(get_font(1),10,100, 0, "设置成功，请稍等****", &ft_color, 0);
		//page->usr_data[3] = 1;
		return 0;
	}
	text_line_ex(10, 20, 0, "门口机对讲参数设置", &ft_color, 0);
	text_line_ex(10, 40, 0, "1：模数混合系统", &ft_color, 0);
	text_line_ex(10, 60, 0, "2：存数字系统", &ft_color, 0);
	text_line_ex(10, 80, 0, "3：围墙机", &ft_color, 0);
	switch(page->usr_data[0])
	{
	case 1:
		text_line_ex(200, 40, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[  ]", &ft_color, 0);
		break;
	case 2:
		text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[  ]", &ft_color, 0);
		break;
	case 3:
		text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[√]", &ft_color, 0);
		break;
	default:
		printf("cfg_on_paint switch page->usr_data[0] err ! \n");
		return 0;
	}

	text_line_ex(10, 100, 0, "是否使用梯控系统(无效配置)", &ft_color, 0);
	text_line_ex(10, 120, 0, "4：使用", &ft_color, 0);
	text_line_ex(10, 140, 0, "5：不使用", &ft_color, 0);
	if(0){//if(page->usr_data[1] == 0){
		text_line_ex(200, 120, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 140, 0, "[  ]", &ft_color, 0);
    }else{
    	text_line_ex(200, 120, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 140, 0, "[√]", &ft_color, 0);
    }
	
	text_line_ex(10,160, 0, "选择请按相应键盘数字。", &ft_color, 0);
	text_line_ex(10,180, 0, "确认请按#号键", &ft_color, 0);
	text_line_ex(10,200, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

static int cfg_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	int i;
	int finish_flg;
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[0] = 1;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[0] = 2;
			refresh_page();
			break;
		case SDLK_F3:
			page->usr_data[0] = 3;
			refresh_page();
			break;
		case SDLK_F4:
			page->usr_data[1] = 0;
			refresh_page();
			break;
		case SDLK_F5:
			page->usr_data[1] = 1;
			refresh_page();
			break;
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			if( ( page->usr_data[0] == di_get_door_type() ) /*&& ( page->usr_data[1] == di_get_lift_ctl() ) */){
				end_page();
				break;
			}
			di_set_door_type(page->usr_data[0]);
			//di_set_lift_ctl(page->usr_data[1]);
			di_save_device_cfg(1);
			page->usr_data[2] = 1;
			refresh_page();
			break;
		default:
			break;
	}
	return 0;
}

int create_cfg_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 11;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = cfg_on_timer;
	page->on_paint = cfg_on_paint;
	page->on_keydown = cfg_on_keydown;
	page->usr_data[0] = di_get_door_type();
	page->usr_data[1] = di_get_elevator_ctrl();
	page->usr_data[2] = 0;
	//page->usr_data[3] = 0;
	begin_page(page);
	return 0;
}


static int mode_set_on_timer(int id, struct _tg_page* page)
{

	time_t cur_tm;
	time(&cur_tm);
	if(1 == page->usr_data[2]){
#ifdef USE_FR_LIB
		fr_set_recog_mode(page->usr_data[0]);
#else
		printf("mode_set_on_timer-> it is not USE_FR_LIB \n");
#endif

		end_page();
		return 0;
	}
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}

static int mode_set_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	//int i;
	//int finish_flg;
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[0] = 0;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[0] = 1;
			refresh_page();
			break;
		case SDLK_F3:
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			if(page->usr_data[0] == di_get_recogn_mode()){
				end_page();
				break;
			}
			di_set_recogn_mode(page->usr_data[0]);
			di_save_device_cfg(1);
			page->usr_data[1] = 1;
			refresh_page();
			break;
		default:
			break;
	}
	return 0;
}

static int mode_set_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	//int i;
	//int finish_flg;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == page->usr_data[1]){
		ft_color.r = 255;
		ft_color.g = 0;
		ft_color.b = 0;
		text_line_ex_font(get_font(1),10,100, 0, "设置成功，请稍等****", &ft_color, 0);
		page->usr_data[2] = 1;
		return 0;
	}
	text_line_ex(10, 10, 0, "人脸识别模式设置", &ft_color, 0);
	text_line_ex(10, 40, 0, "1：普通模式", &ft_color, 0);
	text_line_ex(10, 60, 0, "2：安全模式", &ft_color, 0);
	if(page->usr_data[0] == 0){
		text_line_ex(200, 40, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
    }else{
    	text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[√]", &ft_color, 0);
    }
	text_line_ex(10,100, 0,  "在安全模式下,识别时会让用户摇摇头", &ft_color, 0);
	text_line_ex(10,120, 0, "以确保为真实的人。", &ft_color, 0);
	
	text_line_ex(10,140, 0, "选择或取消请按相应键盘数字。", &ft_color, 0);
	text_line_ex(10,180, 0, "确认请按#号键", &ft_color, 0);
	text_line_ex(10,200, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

int create_mode_set_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 8;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = mode_set_on_timer;
	page->on_paint = mode_set_on_paint;
	page->on_keydown = mode_set_on_keydown;
	page->usr_data[0] = di_get_recogn_mode();
	page->usr_data[1] = 0;
	begin_page(page);
	return 0;
}
static int config_set_on_timer(int id, struct _tg_page* page)
{

	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}

static int config_set_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	//int i;
	//int finish_flg;
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[0] = 2;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[0] = 3;
			refresh_page();
			break;
		case SDLK_F3:
			break;
		case SDLK_F4:
			page->usr_data[1] += 1;
			if(page->usr_data[1] >= 15){
				page->usr_data[1] = 1;
			}
			refresh_page();
			break;
		case SDLK_F5:
			page->usr_data[1] -= 1;
			if(page->usr_data[1] <= 0){
				page->usr_data[1] = 14;
			}
			refresh_page();
			break;
		case SDLK_F6:
			break;
		case SDLK_F7:
			page->usr_data[2] += 1;
			if(page->usr_data[2] >= 100){
				page->usr_data[2] = 50;
			}
			refresh_page();
			break;
		case SDLK_F8:
			page->usr_data[2] -= 1;
			if(page->usr_data[2] <  50){
				page->usr_data[2] = 99;
			}
			refresh_page();
			break;
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			page->usr_data[7] = 0;
			end_page();
			break;
		case SDLK_F12:   // #号键
			if(di_get_door_type() != page->usr_data[0]){
				di_set_door_type(page->usr_data[0]);
				page->usr_data[7] = 1;
			}
			if(page->usr_data[1] != di_get_micval()){
				di_set_micval(page->usr_data[1]);
				page->usr_data[7] = 1;
			}
			if(page->usr_data[2] != di_get_talkval()){
				di_set_talkval(page->usr_data[2]);
				page->usr_data[7] = 1;
			}
			end_page();
			break;
		default:
			break;
	}
	return 0;
}

static int config_set_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	//int i;
	//int finish_flg;
	char value[8] = {0};
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	text_line_ex(10, 10, 0, "设备类型", &ft_color, 0);
	text_line_ex(10, 30, 0, "1：门口机", &ft_color, 0);
	text_line_ex(10, 50, 0, "2：围墙机", &ft_color, 0);
	if(page->usr_data[0] == 2){
		text_line_ex(200, 30, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 50, 0, "[  ]", &ft_color, 0);
    }else if(page->usr_data[0] == 3){
    	text_line_ex(200, 30, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 50, 0, "[√]", &ft_color, 0);
    }
	text_line_ex(10, 80, 0, "麦克风灵敏度(1-14):", &ft_color, 0);
	text_line_ex(10, 100, 0, "按4增加或5减少", &ft_color, 0);

	text_line_ex(10, 130, 0, "对讲音量(50-100):", &ft_color, 0);
	text_line_ex(10, 150, 0, "按7增加或8减少", &ft_color, 0);
	
	text_line_ex(10,180, 0, "确认请按#号键", &ft_color, 0);
	text_line_ex(10,200, 0, "返回请按*号键", &ft_color, 0);
	ft_color.g = 120;
	ft_color.b = 80;
	sprintf(value,"(%d)", page->usr_data[1]);
	text_line_ex_font(get_font(1),200,77, 0, value, &ft_color, 0);
	
	sprintf(value,"(%d)", page->usr_data[2]);
	text_line_ex_font(get_font(1),200,127, 0, value, &ft_color, 0);
	return 0;
}

static int config_on_end( struct _tg_page* page)
{
	if(1 == page->usr_data[7]){
		di_save_device_cfg(1);
	}
	return 0;
	
}
int create_config_set_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 9;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = config_set_on_timer;
	page->on_paint = config_set_on_paint;
	page->on_keydown = config_set_on_keydown;
	page->on_end_page = config_on_end;
	page->usr_data[0] = di_get_door_type();
	page->usr_data[1] = di_get_micval();
	page->usr_data[2] = di_get_talkval();
	begin_page(page);
	return 0;
}
static int door_floor_cfg_on_timer(int id, struct _tg_page* page)
{

	time_t cur_tm;
	time(&cur_tm);
	if(1 == page->usr_data[1]){
		end_page();
		return 0;
	}
	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}

static int door_floor_cfg_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	switch(key_code)
	{
		case SDLK_F1:
			page->usr_data[0] = 1;
			refresh_page();
			break;
		case SDLK_F2:
			page->usr_data[0] = 2;
			refresh_page();
			break;
		case SDLK_F3:
			page->usr_data[0] = 3;
			refresh_page();
			break;
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
			break;
		case SDLK_F11:
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			if(page->usr_data[0] == di_get_door_floor()){
				end_page();
				break;
			}
			di_set_door_floor(page->usr_data[0]);
			di_save_device_cfg(1);
			page->usr_data[1] = 1;
			refresh_page();
			break;
		default:
			break;
	}
	return 0;
}

static int door_floor_cfg_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	int i;
	int finish_flg;
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	if(1 == page->usr_data[1]){
		ft_color.r = 255;
		ft_color.g = 0;
		ft_color.b = 0;
		text_line_ex_font(get_font(1),10,100, 0, "设置成功，请稍等****", &ft_color, 0);
		page->usr_data[2] = 1;
		return 0;
	}
	text_line_ex(10, 20, 0, "门口机所在楼层设置", &ft_color, 0);
	text_line_ex(10, 40, 0, "1：平层", &ft_color, 0);
	text_line_ex(10, 60, 0, "2：负1层", &ft_color, 0);
	text_line_ex(10, 80, 0, "3：负2层", &ft_color, 0);
	switch( page->usr_data[0] )
	{
	case 1:
		text_line_ex(200, 40, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[  ]", &ft_color, 0);
		break;
	case 2:
		text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[√]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[  ]", &ft_color, 0);
		break;
	case 3:
		text_line_ex(200, 40, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 60, 0, "[  ]", &ft_color, 0);
		text_line_ex(200, 80, 0, "[√]", &ft_color, 0);
		break;
	}
	
	text_line_ex(10,160, 0, "选择请按相应键盘数字", &ft_color, 0);
	text_line_ex(10,180, 0, "确认请按#号键", &ft_color, 0);
	text_line_ex(10,200, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

int create_door_floor_cfg_page()
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 13;
	page->pFont = get_base_font();
	page->background = get_image(6);
	page->timer_interval = DF_TIME_INTERVAL;	
	page->on_timer = door_floor_cfg_on_timer;
	page->on_paint = door_floor_cfg_on_paint;
	page->on_keydown = door_floor_cfg_on_keydown;
	page->usr_data[0] = di_get_door_floor();
	page->usr_data[1] = 0;
	begin_page(page);
	return 0;
}
static int main_menu_on_timer(int id, struct _tg_page* page)
{

	time_t cur_tm;
	time(&cur_tm);

	if((unsigned int)cur_tm - page->last_time >= DF_PAGE_TIME_OUT){
		//大于30秒超时自动退出页面
		end_page();
	}
	return 0;
}

static int main_menu_on_keydown(SDLKey key_code, struct _tg_page* page)
{
	switch(key_code)
	{
		case SDLK_F1:
			create_call_page(NULL);
			break;
		case SDLK_F2:
		case SDLK_F3:
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
		case SDLK_F9:	
		case SDLK_F11:
			create_unknow_function_page();
			break;
		case SDLK_F10:   // *号键
			end_page();
			break;
		case SDLK_F12:   // #号键
			create_unknow_function_page();
			break;
		default:
			break;
	}
	return 0;
}

static int main_menu_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	SDL_Color ft_color;
	ft_color.r = 0;
	ft_color.g = 0;
	ft_color.b = 0;
	text_line_ex(200,221, 0, "返回请按*号键", &ft_color, 0);
	return 0;
}

int create_main_menu_page(void)
{
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 14;
	page->pFont = get_base_font();
	page->background = get_image(12);
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_timer = main_menu_on_timer;
	page->on_paint = main_menu_on_paint;
	page->on_keydown = main_menu_on_keydown;
	begin_page(page);
	return 0;
}

static int unknow_function_on_paint(SDL_Surface* screen, struct _tg_page* page)
{
	SDL_Color ft_color;
	ft_color.r = 255;
	ft_color.g = 255;
	ft_color.b = 255;
	text_line_ex(120,30, 0, "对不起，系统不支持此", &ft_color, 0);
	text_line_ex(120,50, 0, "功能", &ft_color, 0);
	return 0;
}

static int unknow_function_on_timer(int id, struct _tg_page* page)
{
	time_t cur_tm;
	time(&cur_tm);
	if((unsigned int)cur_tm - page->usr_data[0] >= 2){
		//大于2秒超时自动退出页面
		end_page();
	}
	return 0;
}

int create_unknow_function_page(void)
{
	time_t cur_tm;
	PAGA_S* page = malloc_page();
	if(!page){
		return -1;
	}
	page->page_id = 15;
	page->pFont = get_base_font();
	page->background = get_image(2);
	page->timer_interval = DF_TIME_INTERVAL;
	page->on_timer = unknow_function_on_timer;
	page->on_paint = unknow_function_on_paint;
	time(&cur_tm);
	page->usr_data[0] = (unsigned int)cur_tm;
	begin_page(page);
	return 0;
}


int sdl_door_init(SDL_Surface* screen)
{
	int ret = 0;
	ret = init_gui_utile();
	if(0 != ret){
		return ret;
	}
	ret = load_door_resource(screen);
	return ret;
}

void sdl_door_uninit()
{
	uninit_gui_utile();
	unload_door_resource();
}

int sdl_door_msg_process(SDL_Event* pEven)
{
	return msg_process(pEven);
}


//end


