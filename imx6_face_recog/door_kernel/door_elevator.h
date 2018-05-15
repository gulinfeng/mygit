/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_elevator.h
* 文件标识：  
* 摘 要 门口机梯控模块接口
*
* 当前版本：0.1
* 作 者：glf
* 完成日期：2018-03-29
*
*/
#ifndef DOOR_ELEVATOR_H
#define DOOR_ELEVATOR_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ERR_EXIT(m)\
    do{\
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)
		
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux-specific) */
};

typedef unsigned char      cb_u8;
typedef unsigned short int cb_u16;
typedef unsigned int       cb_u32;
typedef signed char        cb_s8;
typedef signed short int   cb_s16;
typedef signed int         cb_s32;
typedef char               cb_char;

typedef enum {cb_false, cb_true} cb_bool;

typedef struct {
    cb_u8    *bufptr;
    cb_u32    buflen;
    cb_u32    datalen;
    cb_u32    readpos;
    cb_u32    writepos;
}CircleBufferMngr;

typedef struct {
    void (*cb_init)(CircleBufferMngr **, cb_u32);
    void (*cb_deinit)(CircleBufferMngr **);
    void (*cb_info)(CircleBufferMngr *, cb_char *, cb_u32);
    cb_u32 (*cb_read)(CircleBufferMngr *, cb_u8 *, cb_u32);
    cb_u32 (*cb_write)(CircleBufferMngr *, cb_u8 *, cb_u32);
    cb_u32 (*cb_datalen)(CircleBufferMngr *);
    cb_bool (*cb_full)(CircleBufferMngr *);
    cb_bool (*cb_empty)(CircleBufferMngr *);
}CircleBufferApi;

extern CircleBufferApi circleBufApi;


#define CB_MEMCPY   memcpy
#define CB_MEMSET   memset
#define CB_SPRINT   sprintf
#define CB_MALLOC   malloc
#define CB_MFREE    free
#define CB_ASSERT   assert
#define CB_SNPRINT  snprintf
#define CB_TRACE    printf


//锁定, 禁止中断和任务调度
#define CB_GLOBAL_LOCK
#define CB_GLOBAL_UNLOCK

extern CircleBufferApi circleBufApi;

#define CBMA_INIT     circleBufApi.cb_init
#define CBMA_DEINIT   circleBufApi.cb_deinit
#define CBMA_INFO     circleBufApi.cb_info
#define CBMA_READ     circleBufApi.cb_read
#define CBMA_WRITE    circleBufApi.cb_write
#define CBMA_DATALEN  circleBufApi.cb_datalen
#define CBMA_FULL     circleBufApi.cb_full
#define CBMA_EMPTY    circleBufApi.cb_empty

/*******************************************************************************
** 函数: cb_init
** 功能: 初始化
** 作者: avrbase_lei
*******/
extern void cb_init(CircleBufferMngr **ppmngr, cb_u32 buflen);

/*******************************************************************************
** 函数: cb_deinit
** 功能: 资源释放
** 作者: avrbase_lei
*******/
extern void cb_deinit(CircleBufferMngr **ppmngr);

/*******************************************************************************
** 函数: cb_info
** 功能: 打印管理变量中的各种值到长度为buflen的outbuf中
** 作者: avrbase_lei
*******/
extern void cb_info(
    CircleBufferMngr *pmngr,
    int (*user_printf)(const char *, ...));

/*******************************************************************************
** 函数: cb_read
** 功能: 读取不超过buflen长度的数据到outbuf中, 其中outbuf的长度不得低于buflen
** 返回: 返回实际读取的数据长度, 字节为单位
** 说明: 如果传入的outbuf地址为NULL, 则直接删除buflen长度的数据
** 作者: avrbase_lei
*******/
extern cb_u32 cb_read(
    CircleBufferMngr *pmngr,
    cb_u8 *outbuf,
    cb_u32 buflen);

/*******************************************************************************
** 函数: cb_write
** 功能: 将datptr所指向的地址中的datlen长度的数据写入到pmngr->bufptr中
** 返回: 返回实际写入的数据长度, 字节为单位
** 作者: avrbase_lei
*******/
extern cb_u32 cb_write(
    CircleBufferMngr *pmngr,
    cb_u8 *datptr,
    cb_u32 datlen);

/*******************************************************************************
** 函数: cb_datalen
** 功能: 查询pmngr->bufptr中的数据长度
** 返回: 返回pmngr->bufptr数据长度, 字节为单位
** 作者: avrbase_lei
*******/
extern cb_u32 cb_datalen(CircleBufferMngr *pmngr);

/*******************************************************************************
** 函数: cb_full
** 功能: 判断缓冲区是否已满
** 作者: avrbase_lei
*******/
extern cb_bool cb_full(CircleBufferMngr *pmngr);

/*******************************************************************************
** 函数: cb_empty
** 功能: 判断缓冲区是否为空
** 作者: avrbase_lei
*******/
extern cb_bool cb_empty(CircleBufferMngr *pmngr);


extern int user_sem_creat(key_t key);
extern int user_sem_open(key_t key);
extern int user_sem_setval(int semid, int val);
extern int user_sem_getval(int semid);
extern int user_sem_del(int semid);
extern int user_sem_p(int semid);
extern int user_sem_v(int semid);

int elevator_get_room(unsigned int data_buf, char *room, ELEVATOR_CARD_OR_NET open_flag);
int generate_rs485_params( struct rs485packet** p485Packet);
int elevator_rs485_send_cmd(struct rs485packet *p485Packet);


void ReadyFor485();
int SendSComData4485(int nSComFd,char* szBuf,int nSize);
int RecvSComData4485(int nSComFd,char* szBuf,int nSize);
int OpenSCom4485(char* szDevName, int nSpeed);
void set_speed4485(int fd, int speed);
int set_serial4485(int fd, int databits, int parity, int stopbits, int flowcontrol,int vtime,int vmin);




#ifdef __cplusplus
}
#endif

#endif//

