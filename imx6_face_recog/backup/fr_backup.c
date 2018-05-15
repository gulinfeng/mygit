/**implement face data backup to service**/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <assert.h>
#include <dirent.h>  
#include <sys/stat.h>  
#include <unistd.h>  
#include <sys/types.h> 
#include "ftplib.h"
#include "fr_backup.h"

static int ftp_init_flg = 0;
static netbuf *s_conn = NULL;
static char s_host[32] = {0};
static char s_user[32] = {0};
static char s_passwd[32] = {0};
static char s_fr_path[128] = {0};

/**
* 递归删除目录(删除该目录以及该目录包含的文件和目录)
* @dir:要删除的目录绝对路径
*/
static int remove_dir(const char *dir)
{
    char cur_dir[] = ".";
    char up_dir[] = "..";
    char dir_name[128];
    DIR *dirp;
    struct dirent *dp;
    struct stat dir_stat;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);

    // 参数传递进来的目录不存在，直接返回
    if ( 0 != access(dir, F_OK) ) {
        return 0;
    }

    // 获取目录属性失败，返回错误
    if ( 0 > stat(dir, &dir_stat) ) {
        perror("get directory stat error");
        return -1;
    }

    if ( S_ISREG(dir_stat.st_mode) ) {  // 普通文件直接删除
        remove(dir);
    } else if ( S_ISDIR(dir_stat.st_mode) ) {   // 目录文件，递归删除目录中内容
        dirp = opendir(dir);
        while ( (dp=readdir(dirp)) != NULL ) {
            // 忽略 . 和 ..
            if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
                continue;
            }

            sprintf(dir_name, "%s/%s", dir, dp->d_name);
            remove_dir(dir_name);   // 递归调用
        }
        closedir(dirp);

        rmdir(dir);     // 删除空目录
    } else {
        perror("unknow file type!");    
    }
    return 0;
}


static void get_max_pids(char* piddir[], int num)
{
	int i;
	int max_pid[64] = {-1};
	int cur_pid = 0;
	int tmp_min,tmp_dex;
    DIR *dirp;
    struct dirent *dp;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	dirp = opendir(s_fr_path);
	while ( (dp=readdir(dirp)) != NULL ) {
            // 忽略 . 和 ..
            if ( (0 == strcmp(".", dp->d_name)) || (0 == strcmp("..", dp->d_name)) ) {
                continue;
            }
            //sprintf(dir_name, "%s/%s", dir, dp->d_name);
			if(dp->d_type & DT_DIR){
				cur_pid = atoi(&dp->d_name[3]);
				printf("DEBUG: cur pid :%d\n", cur_pid);
				tmp_min = max_pid[0];
				tmp_dex = 0;
				for(i = 0; i < num; i++){
					if(max_pid[i] < tmp_min){
						tmp_dex = i;
						tmp_min = max_pid[i];
					}
				}
				max_pid[tmp_dex] = max_pid[tmp_dex] > cur_pid ? max_pid[tmp_dex] : cur_pid;
			}
    }
    closedir(dirp);
	for(i = 0; i < num; i++){
		strcpy(piddir[i],"");
		if(max_pid[i] >= 0){
			sprintf(piddir[i], "pid%d", max_pid[i] );
			printf("DEBUG: get max pids :%s\n", piddir[i]);
		}
	}
}

static int readline(const char *mbuf, char *buf, int maxlen)
{
    int len=0;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
    while(len< maxlen-1 && *(mbuf+len)!='\n')
    {
        *(buf+len)=*(mbuf+len);
        len++;
    }
    *(buf+len)='\0';
	if(*(mbuf+len)=='\n'){
		len += 1;
	}
    return len;
}

static int is_valid_ip(const char *ip) 
{ 
    int section = 0;  //每一节的十进制值 
    int dot = 0;       //几个点分隔符 
    //int last = -1;     //每一节中上一个字符 
    printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
    while(*ip)
    { 
        if(*ip == '.')
        { 
            dot++; 
            if(dot > 3)
            { 
                return 0; 
            } 
            if(section >= 0 && section <=255)
            { 
                section = 0; 
            }else{ 
                return 0; 
            } 
        }else if(*ip >= '0' && *ip <= '9')
        { 
            section = section * 10 + *ip - '0'; 
            //if(last == '0')
            //{ 
            //   return 0; 
            //} 
        }else{ 
            return 0; 
        } 
        //last = *ip; 
        ip++;        
    }

    if(section >= 0 && section <=255)
    { 
        if(3 == dot)
        {
        section = 0; 
        printf ("IP address success!\n");       
        //printf ("%d\n",dot);
        return 1;
      }
    } 
    return 0; 
} 
#define DBUF_LEN 256*32
static int get_sameunit_doorips(char* unit, char* ips[], int max)
{
	int readed = 0;
	int l = 0;
	int num = 0;
	int buflen = 0;
	char line[64];
	char *dbuf;
    netbuf *nData;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	if (!FtpAccess(unit, FTPLIB_DIR, FTPLIB_ASCII, s_conn, &nData))
    {
    	printf("WARNNING: ftp access fialed %s\n",FtpLastResponse(s_conn));
    	return -1;
	}
	dbuf = (char*)malloc(DBUF_LEN);
	if(!dbuf){
		FtpClose(nData);
		return -2;
	}
	memset(dbuf, 0, DBUF_LEN);
	readed = 0;
	while ((l = FtpRead(&dbuf[readed], 8192, nData)) > 0)
	{
	    readed += l;
	}
	FtpClose(nData);
	buflen = strlen(dbuf);
	printf("DEBUG:read len=%d\n",buflen);
	readed = 0;
	do{
		l = readline(&dbuf[readed], line, 64);
		readed += l;
		printf("DEBUG: readline(%d):%s\n", l, line);
		if(is_valid_ip(line)){
			strcpy(ips[num], line);
			num++;
			if(max < num){
				break;
			}
		}
	}while((buflen > readed) && (l > 0));
	free(dbuf);
	return num;
}

void ftp_set_config(char* host, char* user, char* passwd, char* fr_path)
{
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	ftp_close();
	strcpy(s_host,host);
	strcpy(s_user,"anonymous");
	strcpy(s_passwd,"123456");
	if(user){
		strcpy(s_user,user);
		if(passwd){
			strcpy(s_passwd,passwd);
		}
	}
	strcpy(s_fr_path,"/home/wffrdb");
	if(fr_path){
		strcpy(s_fr_path,fr_path);
	}
	printf("DEBUG:FTP user:%s passwd:%s path:%s\n",s_user,s_passwd,s_fr_path);
}

int ftp_connect()
{
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	if(s_conn){
		return 0;
	}
	if(0 == ftp_init_flg){
		FtpInit();
		ftp_init_flg = 1;
	}
	if (!FtpConnect(s_host,&s_conn))
	{
    	printf("Unable to connect to node: %s\n",s_host);
		s_conn = NULL;
    	return -1;
	}
	if (!FtpLogin(s_user,s_passwd,s_conn))
	{
		ftp_close();
    	printf("Login failure: %s\n",FtpLastResponse(s_conn));
    	return -2;
	}
	return 0;
}


void ftp_close()
{
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	if(s_conn){
		FtpQuit(s_conn);
		s_conn = NULL;
		return;
	}
}

int ftp_report_deviceip(char* unit, char* dv_ip)
{
	//char remotepath[64];
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	int ret = ftp_connect();
	if(ret){
		return ret;
	}
	ret = FtpMkdir(unit, s_conn);
	if(1 != ret){
		printf("FTP MKDIR failure:\n%s",FtpLastResponse(s_conn));
	}
	ret = FtpChdir(unit, s_conn);
	if(1 != ret){
		printf("FTP CHADIR failure:\n%s",FtpLastResponse(s_conn));
		ftp_close();
		return -2;
	}
	ret = FtpMkdir(dv_ip, s_conn);
	if(1 != ret){
		printf("FTP MKDIR failure: \n%s",FtpLastResponse(s_conn));
	}
	ftp_close();
	return 0;
}

int ftp_load_data(char* unit)
{
	int empty = 0;
	char remotepath[64];
	char localpath[64];
	netbuf *nData;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	int ret = ftp_connect();
	if(ret){
		return ret;
	}
	
	printf(" ftp_load_data ..  \n");
	printf(" ftp_load_data ..  \n");

	
	//strcpy(unitpath, unit);
	sprintf(remotepath,"%s/db.dat", unit);
	sprintf(localpath,"%s/db.dat", s_fr_path);
	
	printf(" ftp_load_data   %s \n", remotepath);
	printf(" ftp_load_data   %s \n", localpath);
	
	if (!FtpAccess(remotepath, FTPLIB_FILE_READ, FTPLIB_IMAGE, s_conn, &nData))
    {
    	printf("WARNNING: %s is not exist\n", remotepath);
    	empty = 1;
	}
	if(1 == empty){
		//服务器没有数据库就上传db
		FtpMkdir(unit, s_conn);
		if((access(localpath,F_OK))==-1)   
		{   
			printf("WARNNING: %s is not exist\n", localpath);
		    ftp_close();
			return 0;  
		}
		ret = FtpPut(localpath, remotepath, FTPLIB_IMAGE, s_conn);
		if(1 != ret){
			printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
			printf("\t FTP PUT failure %s\n",FtpLastResponse(s_conn));
			ftp_close();
			return -1;
		}else{
			printf("DEBUG: put %s to %s\n", localpath, remotepath);
		}
		ftp_close();
		return 0;
	}
	FtpClose(nData);
	remove_dir(s_fr_path);
	mkdir(s_fr_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
	ret = FtpGet(localpath, remotepath, FTPLIB_IMAGE, s_conn);
	if(1 != ret){
		printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
		printf("\t FTP GET failure %s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -2;
	}else{
		printf("DEBUG: get %s from %s\n", localpath, remotepath);
	}
	ftp_close();
	return 0;
}

int ftp_update_local(char* unit)
{
	char remotepath[64];
	char localpath[64];
	netbuf *nData;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	int ret = ftp_connect();
	if(ret){
		return ret;
	}
	sprintf(remotepath,"%s/db.dat", unit);
	sprintf(localpath,"%s/db.dat", s_fr_path);
	
	if (!FtpAccess(remotepath, FTPLIB_FILE_READ, FTPLIB_IMAGE, s_conn, &nData))
    {
    	printf("WARNNING: %s is not exist\n", remotepath);
		ftp_close();
    	return -1;
	}
	FtpClose(nData);
	
	ret = FtpGet(localpath, remotepath, FTPLIB_IMAGE, s_conn);
	if(1 != ret){
		printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
		printf("\t FTP GET failure %s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -1;
	}else{
		printf("DEBUG: get %s from %s\n", localpath, remotepath);
	}
	ftp_close();
	return 0;
}

static void put_files(char* piddir)
{
	int ret;
	char remotepath[64];
	char localpath[64];
	char dir_name[64];
    DIR *dirp;
    struct dirent *dp;
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	sprintf(dir_name,"%s/%s",s_fr_path, piddir);
	dirp = opendir(dir_name);
	while ( (dp=readdir(dirp)) != NULL ) {
            // 忽略 . 和 ..
            if ( (0 == strcmp(".", dp->d_name)) || (0 == strcmp("..", dp->d_name)) ) {
                continue;
            }
			if(dp->d_type & DT_REG){
				sprintf(localpath,"%s/%s",dir_name, dp->d_name);
				sprintf(remotepath,"%s/%s",piddir, dp->d_name);
				ret = FtpPut(localpath, remotepath, FTPLIB_IMAGE, s_conn);
				if(1 != ret){
					printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
					printf("\t FTP GET failure %s\n",FtpLastResponse(s_conn));
				}else{
					printf("DEBUG: put %s to %s\n", localpath, remotepath);
				}
			}
    }
    closedir(dirp);
}

int ftp_update_remote(char* unit,  int face_num, char* doorip[], int maxlen, int backup_flg)
{
	int i;
	int cur_dex = 0;
	int ipnum = 0;
	char remotepath[64]={0};
	char localpath[64]={0};
	char dirbuff[64][32];
	char* piddir[64];
	memset(dirbuff, 0, sizeof(dirbuff));
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	int ret = ftp_connect();
	if(ret){
		return ret;
	}
	FtpMkdir(unit, s_conn);
	for(i = 0; i < 64; i ++){
		piddir[i] = dirbuff[i];
	}
	ret = FtpChdir(unit, s_conn);
	if(1 != ret){
		printf("WARNNING: FTP CHADIR failure:%s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -2;
	}
	//上传最新照片
	get_max_pids(piddir, face_num);
	while(cur_dex < face_num){
		if(strlen(piddir[cur_dex]) > 0){
			FtpMkdir(piddir[cur_dex], s_conn);
			put_files(piddir[cur_dex]);
		}
		cur_dex++;
	}
	//备份服务器上的db到本地
	if(0 != backup_flg){
		printf("DEBUG: backup remote db.dat file\n");
		strcpy(remotepath,"db.dat");
		sprintf(localpath,"%s/db.dat.bak", s_fr_path);
		ret = FtpGet(localpath, remotepath, FTPLIB_IMAGE, s_conn);
		if(1 != ret){
			printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
			printf("\t FTP GET failure %s\n",FtpLastResponse(s_conn));
		}
	}
	//上传DB文件
	strcpy(remotepath,"db.dat");
	sprintf(localpath,"%s/db.dat", s_fr_path);
	ret = FtpPut(localpath, remotepath, FTPLIB_IMAGE, s_conn);
	if(1 != ret){
		printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
		printf("\t FTP PUT failure %s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -1;
	}else{
		printf("DEBUG: put %s to %s\n", localpath, remotepath);
	}
	ret = FtpChdir("/", s_conn);
	if(1 != ret){
		printf("WARNNING: FTP CHADIR failure:%s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -3;
	}
	ipnum = get_sameunit_doorips(unit, doorip, maxlen);
	ftp_close();
	return ipnum;
}

int ftp_upload_data(char* unit, int force)
{
	netbuf* dir_hd;
	DIR *dirp;
    struct dirent *dp;
	char remotepath[64];
	char localpath[64];
	int ret = ftp_connect();
	if(ret){
		return ret;
	}
	printf("file=%s, func=%s, line=%d\n", __FILE__, __FUNCTION__, __LINE__);
	// 如果目录不存在就上传
	if(0 == force){
		ret = FtpAccess(unit, FTPLIB_DIR, FTPLIB_ASCII, s_conn, &dir_hd);
		if(1 == ret){
			FtpClose(dir_hd);
			ftp_close();
			printf("DEBUG: [%s] director is already exist.\n", unit);
			return 0;
		}
	}
	ret = FtpMkdir(unit, s_conn);
	if(1 != ret){
		printf("WARNNING: FTP MKDIR failure %s\n",FtpLastResponse(s_conn));
	}
	
	ret = FtpChdir(unit, s_conn);
	if(1 != ret){
		printf("WARNNING: FTP CHADIR failure:%s\n",FtpLastResponse(s_conn));
		ftp_close();
		return -2;
	}
	dirp = opendir(s_fr_path);
	while ( (dp=readdir(dirp)) != NULL ) {
            // 忽略 . 和 ..
            if ( (0 == strcmp(".", dp->d_name)) || (0 == strcmp("..", dp->d_name)) ) {
                continue;
            }
			if(dp->d_type & DT_REG){
				sprintf(remotepath,"%s",  dp->d_name);
				sprintf(localpath,"%s/%s", s_fr_path, dp->d_name);
				ret = FtpPut(localpath, remotepath, FTPLIB_IMAGE, s_conn);
				if(1 != ret){
					printf("WARNNING: local=%s, remote=%s\n", localpath, remotepath);
					printf("\t FTP GET failure %s\n",FtpLastResponse(s_conn));
				}else{
					printf("DEBUG: put %s to %s\n", localpath, remotepath);
				}
			}
			if(dp->d_type & DT_DIR){
				FtpMkdir(dp->d_name, s_conn);
				put_files(dp->d_name);
			}
    }
    closedir(dirp);
	ftp_close();
	return 0;
}

//end

