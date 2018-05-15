/***implement door base info and serial info data***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "iniparser.h"
#include "door_info.h"
#include "door_common.h"


#define INI_IP_FILE           "./ip.ini"
#define INI_CARD_FILE         "./card.ini" 
#define INI_CONFIG_FILE       "./config.ini"
#define INI_NETWORKER_FILE    "./network.ini"
#define INI_BLACK_LIST        "./blacklist.ini"

typedef struct _tag_device_config
{
	char ip[32];
	char subnetmast[32];
	char getway[32];
	char server_ip[32];
	char lift_ctl_ip[32];
	unsigned short  log_port;
	unsigned short  server_port;
	unsigned short  center_port;
	unsigned short alarm_port;
	int carduse;
	int video_width;
	int video_height;
	int watchdog;
	int recogn_mode;
	
	int lift_ctrl;//elevator ctrl switch: 0:close 1:open
	int door_floor;//where the door mation is 
	int elevator_ctrl;//elevator communication switch 0:rs485,1:tcp/ip
}DEVICE_CFG_S;

static dictionary* g_room_map_ip = NULL;
static dictionary* g_config = NULL;
static dictionary* g_blacklist = NULL;
static dictionary* g_netcfg = NULL;
	
static DEVICE_CFG_S g_device_cfg = {0};

static char UserCard[9];
static char MngCard[6];
static char SysCard[6]={0x58,0x00,0x01,0x00,0x00,0x89};
static int card_len = 5;
static int card_sector = 15;
static int lock_authen_flg = 0;
static int face_enroll_flg = 0;
static int face_data_updated = 0;

void set_face_data_updated(int flg)
{
	face_data_updated = flg;
	printf("DEBUG: face data updated-------\n");
}

int is_face_data_updated()
{
	return face_data_updated;
}

void di_set_face_enroll(int enroll)
{
	face_enroll_flg = (0 == enroll) ? 0 : 1;
}
int di_get_face_enroll()
{
	return face_enroll_flg;
}
void di_set_lock_authentication(int authen)
{
	lock_authen_flg = (0 == authen) ? 0 : 1;
}
int di_get_lock_authentication()
{
	return lock_authen_flg;
}

static char StrToChar(char* pszHex)
{
	int i;
	char temp[2],ret;
	for(i=0;i<2;i++)
	{
		if((pszHex[i]>='0')&&(pszHex[i]<='9'))
			temp[i]=pszHex[i]-48;
		else if((pszHex[i]>='A')&&(pszHex[i]<='F'))
			temp[i]=pszHex[i]-55;
		else if((pszHex[i]>='a')&&(pszHex[i]<='f'))
			temp[i]=pszHex[i]-87;
		else temp[i]=0;
	}
	ret=temp[0]<<4;
	ret+=temp[1];
	return ret; 
}

static void ValToHex(char* cDes,char* cSrc)
{
	int i,nNum;
	char *p1,*p2;
    nNum=strlen(cSrc)/3;
	p1=cSrc+1;
	for(i=0;i<nNum-1;i++)
	{
		p2=strchr(p1,',');
		*p2=0;
		cDes[i]=StrToChar(p1);
		p1=p2+1;
	}
	p2=strchr(p1,'}');
	*p2=0;
	cDes[nNum-1]=StrToChar(p1);
}

int is_file_exist(char* file)
{
	int ret;
	if(NULL == file){
		return 0;
	}
	ret = access(file,F_OK);
	if(0 == ret){
		return 1;
	}
	return 0;
}

void di_load_card()
{
	char* str_tmp;
	dictionary* dict= iniparser_load(INI_CARD_FILE);
	if(dict){
		str_tmp = iniparser_getstring(dict,"CardNum:UserCard", "");
		if(strlen(str_tmp) == 28){
			ValToHex(UserCard,str_tmp);
		}
		str_tmp = iniparser_getstring(dict,"CardNum:MngCard", "");
		if(strlen(str_tmp) == 19){
			ValToHex(MngCard,str_tmp);
		}
		iniparser_freedict(dict);
	}
	di_reload_blacklist();
	return;
}
void di_update_card()
{
	char temp[64];
	FILE* fp = NULL;
	fp = fopen(INI_CARD_FILE, "w");
	if (fp == NULL)
	{
		printf("can't open %s\n", INI_CARD_FILE);
		return;
	}
	fprintf(fp, "[CardNum]\n");
	memset(temp,'\0',64);
	sprintf(temp,"{%.2x,%.2x,%.2x,%.2x,%.2x,%.2x,%.2x,%.2x,%.2x}",UserCard[0],UserCard[1],UserCard[2],UserCard[3],UserCard[4],UserCard[5],UserCard[6],UserCard[7],UserCard[8]);
	//SetValueToEtc(g_hData,"CardNum","UserCard",temp);
	fprintf(fp, "UserCard=%s\n", temp);
	memset(temp,'\0',64);
	sprintf(temp,"{%.2x,%.2x,%.2x,%.2x,%.2x,%.2x}",MngCard[0],MngCard[1],MngCard[2],MngCard[3],MngCard[4],MngCard[5]);
	fprintf(fp, "MngCard=%s\n", temp);
	fclose(fp);
	return;
}

char* di_get_user_card()
{
	return UserCard;
}
char* di_get_mng_card()
{
	return MngCard;
}

char* di_get_sys_card()
{
	return SysCard;
}


int di_set_card(char* user, char* mng)
{
	memcpy(UserCard,user,9);
	memcpy(MngCard,mng,6);
	return;
}

int di_is_black_user(char* cardid)
{	
	int value = -1;
	int ret = 0;
	char entry[64]={0};
	if(g_blacklist){
		sprintf(entry, "BLACKLIST:%s", cardid);
		value = iniparser_getint(g_blacklist, entry, -1);
		if(-1 == value){
			ret = 1;
		}
	}
	return ret;
}

void di_reload_blacklist()
{
	if(g_blacklist){
		iniparser_freedict(g_blacklist);
	}
	g_blacklist = iniparser_load(INI_BLACK_LIST);
	return;
}


char* di_get_ip_by_room(char* room)
{
	int i,j;
	int num_sec;
	char* secname;
	char* ip = NULL;
	char entry[64];
	if(NULL == g_room_map_ip){
		g_room_map_ip = iniparser_load(INI_IP_FILE);
	}
	assert(g_room_map_ip);
	num_sec  = iniparser_getnsec(g_room_map_ip);
	for(i = 0; i < num_sec; i++)
	{
		secname = iniparser_getsecname(g_room_map_ip, i);
		snprintf(entry, 64, "%s:%s", secname, room);
		ip = iniparser_getstring(g_room_map_ip, entry, NULL);
		if(ip){
			printf("IPLIST: %s ip=%s\n",entry, ip);
			break;
		}
	}
	return ip;
}

char* di_get_room_by_ip(char* ip_temp)
{
	int i=0,j=0,k=0;
	int num_sec = 0;
	char* secname = NULL;
	char* ip = NULL;
	char entry[64] = {0};
	char *keys[200];
	int nums = 0;
	char* proom = NULL;
	char room_tmp[9] = {0};//room数组7个，这里还有section与:的值 其实用malloc更好
	if(NULL == g_room_map_ip){
		g_room_map_ip = iniparser_load(INI_IP_FILE);
	}
	assert(g_room_map_ip);
	num_sec  = iniparser_getnsec(g_room_map_ip);
	//printf("---di_get_room_by_ip num_sec = %d\n", num_sec);
	for(i = 0; i < num_sec; i++)
	{
		//获取section的名字
		secname = iniparser_getsecname(g_room_map_ip, i);
		snprintf(entry, 64, "%s", secname);
		//printf("---di_get_room_by_ip entry = %s\n", entry);
		
		//根据section获取有多少key
		nums = iniparser_getsecnkeys(g_room_map_ip, entry);
		//printf("---di_get_room_by_ip nums = %d\n", nums);
		//没有malloc来，一个section应该不会超过200个用户
		if(nums > 200){
			printf("---di_get_room_by_ip error key nums is too large, max is 200 \n");
			return NULL;
		}
	
		//获取所有的key
		iniparser_getseckeys(g_room_map_ip, entry, keys);
		
		for(j = 0; j < nums; j++){
			//根据key获取相应的ip
			ip = iniparser_getstring(g_room_map_ip, keys[j], NULL);
			//printf("---%s--- %s\n", keys[j], ip);//1:105这样的形式
			//对比ip，相同则返回该ip对应的房号
			if(strcmp(ip, ip_temp) == 0){
				strcpy(room_tmp, keys[j]);
				proom = strtok(room_tmp, ":");
				if(proom == NULL){
					printf("---di_get_room_by_ip strtok(keys[j], :) failed \n");
					return NULL;
				}
				proom = strtok(NULL, ":");
				if(proom == NULL){
					printf("---di_get_room_by_ip strtok(NULL, :) failed \n");
					return NULL;
				}
				return proom;
			}
		}
		
	}

	return NULL;
}


int  di_reload_ip_ini()
{
	if(g_room_map_ip){
		iniparser_freedict(g_room_map_ip);
	}
	g_room_map_ip = iniparser_load(INI_IP_FILE);
	if(NULL == g_room_map_ip){
		g_room_map_ip = dictionary_new(0);
		iniparser_set(g_room_map_ip, "99", NULL);
	}
	return 0;
}

static void load_net_worker(char* file)
{
	if(NULL == g_netcfg){
		g_netcfg = iniparser_load(file);
		if(NULL == g_netcfg){
			g_netcfg = dictionary_new(0);
			iniparser_set(g_netcfg, "Network", NULL);
			
		}
	}
	strncpy(g_device_cfg.ip, iniparser_getstring(g_netcfg, "Network:IpAddress", "192.168.4.2"), 32);
	strncpy(g_device_cfg.subnetmast, iniparser_getstring(g_netcfg, "Network:SubnetMask", "255.255.255.0"), 32);
	strncpy(g_device_cfg.getway, iniparser_getstring(g_netcfg, "Network:Gateway", "192.168.4.254"), 32);
	strncpy(g_device_cfg.server_ip, iniparser_getstring(g_netcfg, "Network:ServerAddress", "192.168.4.77"), 32);
	strncpy(g_device_cfg.lift_ctl_ip, iniparser_getstring(g_netcfg, "Network:LiftCtlAddress", "192.168.4.111"), 32);
	
	g_device_cfg.server_port = iniparser_getint(g_netcfg, "Network:ServerPort", 10002);
	g_device_cfg.center_port = iniparser_getint(g_netcfg, "Network:CenterPort", 10000);
	g_device_cfg.log_port = iniparser_getint(g_netcfg, "Network:LogPort", 10003);
	g_device_cfg.alarm_port = iniparser_getint(g_netcfg, "Network:AlarmPort", 10004);
}

static void load_system_cfg(char* file)
{
	if(NULL == g_config){
		g_config = iniparser_load(file);
		if(NULL == g_config){
			g_config = dictionary_new(0);
			iniparser_set(g_config, "System", NULL);
		}
	}
	g_device_cfg.carduse = iniparser_getint(g_config, "System:CardUse", 1);
	
	g_device_cfg.video_width = iniparser_getint(g_config, "System:VIDEO_W", 640);
	g_device_cfg.video_height = iniparser_getint(g_config, "System:VIDEO_H", 480);

	g_device_cfg.watchdog = iniparser_getint(g_config, "System:WATCH_DOG", 1);
	
	g_device_cfg.recogn_mode = iniparser_getint(g_config, "System:RECOGN_MODE", 0);
	// elevator ctrl val
	g_device_cfg.lift_ctrl = iniparser_getint(g_config, "System:LiftCtrl", 0);
	//默认是网络方式通讯
	g_device_cfg.elevator_ctrl = iniparser_getint(g_config, "System:EleCtrlType", 1);
	//默认是平层
	g_device_cfg.door_floor = iniparser_getint(g_config, "System:DoorFloor", 1);
}

static int remove_section(dictionary* ini, char* sect)
{
	int i;
	char* keys[32];
	int nums = iniparser_getsecnkeys(ini, sect);
	iniparser_getseckeys(ini, sect, keys);
	for(i = 0; i < nums; i++)
	{
		//printf("delete: %s\n",keys[i]);
		iniparser_unset(ini, keys[i]);
	}
	return nums;
}
	
/**??èY?é????￡??é????ó?ò???config.ini???t￡????ú2e·?3éá???: config.inioínetwork.ini**/
static void load_device_cfg(int reload)
{
	char* file;
	load_net_worker(INI_NETWORKER_FILE);
	load_system_cfg(INI_CONFIG_FILE);
	int config_f = is_file_exist(INI_CONFIG_FILE);
	if(0 == config_f){
		di_save_device_cfg(1);
	}else{
		if(remove_section(g_config, "Network") > 0){
			di_save_device_cfg(1);
		}
	}
	int networker_f = is_file_exist(INI_NETWORKER_FILE);
	if(0 == networker_f){
		di_save_device_cfg(2);
	}
	return;
}

static void di_save_system_cfg()
{
	char vale[16];
	FILE* fp = NULL;
	
	sprintf(vale,"%d",(int)g_device_cfg.carduse);
	iniparser_set(g_config, "System:CardUse", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.video_width);
	iniparser_set(g_config, "System:VIDEO_W", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.video_height);
	iniparser_set(g_config, "System:VIDEO_H", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.watchdog);
	iniparser_set(g_config, "System:WATCH_DOG", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.recogn_mode);
	iniparser_set(g_config, "System:RECOGN_MODE", (const char*)vale);

	// elevator ctrl val
	sprintf(vale,"%d",(int)g_device_cfg.lift_ctrl);
	iniparser_set(g_config, "System:LiftCtrl", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.elevator_ctrl);
	iniparser_set(g_config, "System:EleCtrlType", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.door_floor);
	iniparser_set(g_config, "System:DoorFloor", (const char*)vale);

	fp = fopen(INI_CONFIG_FILE,"wt+");
	if(fp){
		iniparser_dump_ini(g_config, fp);
		fclose(fp);
	}
}

static void di_save_networker_cfg()
{
	char vale[16];
	FILE* fp = NULL;
	iniparser_set(g_netcfg, "Network:IpAddress", (const char*)g_device_cfg.ip);
	iniparser_set(g_netcfg, "Network:SubnetMask", (const char*)g_device_cfg.subnetmast);
	iniparser_set(g_netcfg, "Network:Gateway", (const char*)g_device_cfg.getway);
	iniparser_set(g_netcfg, "Network:ServerAddress", (const char*)g_device_cfg.server_ip);
	iniparser_set(g_netcfg, "Network:LiftCtlAddress", (const char*)g_device_cfg.lift_ctl_ip);

	sprintf(vale,"%d",(int)g_device_cfg.server_port);
	iniparser_set(g_netcfg, "Network:ServerPort", (const char*)vale);
	
	sprintf(vale,"%d",(int)g_device_cfg.center_port);
	iniparser_set(g_netcfg, "Network:CenterPort", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.log_port);
	iniparser_set(g_netcfg, "Network:LogPort", (const char*)vale);

	sprintf(vale,"%d",(int)g_device_cfg.alarm_port);
	iniparser_set(g_netcfg, "Network:AlarmPort", (const char*)vale);

	fp = fopen(INI_NETWORKER_FILE,"wt+");
	if(fp){
		iniparser_dump_ini(g_netcfg, fp);
		fclose(fp);
	}
	
}

void di_save_device_cfg(int type)
{
	if(0 == type){
		di_save_networker_cfg();
		di_save_system_cfg();	
	}else if (1 == type){
		di_save_system_cfg();
	}else if(2 == type){
		di_save_networker_cfg();
	}
	return;
}



int di_load_door_info()
{
	if(0 == is_file_exist("./wavs/main.wav")){
		system("mkdir wavs -p");
		system("cp main.wav welcome.wav err.wav ./wavs/");
	}
	memset(&g_device_cfg, 0, sizeof(DEVICE_CFG_S));
	
	if(NULL == g_room_map_ip){
		g_room_map_ip = iniparser_load(INI_IP_FILE);
		if(NULL == g_room_map_ip){
			g_room_map_ip = dictionary_new(0);
			iniparser_set(g_room_map_ip, "99", NULL);
		}
	}
	assert(g_room_map_ip);
	load_device_cfg(1);
	card_len = iniparser_getint(g_config, "System:CardLen", 5);
    card_sector = iniparser_getint(g_config, "System:CardSector", 15);
	di_load_card();

	return 0;
}

int di_get_card_sector()
{
	return card_sector;
}
void di_set_card_sector(int sector)
{
	char vale[16];
	if (sector > 15){
		sector = 15;
	}
	sprintf(vale,"%d",(int)sector);
	iniparser_set(g_config, "System:CardSector", vale);
	card_sector = sector;
	return;
}
int di_get_card_len()
{
	return card_len;
}
void di_set_card_len(int len)
{
	char vale[16];
	sprintf(vale,"%d",(int)len);
	iniparser_set(g_config, "System:CardLen", vale);
	card_len = len;
	return;
}
//设置门口机所在楼层
void di_set_door_floor(int floor_num)
{
	char vale[16] = {0};
	if( (floor_num < 1) || (floor_num > 3) ){
		printf("---di_set_door_floor failed, please set 1   2  or 3\n");
		return;
	}
	sprintf(vale, "%d", (int)floor_num);
	iniparser_set(g_config, "System:DoorFloor", vale);
	g_device_cfg.door_floor = floor_num;
	return;
}
void di_set_lift_ctl(int lift_ctrl)
{
	char vale[16] = {0};
	if( (lift_ctrl < 0) || (lift_ctrl > 1) ){
		printf("---di_set_lift_ctl failed, please set 0 or 1 \n");
		return;
	}
	sprintf(vale, "%d", (int)lift_ctrl);
	iniparser_set(g_config, "System:LiftCtrl", vale);
	g_device_cfg.lift_ctrl = lift_ctrl;
	return;
}
void di_set_elevator_ctrl(int elevator_ctrl)
{
	char vale[16] = {0};
	if( (elevator_ctrl < 0) || (elevator_ctrl > 1) ){
		printf("---di_set_elevator_ctrl failed, please set 0 or 1 \n");
		return;
	}
	sprintf(vale, "%d", (int)elevator_ctrl);
	iniparser_set(g_config, "System:EleCtrlType", vale);
	g_device_cfg.elevator_ctrl = elevator_ctrl;
	return;
}


int di_get_carused()
{
	return g_device_cfg.carduse;
}

int di_get_video_width()
{
	return g_device_cfg.video_width; //640;
}
int di_get_video_height()
{
	return g_device_cfg.video_height; //480;
}

int di_get_watch_dog()
{
	return g_device_cfg.watchdog; // iniparser_getint(g_config, "System:WATCH_DOG", 1);
}

int di_get_recogn_mode()
{
	return g_device_cfg.recogn_mode; //iniparser_getint(g_config, "System:RECOGN_MODE", 0);
}

int di_get_enroll_authentic()
{
	return iniparser_getint(g_config, "System:ENROLL_AUTHENTIC", 1);
}

static char g_door_unit[32] = {0};
static char g_ftp_host[32] = {0};
static int g_ip_reported_flg = 0;

int di_get_ftp_config(char* user, char* passwd)
{
	strcpy(user, iniparser_getstring(g_config, "System:FTP_USER", "anonymous"));
	strcpy(passwd, iniparser_getstring(g_config, "System:FTP_PASSWD", "123456"));
	return 0;
}

char* di_get_ftp_host()
{
	char ip[32] = {0};
	strcpy(ip, iniparser_getstring(g_config, "System:FTP_HOST", ""));
	if(strlen(ip) <= 0){
		strcpy(ip, di_get_server_ip());
	}
	sprintf(g_ftp_host,"%s:21", ip);
	//printf("debug-------------%s----\n", ip);
	return g_ftp_host;
}
void di_set_ip_reported(int flg)
{
	g_ip_reported_flg = flg;
}

int di_is_ip_reported()
{
	return g_ip_reported_flg;
}

char* di_get_door_unit()
{
	return g_door_unit;
}
int  di_set_door_unit(char* unit)
{
	strncpy(g_door_unit, unit, 32);
}
void di_set_recogn_mode(int mode)
{
	g_device_cfg.recogn_mode = (mode > 0) ? 1 : 0;
	return;
}

//获取电梯控制开关变量
int di_get_lift_ctl(void)
{
	return g_device_cfg.lift_ctrl; 
}
//获取当前门口机所在楼层
int di_get_door_floor(void)
{
	return g_device_cfg.door_floor; 
}
//获取梯控通讯方式: 1是网络方式   0是485方式
int di_get_elevator_ctrl(void)
{
	int ret = -1;
	if(g_device_cfg.elevator_ctrl == 0){
		ret = ELEVATOR_COMMUNICATION_RS485;
	}
	else if(g_device_cfg.elevator_ctrl == 1){
		ret = ELEVATOR_COMMUNICATION_NET;
	}
	else{
		printf("debug message: di_get_elevator_ctrl error! g_device_cfg.elevator_ctrl = %d \n", g_device_cfg.elevator_ctrl);
		ret = -1;
	}
	return ret;
}



int di_get_micval()
{
	return iniparser_getint(g_config, "System:MICVOL", 3);
}
int di_get_dialval()
{
	return iniparser_getint(g_config, "System:DIALVOL", 65);
}
int di_get_talkval()
{
	return iniparser_getint(g_config, "System:TALKVOL", 82);
}

int di_get_door_type()
{
	return iniparser_getint(g_config, "System:SysType", 2);
}
void di_set_micval(int val)
{
	char value[8] = {0};
	if(val >0 && val < 15){
		sprintf(value, "%d", val);
		iniparser_set(g_config, "System:MICVOL",value);
	}
	return;
}
void di_set_dialval(int val)
{
	char value[8] = {0};
	if(val >0 && val < 100){
		sprintf(value, "%d", val);
		iniparser_set(g_config, "System:DIALVOL",value);
	}
	return;
}
void di_set_talkval(int val)
{
	char value[8] = {0};
	if(val >0 && val < 100){
		sprintf(value, "%d", val);
		iniparser_set(g_config, "System:TALKVOL",value);
	}
	return;

}

void di_set_door_type(int type)
{	
	char value[8] = {0};
	sprintf(value, "%d", type);
	iniparser_set(g_config, "System:SysType",value);
	return;
}

char*  di_get_door_ip()
{
	return g_device_cfg.ip; //"192.168.20.117";
}

char* di_get_subnet_mast()
{
	return g_device_cfg.subnetmast;
}

char* di_get_gateway()
{
	return g_device_cfg.getway;
}

char*  di_get_server_ip()
{
	return g_device_cfg.server_ip;//"192.168.20.215";
}

char*  di_get_elevator_ip()
{
	return g_device_cfg.lift_ctl_ip;
}


void di_set_door_ip(char* ip)
{
	strcpy(g_device_cfg.ip, ip);
	return;
}
void di_set_subnet_mast(char* ip)
{
	strcpy(g_device_cfg.subnetmast, ip);
	return;
}
void di_set_gateway(char* ip)
{
	strcpy(g_device_cfg.getway, ip);
	return;
}
void di_set_server_ip(char* ip)
{
	strcpy(g_device_cfg.server_ip, ip);
	return;
}
void di_set_elevator_ip(char* ip)
{
	strcpy(g_device_cfg.lift_ctl_ip, ip);
	return;
}

void di_set_server_port(unsigned short port)
{
	g_device_cfg.server_port = port; //10002;
}

void di_set_center_port(unsigned short port)
{
	g_device_cfg.center_port = port;//10000;
}

void di_set_log_port(unsigned short port)
{
	g_device_cfg.log_port = port; //10003;
}

void di_set_alram_port(unsigned short port)
{
	g_device_cfg.alarm_port = port;
}

unsigned short di_get_server_port()
{
	return g_device_cfg.server_port; //10002;
}

unsigned short di_get_center_port()
{
	return g_device_cfg.center_port; //10000;
}

unsigned short di_get_log_port()
{
	return g_device_cfg.log_port; //10003;
}

unsigned short di_get_alram_port()
{
	return g_device_cfg.alarm_port; //10003;;
}


//end

