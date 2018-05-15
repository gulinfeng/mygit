/****implement card oporate *******/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>    // POSIX terminal control definitions 
#include <errno.h>      // Error number definitions 
#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include "door_card.h"
#include "tx522.h"
#include "door_media.h"
#include "comm_utile.h"
#include "door_info.h"
#include "platform_ioctl.h"
#include "door_log.h"
#include "door_elevator.h"
#include "taskpool.h"


static int nSerialfd = 0;
//����������
static int name_arr[] = {921600, 576000, 500000, 460800, 230400, 115200, 57600, 38400, 19200, 9600,4800, 2400, 1800, 1200, 300 };
//������ֵ
static int speed_arr[] = {B921600,B576000,B500000,B460800,B230400,B115200,B57600,B38400,B19200,B9600,B4800,B2400,B1800,B1200,B600,B300};

//���������õ��ź���
static int sem_mutex = 0;		

CircleBufferApi circleBufApi = {
    cb_init,
    cb_deinit,
    cb_info,
    cb_read,
    cb_write,
    cb_datalen,
    cb_full,
    cb_empty,
};


int user_sem_creat(key_t key)
{
    int semid;
    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0644);
    if(semid == -1){
        //ERR_EXIT("semget");
        printf("---sem_creat failed \n");
		return -1;
    }
    return semid;
}

int user_sem_open(key_t key)
{
    int semid;
    semid = semget(key, 0, 0);
    if(semid == -1){
        //ERR_EXIT("semget");
		printf("---sem_open failed \n");
		return -1;
    }
    return semid;
}

int user_sem_setval(int semid, int val)
{
    union semun su;
    su.val = val;
    int ret = semctl(semid, 0, SETVAL, su);
    if(ret == -1){
        ERR_EXIT("semctl");
    }
    return 0;
}

int user_sem_getval(int semid)
{
    int val;
    val = semctl(semid, 0, GETVAL, 0);
    
    return val;
}

int user_sem_del(int semid)
{
    int ret;
    ret = semctl(semid, 0, IPC_RMID, 0);
    if(ret == -1){
        ERR_EXIT("semctl");
    }

    return 0;
}

//PVԭ��
int user_sem_p(int semid)
{
    struct sembuf sb = {0, -1, 0};
    int ret = semop(semid, &sb, 1);
    if(ret == -1){
		printf("errno = %d \n", errno);
        ERR_EXIT("semop");
    }
    return ret;
}

int user_sem_v(int semid)
{
    struct sembuf sb = {0, +1, 0};
    int ret = semop(semid, &sb, 1);
    if(ret == -1){
        ERR_EXIT("semop");
    }
    return ret;
}

/*******************************************************************************
** ����: cb_init
** ����: ��ʼ��
** ����: avrbase_lei
*******/
void cb_init(CircleBufferMngr **ppmngr, cb_u32 buflen)
{    
    CB_ASSERT(NULL != ppmngr);
    
    if (NULL != *ppmngr)
        return;
    
    if (0 == buflen)
        return;

    *ppmngr = (CircleBufferMngr*)CB_MALLOC(sizeof(CircleBufferMngr));
    CB_ASSERT(NULL != *ppmngr);
    CB_MEMSET((void*)*ppmngr, 0, sizeof(CircleBufferMngr));
    
    (*ppmngr)->bufptr = (cb_u8*)CB_MALLOC(buflen);
    CB_ASSERT(NULL != (*ppmngr)->bufptr);
    (*ppmngr)->buflen = buflen;
    CB_MEMSET((void*)(*ppmngr)->bufptr, 0, buflen);

}

/*******************************************************************************
** ����: cb_deinit
** ����: ��Դ�ͷ�
** ����: avrbase_lei
*******/
void cb_deinit(CircleBufferMngr **ppmngr)
{
    CB_ASSERT(NULL != ppmngr);

    if(NULL == *ppmngr)
        return;
    
    if (NULL != (*ppmngr)->bufptr)
    {
        CB_MFREE((*ppmngr)->bufptr);
        (*ppmngr)->bufptr = NULL;
        (*ppmngr)->buflen = 0;
    }

    CB_MFREE(*ppmngr);
    *ppmngr = NULL;
}

/*******************************************************************************
** ����: cb_info
** ����: ��ӡ��������еĸ���ֵ������Ϊbuflen��outbuf��
** ����: avrbase_lei
*******/
void cb_info(
    CircleBufferMngr *pmngr,
    int (*user_printf)(const char *, ...))
{
    CB_ASSERT(NULL != pmngr);
    
    user_printf(
        " -cb_info: datalen=%d,readpos=%d,writepos=%d.-",
        pmngr->datalen,
        pmngr->readpos,
        pmngr->writepos);
}

/*******************************************************************************
** ����: cb_read
** ����: ��ȡ������buflen���ȵ����ݵ�outbuf��, outbuf�ĳ��Ȳ��õ���buflen
** ����: ����ʵ�ʶ�ȡ�����ݳ���, �ֽ�Ϊ��λ
** ˵��: ��������outbuf��ַΪNULL, ��ֱ��ɾ��buflen���ȵ�����
** ����: avrbase_lei
*******/
cb_u32 cb_read(
    CircleBufferMngr *pmngr,
    cb_u8 *outbuf,
    cb_u32 buflen)
{
    cb_u32 readlen = 0, tmplen = 0;
    
    CB_ASSERT(NULL != pmngr);
    CB_ASSERT(NULL != pmngr->bufptr);

    if(cb_empty(pmngr))
        return 0;
    
    CB_GLOBAL_LOCK;
    readlen = buflen > pmngr->datalen ? pmngr->datalen : buflen;
    tmplen = pmngr->buflen - pmngr->readpos;
    if(NULL != outbuf)
    {
        if(readlen <= tmplen)
        {
            CB_MEMCPY(
                (void*)outbuf,
                (void*)&pmngr->bufptr[pmngr->readpos],
                readlen);
        }
        else
        {
            CB_MEMCPY(
                (void*)outbuf,
                (void*)&pmngr->bufptr[pmngr->readpos],
                tmplen);
            
            CB_MEMCPY(
                (void*)&outbuf[tmplen],
                (void*)pmngr->bufptr,
                readlen - tmplen);
        }
    }
    
    pmngr->readpos = (pmngr->readpos + readlen) % pmngr->buflen;
    pmngr->datalen -= readlen;
    CB_GLOBAL_UNLOCK;
    
    return readlen;
}

/*******************************************************************************
** ����: cb_write
** ����: ��datptr��ָ��ĵ�ַ�е�datlen���ȵ�����д�뵽pmngr->bufptr��
** ����: ����ʵ��д������ݳ���, �ֽ�Ϊ��λ
** ����: avrbase_lei
*******/
cb_u32 cb_write(CircleBufferMngr *pmngr, cb_u8 *datptr, cb_u32 datlen)
{
    cb_u32 writelen = 0, tmplen = 0;
    
    CB_ASSERT(NULL != pmngr);
    CB_ASSERT(NULL != pmngr->bufptr);

    if(cb_full(pmngr))
        return 0;
    
    CB_GLOBAL_LOCK;
    tmplen = pmngr->buflen - pmngr->datalen;
    writelen = tmplen > datlen ? datlen : tmplen;

    if(pmngr->writepos < pmngr->readpos)
    {
        CB_MEMCPY(
            (void*)&pmngr->bufptr[pmngr->writepos],
            (void*)datptr,
            writelen);
    }
    else
    {
        tmplen = pmngr->buflen - pmngr->writepos;
        if(writelen <= tmplen)
        {
            CB_MEMCPY(
                (void*)&pmngr->bufptr[pmngr->writepos],
                (void*)datptr,
                writelen);
        }
        else
        {
            CB_MEMCPY(
                (void*)&pmngr->bufptr[pmngr->writepos],
                (void*)datptr,
                tmplen);
            CB_MEMCPY(
                (void*)pmngr->bufptr,
                (void*)&datptr[tmplen],
                writelen - tmplen);
        }
    }

    pmngr->writepos = (pmngr->writepos + writelen) % pmngr->buflen;
    pmngr->datalen += writelen;
    CB_GLOBAL_UNLOCK;
        
    return writelen;
}

/*******************************************************************************
** ����: cb_datalen
** ����: ��ѯpmngr->bufptr�е����ݳ���
** ����: ����pmngr->bufptr���ݳ���, �ֽ�Ϊ��λ
** ����: avrbase_lei
*******/
cb_u32 cb_datalen(CircleBufferMngr *pmngr)
{
    CB_ASSERT(NULL != pmngr);
    return pmngr->datalen;
}

/*******************************************************************************
** ����: cb_full
** ����: �жϻ������Ƿ�����
** ����: avrbase_lei
*******/
cb_bool cb_full(CircleBufferMngr *pmngr)
{
    CB_ASSERT(NULL != pmngr);
    return (cb_bool)(pmngr->buflen == pmngr->datalen);    
}

/*******************************************************************************
** ����: cb_empty
** ����: �жϻ������Ƿ�Ϊ��
** ����: avrbase_lei
*******/
cb_bool cb_empty(CircleBufferMngr *pmngr)
{
    CB_ASSERT(NULL != pmngr);
    return (cb_bool)(0 == pmngr->datalen);    
}




/*���ô��ڲ���*/
int set_serial4485(int fd, int databits, int parity, int stopbits, int flowcontrol,int vtime,int vmin)
{
	struct termios options;
	if (tcgetattr(fd, &options) != 0)//get property 
	{
		printf("SetupSerial 1\n");
		return -1;
	}

	/* set up the data size */
	options.c_cflag &= ~CSIZE;
	switch (databits)
	{
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr,"Unsupported data size\n");
		return -1;
	}

	/* set up parity */
	switch (parity)
	{
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;	/* Clear parity enable */
		options.c_iflag &= ~INPCK;	/* Enable parity checking */
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);	/* Enable odd parity*/ 
		options.c_iflag |= INPCK;		/* Disnable parity checking */
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;	/* Enable parity */
		options.c_cflag &= ~PARODD;	/* Turn odd off =>Even parity*/  
		options.c_iflag |= INPCK;	/* Disnable parity checking */
		break;
	case 'S':
	case 's':  // as no parity 
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;

	default:
		fprintf(stderr,"Unsupported parity\n");
		return (-1);
	}

	/* set up the stop bit(s) */   
	switch (stopbits)
	{
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr,"Unsupported stop bits\n");
		return (-1);
	}

	/* Set input parity option */
	if (parity != 'n')
	{
		options.c_iflag |= INPCK;
	}

	switch (flowcontrol)
	{
	case 'n':
	case 'N':
		options.c_cflag &= ~CRTSCTS;	// Disable hardware flow control
		break;
	case 'y':
	case 'Y':	
		options.c_cflag |= CRTSCTS;	// Enable hardware flow control
		break;
	default:
		fprintf(stderr, "Unsupported format to assign hardware flow control\n");
		return (-1);
	}


	options.c_cflag |= (CLOCAL | CREAD);		// Enable receiver and set local mode

	options.c_iflag &= ~(IXON | IXOFF | IXANY);	// Disable software flow control

	options.c_lflag = ~(ICANON | ECHO | ECHOE | ISIG);	// Choosing raw input

	options.c_oflag &= ~OPOST;		// Raw Output	add in 3.1	     

	options.c_cc[VTIME] = vtime; 	// VTIME����һ���������ȴ���ʱֵ,read������ȡʱ�䳬��0.1*vtime�򷵻�
	options.c_cc[VMIN] = vmin;        //VMIN��ʾΪ���������������Ҫ��ȡ�������ַ��������������ݴ���vmin��read��������

	tcflush(fd,TCIFLUSH);		
	/* Update the options and do it NOW */
	if (tcsetattr(fd, TCSANOW, &options) != 0)
	{
		printf("SetupSerial 3\n");
		return -1;
	}
	return 0;
}

/*���ò�����*/
void set_speed4485(int fd, int speed)
{
	int	i;
	int	status;
	struct termios Opt;
	tcgetattr(fd, &Opt);   //save current serial port setting 
	for (i = 0; i < sizeof(speed_arr) / sizeof(int);  i++)
	{
		if  (speed == name_arr[i])
		{
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]); //input bps
			cfsetospeed(&Opt, speed_arr[i]);// output bps 
			status = tcsetattr(fd, TCSANOW, &Opt);//set new property 
			if  (status != 0)
			{
				printf("tcsetattr fd\n");
				return;
			}

			tcflush(fd,TCIOFLUSH);
		}
	}
}

/*�򿪴���*/
int OpenSCom4485(char* szDevName, int nSpeed)
{
	int nSerialFd;
	nSerialFd = open(szDevName,O_RDWR|O_NONBLOCK );//�򿪴���  ttyS1
	if (-1 == nSerialFd)
	{
		perror("Can't Open Serial Port");
		return 0;	
	}
	set_speed4485(nSerialFd, nSpeed); 
	if (set_serial4485(nSerialFd,8,'e',1,'n',150,6) == -1)//15sec
	{
		printf("Databits, Stopbit(s), Parity Setting Error\n");
		return 0;
	}
	printf("485 Serial Port init OK!\n");
	return nSerialFd;
}

/*���մ�������*/
int RecvSComData4485(int nSComFd,char* szBuf,int nSize)
{
	int nRet,i;
		usleep(10000);
	nRet = read(nSComFd, szBuf, nSize);

	printf("485 receive:");
	for (i=0;i<nSize;i++)
	{
		printf("%.2x ",szBuf[i]);
	}
	printf("\n");
	return nRet;
}

/*���ʹ�������*/
int SendSComData4485(int nSComFd,char* szBuf,int nSize)
{
	int nRet;
	int i;
/*	printf("485_fd = %d command:", nSComFd);
	for (i=0;i<nSize;i++)
	{
		printf("%.2x ",szBuf[i]);
	}
	printf("\n");
*/
	EnableSend();
	nRet = write(nSComFd, szBuf, nSize);
	usleep(50000);
	EnableRecv();

	return nRet;
}

/*485׼��*/
void ReadyFor485() 
{
	int ret = 0;
	nSerialfd = OpenSCom4485("/dev/ttyS0", 9600);
	EnableRecv();//����
	printf("ReadyFor485. nSerialfd = %d \n", nSerialfd);

//PV��ʼ��
	ret = user_sem_open(6688);
	if(ret == -1){
		printf("---sem_open(6688) failed , prepare to create a new sem and set val 1\n");
		//��semʧ�ܣ����½�һ��
		sem_mutex = user_sem_creat(6688);
		user_sem_setval(sem_mutex, 1);//������ź�����ʼֵΪ 1 
	}
	else{
		printf("---sem_open(6688) successd , prepare to open sem and get val \n");
		//��sem �ɹ�
		sem_mutex = ret;
		if(user_sem_getval(sem_mutex) != 1){
			printf("---sem get val is not 1, now, set val 1\n");
			user_sem_setval(sem_mutex, 1);
		}
	}
	
}

int generate_rs485_params( struct rs485packet** p485Packet)
{
	char buffz[8] = {0xfa,0xff,0xf1,0x02,0x01,0x01,0x01,0xfe};
	struct rs485packet* pmy = (struct rs485packet*)malloc(sizeof(struct rs485packet));
	if(!pmy)
	{
		printf("ERROR: malloc rs485 packet fialed\n");
		//free(pParam);
		return -2;
	}
	memset(pmy, 0, sizeof(struct rs485packet));
	//�� 485 Ҫ���͵����ݿ����� p485Packet ��
	memcpy(pmy->send_buff, buffz, 8);
	
	(*p485Packet) = pmy;

	return 0;
}
int rs485_send_second_packet(int id, int interval)
{
	printf("rs485_send_second_packet enter\n");

	return 0;
}

static int rs485_sendcom(unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	int i = 0, roomlen = 0;
	char clevel[3] = {0};
	int ilevel = 0;
	struct rs485packet* p485Packet = (struct rs485packet*)Data1;
	printf("---rs485_sendcom enter. door_open_flag = %d \n", p485Packet->door_open_flag);

	//����PV�Ƿ�ֹ�սӵ�0x11�����˵�һ֡485���ݺ󣬵ȴ�һ�����;��ˢ�������յ�0x11�ˣ���ʱ����һ�£���ֹ�����ص�
	user_sem_p(sem_mutex);

	//�жϷ��ų��� ����3����4��ֱ���˳����� ����ʹ��485������
	roomlen = strlen(p485Packet->room);
	if((roomlen != 3) && (roomlen != 4)){
		printf("----call_elevator:rs485_sendcom unknow room num\n");
		return 1;
	}

	/*if(p485Packet->door_open_flag == OPEN_DOOR_BY_CARD){
		printf("----call_elevator:rs485_sendcom open door by card \n");
	}
	else if(p485Packet->door_open_flag == OPEN_DOOR_BY_NET){
		printf("----call_elevator:rs485_sendcom open door by net \n");
	}
	else{
		printf("---call_elevator:rs485_sendcom error 485 \n");
	}*/
	
	//�ж��ǵ��ϻ��ǵ���
	switch(p485Packet->door_floor){
		case 0x01://ƽ��
			p485Packet->send_buff[4] = 0x01;
			break;
		case 0x02://-1��
			p485Packet->send_buff[4] = 0xaa;
			break;
		case 0x03://-2��
			p485Packet->send_buff[4] = 0xbb;
			break;
		default:
			printf("----call_elevator:rs485_sendcom unknow floor\n");
			break;
	}

	//485����
	printf("----call_elevator:rs485_sendcom start send first packet :");
	for(i=0; i<8; i++){
		printf(" 0x%x ", p485Packet->send_buff[i]);
	}
	printf("\n");

	SendSComData4485(nSerialfd, p485Packet->send_buff, 8);

	sleep(1);//�ٹ�1���ӣ����͵ڶ�������   755door���������ģ���֪��Ϊɶ��Э�鲻���
	
	switch( roomlen ){
		case 3://3λ����
			p485Packet->send_buff[4] = p485Packet->room[0] - '0';
			break;
		case 4://4λ����
			strncpy(clevel, p485Packet->room, 2);
			ilevel = atoi(clevel);
			printf("----call_elevator:rs485_sendcom  ilevel = %d\n", ilevel);
			p485Packet->send_buff[4] = (char)ilevel;
			break;
		default:
			printf("----call_elevator:rs485_sendcom unknow room num\n");
			return 1;
			break;
	}
	
	p485Packet->send_buff[6] = 0x00;//���ݿ�����
	//485����
	printf("----call_elevator:rs485_sendcom start send second packet :");
	for(i=0; i<8; i++){
		printf(" 0x%x ", p485Packet->send_buff[i]);
	}
	printf("\n");
	SendSComData4485(nSerialfd, p485Packet->send_buff, 8);

	user_sem_v(sem_mutex);

	return 1;
}

void free_rs485_params(int process_ret, unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	struct rs485packet* p485Packet = (struct rs485packet*)Data1;
	if(p485Packet){
		printf("---pthread id = %u---   free rs485 pPacket\n", (unsigned int)pthread_self());
		free(p485Packet);
	}
	return;
}
void rs485_timeout_processing(int process_ret, unsigned int Data1, unsigned int Data2, unsigned int Data3)
{
	printf("rs485_timeout_processing\n");

}

int elevator_rs485_send_cmd(struct rs485packet *p485Packet)
{
	assert(p485Packet);
	pTASK_PARAM pTask = malloc_task_param();
	if(!pTask){
		free_rs485_params(0, (unsigned int)p485Packet, 0, 0);
		printf("ERROR: malloc_task_param fialed\n");
		return -1;
	}
	pTask->Data1 = (unsigned int)p485Packet;
	pTask->Data2 = 0;
	pTask->Data3 = 0;
	pTask->post_processing = (task_callback)free_rs485_params;
	pTask->processing = (task_processing)rs485_sendcom;
	//pTask->time_out = 1000;
	//pTask->try_times = 1;
	//pTask->timeout_processing = rs485_timeout_processing;//�ò���
	put_task_by_param(pTask);
	return 0;
}



/*
��ȡ�����
���� open_flag = OPEN_DOOR_BY_CARD ʱ��data_buf Ϊָ�� unsigned char DataBfr[16] = {0}; ��ָ��
	 open_flag = OPEN_DOOR_BY_NET  ʱ��data_buf Ϊָ�� ���յ���tcppacket ָ��
room: �����room�������ڸú����н����������ֵ��
����ִ�н��
*/
int elevator_get_room(unsigned int data_buf, char *room, ELEVATOR_CARD_OR_NET open_flag)
{
	int ret = -1;
	struct tcppacket* pPacket_data_buf = NULL;
	unsigned char *card_data_buf = NULL;
	char *proom = NULL;

	if(open_flag == OPEN_DOOR_BY_CARD){
		printf("---elevator_get_room:open door by card---\n");
		
		card_data_buf = (unsigned char *)data_buf;
		if(card_data_buf[0] != 0x68){//�����û����ͷ���
			printf("---elevator_get_room:this is not a user card \n");
			return -1;
		}
		ret = ( (card_data_buf[5]/16)*10 + card_data_buf[5]%16 )*100 + \
			    (card_data_buf[6]/16)*10 + (card_data_buf[6]%16);
		sprintf(room, "%d", ret);
		printf("---elevator_get_room:card receive roomnum = %s\n", room);
	}
	else if(open_flag == OPEN_DOOR_BY_NET){
		printf("---elevator_get_room:open door by net---\n");
		
		pPacket_data_buf = (struct tcppacket *)data_buf;

		if(pPacket_data_buf->ip == NULL){
			printf("---elevator_get_room:pPacket_data_buf->ip is NULL");
			return -4;
		}
		//�����յ�������֡�е�IP��ַ������ip.ini�ļ�����ȡ��room
		proom = di_get_room_by_ip(pPacket_data_buf->ip);
		if(proom == NULL){
			printf("---elevator_get_room:di_get_room_by_ip failed \n");
			return -3;
		}
		strcpy(room, proom);
		printf("---elevator_get_room: receive ip address = %s  room= %s\n", pPacket_data_buf->ip, room);
	}
	else{
		//���øú�����open_flag�����������Ĳ����ģ���˲����ܽ���
		printf("---elevator_get_room:call_elevator error net \n");
		return -2;
	}
	return 0;
}










//end

