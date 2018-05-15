/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_elevator.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ��ݿ�ģ��ӿ�
*
* ��ǰ�汾��0.1
* �� �ߣ�glf
* ������ڣ�2018-03-29
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


//����, ��ֹ�жϺ��������
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
** ����: cb_init
** ����: ��ʼ��
** ����: avrbase_lei
*******/
extern void cb_init(CircleBufferMngr **ppmngr, cb_u32 buflen);

/*******************************************************************************
** ����: cb_deinit
** ����: ��Դ�ͷ�
** ����: avrbase_lei
*******/
extern void cb_deinit(CircleBufferMngr **ppmngr);

/*******************************************************************************
** ����: cb_info
** ����: ��ӡ��������еĸ���ֵ������Ϊbuflen��outbuf��
** ����: avrbase_lei
*******/
extern void cb_info(
    CircleBufferMngr *pmngr,
    int (*user_printf)(const char *, ...));

/*******************************************************************************
** ����: cb_read
** ����: ��ȡ������buflen���ȵ����ݵ�outbuf��, ����outbuf�ĳ��Ȳ��õ���buflen
** ����: ����ʵ�ʶ�ȡ�����ݳ���, �ֽ�Ϊ��λ
** ˵��: ��������outbuf��ַΪNULL, ��ֱ��ɾ��buflen���ȵ�����
** ����: avrbase_lei
*******/
extern cb_u32 cb_read(
    CircleBufferMngr *pmngr,
    cb_u8 *outbuf,
    cb_u32 buflen);

/*******************************************************************************
** ����: cb_write
** ����: ��datptr��ָ��ĵ�ַ�е�datlen���ȵ�����д�뵽pmngr->bufptr��
** ����: ����ʵ��д������ݳ���, �ֽ�Ϊ��λ
** ����: avrbase_lei
*******/
extern cb_u32 cb_write(
    CircleBufferMngr *pmngr,
    cb_u8 *datptr,
    cb_u32 datlen);

/*******************************************************************************
** ����: cb_datalen
** ����: ��ѯpmngr->bufptr�е����ݳ���
** ����: ����pmngr->bufptr���ݳ���, �ֽ�Ϊ��λ
** ����: avrbase_lei
*******/
extern cb_u32 cb_datalen(CircleBufferMngr *pmngr);

/*******************************************************************************
** ����: cb_full
** ����: �жϻ������Ƿ�����
** ����: avrbase_lei
*******/
extern cb_bool cb_full(CircleBufferMngr *pmngr);

/*******************************************************************************
** ����: cb_empty
** ����: �жϻ������Ƿ�Ϊ��
** ����: avrbase_lei
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

