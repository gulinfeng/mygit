/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_common.h
* 文件标识：  
* 摘 要 门口机通用头文件
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-4
*
*/
#ifndef DOOR_COMMON_H
#define DOOR_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum door_error
{
	memery_err,
	card_err,
	video_err,
	audio_err,
	net_err,
	timeout
}DOOR_ERROR_E;

/****** 门口机对讲状态, 内部使用*********/
typedef enum door_stata
{
	door_idle,
	door_calling,
	door_preview,
	door_message
}DR_STATA_E;



/******门口机被动回调******/

//错误
#define DR_SYS_ERROR         10

//被监控
#define DR_BE_PREVIEW        11

//报警
#define DR_BE_ALARM          12 

//被呼叫
#define DR_BE_RING           13


/**门锁状态**/
#define DR_LOCK_OPENED       14
#define DR_LOCK_CLOSED       15

/**刷卡操作状态**/
#define DR_CARDREAD_ERROR         16

/*配置文件加载完成*/
#define DR_FINISH_LOAD_CFG      17

/*被通知更新人脸数据*/
#define DR_NOTIFY_FR_UPDATE        18
/******门口机主动回调******/
#define DR_OPERATE_SUCCESS               20    //操作成功
#define DR_OPERATE_FAILD                 21    //操作失败
#define DR_STARTED                       22
#define DR_STOPED                        23

#define DR_AUDIO_PROGRESS                24        //收发音频数据进度  暂时不支持
#define DR_VIDEO_PROGRESS                25        //收发视频数据进度  暂时不支持

//被监控取消
#define DR_PREVIEW_CANCELED             DR_STOPED

/**呼叫状态**/
//#define DR_CALL_RING                   20
#define DR_CALL_DIAL                   26
#define DR_CALL_BUSY                   27
#define DR_CALL_ESTABLISHED            DR_STARTED
#define DR_CALL_CANCELED               DR_STOPED
#define DR_CALL_NO_ROOM                29


/**视频预览状态**/
//#define DR_VIDEO_STREAM_BEGIN       DR_BEGIN
//#define DR_VIDEO_STREAM_END         DR_END

/**留言状态**/
#define DR_MESSAGE_BEGIN            DR_STARTED
#define DR_MESSAGE_END              DR_STOPED



/*****内部使用******/

//无错误
#define DR_NO_ERROR              0

//网络断开
#define DR_NET_DISCONNECTED      -1      //网络断开

#define DR_RECV_START             1

#define DR_RECV_CONTINUE          2     //数据未接收完

#define DR_RECV_COMPLETED         3    //数据完整接受


/***** 内部使用*********/
/*******回调函数定义********/
/*所有回调函数中不能做长时间阻塞操作
*/
//用户回调
/**
* state        回调状态
* param        状态对应参数
* usr_data     用户自己数据
**/
typedef int (*DOOR_CALLBACK)(int state, unsigned int param, unsigned int usr_data);


/**网络数据包，内部使用**/
struct tcppacket{
    unsigned int   state;
    time_t         ltime;
    char  roomnum[7];
    char  password[5];
    char  title[20];
    char  ip[1024];
};

struct door_cmd_dt;

//网络响应回调，内部使用
typedef int (*NET_RESPONES_CK)(int rcv_state, struct tcppacket* packet, struct door_cmd_dt* cmddt);

/*用户请求参数数据，内部使用*/
typedef struct door_cmd_dt
{
	//unsigned int state;
	unsigned int port;
	unsigned int reserve1;
	unsigned int reserve2;
	char   room[8];
	char   address[32];
	DOOR_CALLBACK    usr_ck;
	NET_RESPONES_CK  net_respones_ck; 
}DOOR_CMD_DT_S;

/**梯控部分**/
//梯控通讯枚举
typedef enum{
	ELEVATOR_COMMUNICATION_RS485 = 0,//使用485方式
	ELEVATOR_COMMUNICATION_NET = 1   //使用网络方式
}ELEVATOR_COMMUNICATION_SWITCH;

//在向电梯模块发送数据的时候，需要区别是刷卡开门还是网络通讯开门的
typedef enum{
	OPEN_DOOR_BY_CARD = 0,   //使用卡来开门
	OPEN_DOOR_BY_NET  = 1    //使用网络来开门,即通过发送0x11来开门的
}ELEVATOR_CARD_OR_NET;

typedef struct rs485packet
{
	int door_open_flag;// 刷卡进入  还是  收到0x11进入
	int door_floor; //门口机所在楼层
	char room[8];
	char send_buff[8];//rs485发送的数据包
};


#ifdef __cplusplus
}
#endif

#endif //
