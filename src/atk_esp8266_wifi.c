/**********************************************************************
**  名称: ATK_ESP8266_WIFI.c文件
*   日期: 2019/5/20
*   作者: sf
*   版本: V0.01
*   描述: 主要涉及到实现的ESP8266WiFi模块的逻辑功能有：
*		1. ......
*   修改记录: 
*	   日期		修改人		版本 		修改内容
*	2019/5/20 	sf			V0.01       软件初始化版本
*
*********************************************************************/
#include "Atk_Esp8266_wifi.h"

/**************************************************************************
** 宏定义
**************************************************************************/
#define M_WIFI_SSID_MAX_LEN				(64)	/* wifi接入点名称最大长度 */
#define M_WIFI_PSWD_MAX_LEN				(32) 	/* wifi密码最大长度 */
#define M_WIFI_MAC_MAX_LEN				(20)	/* Wifi搜索的接入点的mac地址 */
#define M_WIFI_LIST_MAX_CNT				(20)	/* 最大支持20组可搜索的WIFI信息 */
#define M_WIFI_IP_MAX_LEN				(16)	/* 最长的IP信息长度 */
#define M_WIFI_MAX_IP_CNT				(5)		/* 最大支持5路IP */
#define M_WIFI_MAX_RECV_LEN				(1024)	/* 最大接收长度 */
#define M_WIFI_MAX_OVER_TIME			(500)	/* 最大超时时间(ms) */

#define M_WIFI_MAX_CMD_LEN				(64)	/* 发送指令最大长度 */
#define M_WIFI_MAX_QUEUE_CNT			(11)	/* 发送队列最大个数 */

#define M_WIFI_SET_BIT(n, x)			(n | ((1ul) << x))	/* x从0开始 */
#define M_WIFI_GET_BIT(n, x)			(n & ((1ul) << x))
#define M_WIFI_CLS_BIT(n, x)			(n & ~((1ul) << x))

/* 处理指令的回调函数 */
typedef int (*WifiDealCmdCb)(char *data, u16 len);

/* 组包消息的回调函数 */
typedef int (*WifiPackDataCb)(char *data, u8 cmdWay);

/**************************************************************************
** 结构体定义
**************************************************************************/
/* 对应wifi指令处理信息 */
typedef struct _WifiCmdInfoSt
{
	WifiCmdTypeE type;			/* wifi指令类型 */
	u8 maxCnt;					/* 请求指令最大次数 */
	u16 maxOvertime;			/* 每次请求最大超时时间,单位100ms */
	char *cmdStr;				/* 指令字符串 */
	WifiDealCmdCb cb;			/* wifi指令处理的回调函数 */
	WifiPackDataCb packCb;		/* wifi组包数据信息 */
} WifiCmdInfoSt;

/* WIFI应用模式 */
typedef enum _WifiAppModeE
{
	E_WIFI_MODE_STA = 1,		/* Station 模式，从设备模式 */
	E_WIFI_MODE_AP,				/* AP模式，热点模式 */
	E_WIFI_MODE_AP_STA,			/* AP+Station模式 */
} WifiAppModeE;

/* wifi模块用到的串口配置信息 */
typedef struct _WifiUartInfoSt
{
	int baud;					/* 波特率 */
	u8 dataBits:3;				/* 数据位,取值5,6,7,8 */
	u8 stopBits:2;				/* 停止位,取值1-1bit,2-1.5bits,3-2bits */
	u8 parityBits:2;			/* 校验位,取值0-None,1-Odd,2-Even */
	u8 flowCtrl:1;				/* 流控，0-不是能流控，1-同时使能RTS和CTS */
} WifiUartInfoSt;

/* wifi模块用到的数据信息，需要存储到flash中 */
typedef struct _WifiFlashDataSt
{
	/* 串口配置信息 */
	WifiUartInfoSt uart;		/* 串口配置信息 */

	/* 其他的参数 */
	WifiAppModeE mode;			/* wifi应用模式 */

	u8 apSw:1;					/* ap模式开关，0-关闭，1-开启 */
	u8 staSw:1;					/* sta模式开关，0-关闭，1-开启 */
	u8 apstaSw:1;				/* ap和sta模式开关，0-关闭，1-开启 */
	u8 autoConnFlag:1;			/* 开机使能STA模式自动连接，0-禁止，1-使能 */
	u8 restore:4;				/* 预留5bits */

	char apIp[M_WIFI_IP_MAX_LEN];	/* Ap模式下的IP地址 */ 
	char apMac[M_WIFI_MAC_MAX_LEN];	/* Ap模式下的mac地址 */ 

	char staIp[M_WIFI_IP_MAX_LEN];	/* Sta模式下的IP地址 */ 
	char staMac[M_WIFI_MAC_MAX_LEN];/* Sta模式下的mac地址 */ 

	char ssid[M_WIFI_SSID_MAX_LEN];	/* wifi接入点名称 */
	char pswd[M_WIFI_PSWD_MAX_LEN];	/* wifi密码 */
} WifiFlashDataSt;

/* wifi接入点的加密类型 */
typedef enum _WifiEncryptTypeE
{
	E_WIFI_ENPT_TYPE_OPEN = 0,		/* 不需要密码 */
	E_WIFI_ENPT_TYPE_WEP,			/* WEP加密方式 */
	E_WIFI_ENPT_TYPE_WPA_PSK,		/* WPA_PSK加密方式 */
	E_WIFI_ENPT_TYPE_WPA2_PSK,		/* WPA2_PSK加密方式 */
	E_WIFI_ENPT_TYPE_WPA_WPA2_PSK,	/* WPA_WPA2_PSK加密方式 */
} WifiEncryptTypeE;

/* 查询当前可用wifi信息，包括名称，信号强度，mac地址，通道号 */
typedef struct _WifiListInfoSt
{
	WifiEncryptTypeE eptyType;		/* 加密类型 */
	s8 rssi;						/* 信号强度 */
	u8 channel;						/* 通道 */
	char ssid[M_WIFI_SSID_MAX_LEN];	/* wifi接入点名称 */
	char mac[M_WIFI_MAC_MAX_LEN];	/* mac地址 */
} WifiListInfoSt;

/* Server模式下，管理多路IP的信息 */
typedef struct _WifiMuxIpInfoSt
{
	u8 sockId;						/* socket id，0-4，支持5路 */
	u8 sta;							/* 当前的socked id通道的状态，2-获取Ip,3-建立连接，4-失去连接 */
	u8 sockType;					/* socket type, 0 - tcp, 1- udp */
	char remoteIp[M_WIFI_IP_MAX_LEN];/* 连接模块的远程设备IP地址 */
	u16 remotePort;					/* 连接模块的远程设备的端口 */
} WifiMuxIpInfoSt;

/* 发送指令方式 */
typedef enum _WifiSendCmdWayE
{
	E_WIFI_CMD_TEST = 0,			/* 测试指令 */
	E_WIFI_CMD_QUERY,				/* 查询指令 */
	E_WIFI_CMD_SET,					/* 设置指令 */
	E_WIFI_CMD_EXE,					/* 执行指令 */
} WifiSendCmdWayE;

/* 每一项指令数据数据信息 */
typedef struct _WifiCmdItemDataSt
{
	u8 way;							/* 指令方式,0-测试指令，1-查询指令，2-设置指令，3-执行指令 */
	u8 type;						/* 命令指令类型 */
	u8 len;							/* 命令长度 */
	char data[M_WIFI_MAX_CMD_LEN];	/* 命令指令数据 */
} WifiCmdItemDataSt;

/* 发送数据队列结构体 */
typedef struct _WifiCmdQueueSt
{
	u8 index;						/* 入队列索引 */
	u8 outdex;						/* 出队列索引 */
	WifiCmdItemDataSt item[M_WIFI_MAX_QUEUE_CNT];	/* 管理数据队列 */
} WifiCmdQueueSt;

/* wifi模块用到的数据信息结构体 */
typedef struct _WifiDataInfoSt
{
	u8 ateSw:1;						/* 回显开关,0-关闭回显，1-开启回显 */
	u8 muxIpFlag:1;					/* 支持多路IP的标志，1 - 代表多路，0-单路，默认单路 */
	u8 serverModeFlag:1;			/* 服务器模式，0 - 关闭服务器模式，1-开启服务器模式，只有在多路IP的请情况下，才支持开始服务器模式 */
	u8 sendFlag:1;					/* 发送标志,0-未发送，1-已发送 */
	u8 workFlag:1;					/* wifi模块工作状态，0-未工作，1-工作 */

	u8 restore:3;
	u8 sendCmdWay;					/* 发送指令方式 */
	u8 sendCmdType;					/* 发送指令类型 */
	u8 sendCnt;						/* 发送次数 */
	u8 delayTime;					/* 延时时间 */

	WifiCmdQueueSt queue;			/* 队列数据 */
	char recvData[M_WIFI_MAX_RECV_LEN];			/* 接收WIFI模块的数据 */
	WifiMuxIpInfoSt muxIp[M_WIFI_MAX_IP_CNT];	/* 作为服务器模式下，管理多个client */
	WifiListInfoSt list[M_WIFI_LIST_MAX_CNT];	/* 可搜索到的WIFI信息 */
} WifiDataInfoSt;

/**************************************************************************
** 函数声明
**************************************************************************/
/* 处理对应指令的接收回调函数 */
static int WifiDealAt(char *data, u16 len);
static int WifiDealRst(char *data, u16 len);
static int WifiDealGmr(char *data, u16 len);
static int WifiDealAte(char *data, u16 len);
static int WifiDealRestore(char *data, u16 len);
static int WifiDealUart(char *data, u16 len);
static int WifiDealCwmode(char *data, u16 len);
static int WifiDealCwjap(char *data, u16 len);
static int WifiDealCwlap(char *data, u16 len);
static int WifiDealCwqap(char *data, u16 len);
static int WifiDealCwsap(char *data, u16 len);
static int WifiDealCwlif(char *data, u16 len);
static int WifiDealCwdhcp(char *data, u16 len);
static int WifiDealCwautoconn(char *data, u16 len);
static int WifiDealCipstamac(char *data, u16 len);
static int WifiDealCipapmac(char *data, u16 len);
static int WifiDealCipsta(char *data, u16 len);
static int WifiDealCipap(char *data, u16 len);
static int WifiDealSavetranslink(char *data, u16 len);
static int WifiDealCipstatus(char *data, u16 len);
static int WifiDealCipstart(char *data, u16 len);
static int WifiDealCipsend(char *data, u16 len);
static int WifiDealCipclose(char *data, u16 len);
static int WifiDealCifsr(char *data, u16 len);
static int WifiDealCipmux(char *data, u16 len);
static int WifiDealCpserver(char *data, u16 len);
static int WifiDealCipmode(char *data, u16 len);
static int WifiDealCipsto(char *data, u16 len);
static int WifiDealCiupdate(char *data, u16 len);
static int WifiDealPing(char *data, u16 len);

/* 组包发送命令函数 */
static int WifiPackCmdAt(char *data, u8 cmdWay);
static int WifiPackCmdRst(char *data, u8 cmdWay);
static int WifiPackCmdGmr(char *data, u8 cmdWay);
static int WifiPackCmdAte(char *data, u8 cmdWay);
static int WifiPackCmdRestore(char *data, u8 cmdWay);
static int WifiPackCmdUart(char *data, u8 cmdWay);
static int WifiPackCmdCwmode(char *data, u8 cmdWay);
static int WifiPackCmdCwjap(char *data, u8 cmdWay);
static int WifiPackCmdCwlap(char *data, u8 cmdWay);
static int WifiPackCmdCwqap(char *data, u8 cmdWay);
static int WifiPackCmdCwsap(char *data, u8 cmdWay);
static int WifiPackCmdCwlif(char *data, u8 cmdWay);
static int WifiPackCmdCwdhcp(char *data, u8 cmdWay);
static int WifiPackCmdCwautoconn(char *data, u8 cmdWay);
static int WifiPackCmdCipstamac(char *data, u8 cmdWay);
static int WifiPackCmdCipapmac(char *data, u8 cmdWay);
static int WifiPackCmdCipsta(char *data, u8 cmdWay);
static int WifiPackCmdCipap(char *data, u8 cmdWay);
static int WifiPackCmdSavetranslink(char *data, u8 cmdWay);
static int WifiPackCmdCipstatus(char *data, u8 cmdWay);
static int WifiPackCmdCipstart(char *data, u8 cmdWay);
static int WifiPackCmdCipsend(char *data, u8 cmdWay);
static int WifiPackCmdCipclose(char *data, u8 cmdWay);
static int WifiPackCmdCifsr(char *data, u8 cmdWay);
static int WifiPackCmdCipmux(void);
static int WifiPackCmdCpserver(char *data, u8 cmdWay);
static int WifiPackCmdCipmode(char *data, u8 cmdWay);
static int WifiPackCmdCipsto(char *data, u8 cmdWay);
static int WifiPackCmdCiupdate(char *data, u8 cmdWay);
static int WifiPackCmdPing(char *data, u8 cmdWay);

/**************************************************************************
** 全局变量定义
**************************************************************************/
/* 所有Wifi AT指令信息 */
static const WifiCmdInfoSt g_wifiCmdInfo[] =
{
	/* 基础AT指令 */
	{E_WIFI_CMD_AT, 			10,	5,	"AT",				WifiDealAt,				WifiPackCmdAt},			/* AT测试指令 */
	{E_WIFI_CMD_RST, 			3,	10,	"AT+RST",			WifiDealRst,			WifiPackCmdRst},		/* 重启指令 */
	{E_WIFI_CMD_GMR, 			3,	5,	"AT+GMR",			WifiDealGmr,			WifiPackCmdGmr},		/* 版本信息 */
	{E_WIFI_CMD_ATE,			3,	5,	"ATE",				WifiDealAte,			WifiPackCmdAte},		/* 开关回显功能 */
	{E_WIFI_CMD_RESTORE,		3,	10,	"AT+RESTORE",		WifiDealRestore,		WifiPackCmdRestore},	/* 恢复出厂设置,同时模块会重启 */
	{E_WIFI_CMD_UART,			3,	10,	"AT+UART",			WifiDealUart,			WifiPackCmdUart},		/* 设置串口配置 */

	/* Wifi功能AT指令 */
	{E_WIFI_CMD_CWMODE,			3,	10,	"AT+CWMODE",		WifiDealCwmode,			WifiPackCmdCwmode},			/* 选择 WIFI 应用模式 */
	{E_WIFI_CMD_CWJAP,			3,	10,	"AT+CWJAP",			WifiDealCwjap,			WifiPackCmdCwjap},			/* 加入 AP */
	{E_WIFI_CMD_CWLAP,			3,	10,	"AT+CWLAP",			WifiDealCwlap,			WifiPackCmdCwlap},			/* 列出当前可用 AP */
	{E_WIFI_CMD_CWQAP,			3,	10,	"AT+CWQAP",			WifiDealCwqap,			WifiPackCmdCwqap},			/* 退出与 AP 的连接 */
	{E_WIFI_CMD_CWSAP,			3,	10,	"AT+CWSAP",			WifiDealCwsap,			WifiPackCmdCwsap},			/* 设置 AP 模式下的参数 */
	{E_WIFI_CMD_CWLIF,			3,	10,	"AT+CWLIF",			WifiDealCwlif,			WifiPackCmdCwlif},			/* 查看已接入设备的 IP */
	{E_WIFI_CMD_CWDHCP,			3,	10,	"AT+CWDHCP",		WifiDealCwdhcp,			WifiPackCmdCwdhcp},			/* 设置 DHCP 开关 */
	{E_WIFI_CMD_CWAUTOCONN,		3,	10,	"AT+CWAUTOCONN",	WifiDealCwautoconn,		WifiPackCmdCwautoconn},		/* 设置 STA 开机自动连接到 wifi */
	{E_WIFI_CMD_CIPSTAMAC,		3,	10,	"AT+CIPSTAMAC",		WifiDealCipstamac,		WifiPackCmdCipstamac},		/* 设置 STA 的 MAC 地址 */
	{E_WIFI_CMD_CIPAPMAC,		3,	10,	"AT+CIPAPMAC",		WifiDealCipapmac,		WifiPackCmdCipapmac},		/* 设置 AP 的 MAC 地址 */
	{E_WIFI_CMD_CIPSTA,			3,	10,	"AT+CIPSTA",		WifiDealCipsta,			WifiPackCmdCipsta},			/* 设置 STA 的 IP 地址 */
	{E_WIFI_CMD_CIPAP,			3,	10,	"AT+CIPAP",			WifiDealCipap,			WifiPackCmdCipap},			/* 设置 AP 的 IP 地址 */
	{E_WIFI_CMD_SAVETRANSLINK,	3,	10,	"AT+SAVETRANSLINK",	WifiDealSavetranslink,	WifiPackCmdSavetranslink},	/* 保存透传连接到 Flash */

	/* TCP/IP工具箱AT指令 */
	{E_WIFI_CMD_CIPSTATUS,		3,	10,	"AT+CIPSTATUS",		WifiDealCipstatus,		WifiPackCmdCipstatus},	/* 获得连接状态 */
	{E_WIFI_CMD_CIPSTART,		3,	10,	"AT+CIPSTART",		WifiDealCipstart,		WifiPackCmdCipstart},	/* 建立 TCP 连接或注册 UDP 端口号 */
	{E_WIFI_CMD_CIPSEND,		3,	10,	"AT+CIPSEND",		WifiDealCipsend,		WifiPackCmdCipsend},	/* 发送数据 */
	{E_WIFI_CMD_CIPCLOSE,		3,	10,	"AT+CIPCLOSE",		WifiDealCipclose,		WifiPackCmdCipclose},	/* 关闭 TCP 或 UDP */
	{E_WIFI_CMD_CIFSR,			3,	10,	"AT+CIFSR",			WifiDealCifsr,			WifiPackCmdCifsr},		/* 获取本地 IP 地址 */
	{E_WIFI_CMD_CIPMUX,			3,	10,	"AT+CIPMUX",		WifiDealCipmux,			WifiPackCmdCipmux},		/* 启动多连接 */
	{E_WIFI_CMD_CIPSERVER,		3,	10,	"AT+CIPSERVER",		WifiDealCpserver,		WifiPackCmdCpserver},	/* 配置为服务器 */
	{E_WIFI_CMD_CIPMODE,		3,	10,	"AT+CIPMODE",		WifiDealCipmode,		WifiPackCmdCipmode},	/* 设置模块传输模式 */
	{E_WIFI_CMD_CIPSTO,			3,	10,	"AT+CIPSTO",		WifiDealCipsto,			WifiPackCmdCipsto},		/* 设置服务器超时时间 */
	{E_WIFI_CMD_CIUPDATE,		3,	10,	"AT+CIUPDATE",		WifiDealCiupdate,		WifiPackCmdCiupdate},	/* 网络升级固件 */
	{E_WIFI_CMD_PING,			3,	10,	"AT+PING",			WifiDealPing,			WifiPackCmdPing},		/* PING 命令 */

};

/* wifi数据信息 */
static WifiDataInfoSt g_wifiDataInfo;

/**************************************************************************
** 函数定义
**************************************************************************/
/**************************************************************************
* 函  数: u8 WifiGetQueueCount(void)
* 描  述: 获取队列个数据
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: u8：队列中的个数
**************************************************************************/
static u8 WifiGetQueueCount(void)
{
	if (g_wifiDataInfo.queue.index >= g_wifiDataInfo.queue.outdex)
	{
		return (g_wifiDataInfo.queue.index - g_wifiDataInfo.queue.outdex);
	}

	return (g_wifiDataInfo.queue.index + M_WIFI_MAX_QUEUE_CNT - g_wifiDataInfo.queue.outdex);
}

/**************************************************************************
* 函  数: void WifiInsertQueueData(char *data, u8 len, u8 cmdtype, u8 cmdWay)
* 描  述: 插入数据到队列
* 举  例: 
* 入  参: char *data：指令数据
*		  u8 len:指令数据长度
*		  u8 cmdtype:指令类型
		  u8 cmdWay:指令方式
* 出  参: void
* 返回值: void
**************************************************************************/
static void WifiInsertQueueData(char *data, u8 len, u8 cmdtype, u8 cmdWay)
{
	u8 index = g_wifiDataInfo.queue.index;
	u8 outdex = g_wifiDataInfo.queue.outdex;

	memset(&g_wifiDataInfo.queue.item[index], 0, sizeof(WifiCmdItemDataSt));
	g_wifiDataInfo.queue.item[index].type = cmdtype;
	g_wifiDataInfo.queue.item[index].way = cmdWay;
	g_wifiDataInfo.queue.item[index].len = len;
	memcpy(g_wifiDataInfo.queue.item[index].data, data, len);

	g_wifiDataInfo.queue.index++;
	if (g_wifiDataInfo.queue.index >= M_WIFI_MAX_QUEUE_CNT)
	{
		g_wifiDataInfo.queue.index = 0;
	}

	if (g_wifiDataInfo.queue.index == g_wifiDataInfo.queue.outdex)
	{
		memset(&g_wifiDataInfo.queue.item[outdex], 0, sizeof(WifiCmdItemDataSt));
		g_wifiDataInfo.queue.outdex++;
		if (g_wifiDataInfo.queue.outdex >= M_WIFI_MAX_QUEUE_CNT)
		{
			g_wifiDataInfo.queue.outdex = 0;
		}
	}
}

/**************************************************************************
* 函  数: void WifiDeleteQueueData(void)
* 描  述: 把队列中最早的数据从队列删除
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
static void WifiDeleteQueueData(void)
{
	if (g_wifiDataInfo.queue.index != g_wifiDataInfo.queue.outdex)
	{
		memset(&g_wifiDataInfo.queue.item[g_wifiDataInfo.queue.outdex], 0, sizeof(WifiCmdItemDataSt));
		g_wifiDataInfo.queue.item[g_wifiDataInfo.queue.outdex].type = (u8)E_WIFI_CMD_MAX;
		g_wifiDataInfo.queue.outdex++;
		if (g_wifiDataInfo.queue.outdex >= M_WIFI_MAX_QUEUE_CNT)
		{
			g_wifiDataInfo.queue.outdex = 0;
		}
	}
}

/**************************************************************************
* 函  数: int WifiPackCmdAt(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdAt(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiPackCmdRst(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdRst(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiPackCmdGmr(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdGmr(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdAte(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdAte(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdRestore(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdRestore(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdUart(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdUart(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwmode(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwmode(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwjap(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwjap(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwlap(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwlap(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwqap(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwqap(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwsap(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwsap(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwlif(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwlif(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwdhcp(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwdhcp(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCwautoconn(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCwautoconn(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipstamac(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipstamac(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipapmac(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipapmac(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipsta(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipsta(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipap(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipap(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdSavetranslink(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdSavetranslink(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipstatus(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipstatus(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipstart(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipstart(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipsend(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipsend(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipclose(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipclose(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCifsr(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCifsr(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipmux(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipmux(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCpserver(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCpserver(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipmode(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipmode(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCipsto(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCipsto(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdCiupdate(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdCiupdate(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiPackCmdPing(char *data, u8 cmdWay)
* 描  述: 组包指令数据信息
* 举  例: 
* 入  参: u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: int,M_WIFI_FAIL - 失败，0> 组包数据长度 - 成功
**************************************************************************/
static int WifiPackCmdPing(char *data, u8 cmdWay)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay)
* 描  述: 根据不同的命令类型及指令方式，组包数据，插入发送消息队列
* 举  例: 
* 入  参: WifiCmdTypeE type:命令类型
*		  u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: void
**************************************************************************/
void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay)
{
	u32 i = 0;
	u32 count = sizeof(g_wifiCmdInfo) / sizeof(g_wifiCmdInfo[0]);

	M_WIFI_TRACE_IN();

	for (i = 0; i < count; i++)
	{
		if (g_wifiCmdInfo[i].type == type)
		{
			break;
		}
	}

	if (i != count)
	{
		char data[M_WIFI_MAX_CMD_LEN] = {0};
		u8 datalen = 0;
		int ret = M_WIFI_FAIL;

		ret = g_wifiCmdInfo[i].packCb(data, cmdWay);
		if (M_WIFI_FAIL != ret)
		{
			datalen = (u8)(datalen & 0xff);
			WifiInsertQueueData(data, datalen, type, cmdWay);
		}
	}

	M_WIFI_TRACE_OUT();
}

/**************************************************************************
* 函  数: int WifiDealAt(char *pdata, u16 datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  u16 datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealAt(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{
		if (!strstr(data, "OK"))
		{
			break;
		}

		/* 设置模块可以正常工作 */
		g_wifiDataInfo.workFlag = True;

		ret = M_WIFI_SUCC;
	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRst(char *pdata, u16 datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  u16 datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealRst(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealGmr(char *pdata, u16 datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  u16 datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealGmr(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealAte(char *pdata, u16 datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  u16 datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealAte(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealRestore(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, u16 datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  u16 datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealUart(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwmode(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwjap(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwlap(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwqap(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwsap(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwlif(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwdhcp(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCwautoconn(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipstamac(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipapmac(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipsta(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}


/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipap(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealSavetranslink(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipstatus(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipstart(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipsend(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipclose(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCifsr(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipmux(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCpserver(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipmode(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCipsto(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealCiupdate(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealPing(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealOtherData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealOtherData(char *data, u16 len)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();

	do
	{

	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: int WifiDealRecvData(char *pdata, int datalen)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int datalen:接收到的数据长度
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，M_WIFI_SUCC - 成功
**************************************************************************/
static int WifiDealRecvData(char *pdata, int datalen)
{
	int ret = M_WIFI_FAIL;
	u32 count = sizeof(g_wifiCmdInfo) / sizeof(g_wifiCmdInfo[0]);
	u32 i = 0;

	M_WIFI_TRACE_IN();

	if ((u8)E_WIFI_CMD_MAX == g_wifiDataInfo.sendCmdType)
	{
		return M_WIFI_FAIL;
	}

	for (i = 0; i < count; i++)
	{
		if (g_wifiCmdInfo[i].type == g_wifiDataInfo.sendCmdType)
		{
			break;
		}
	}

	/* 有找到对应的指令类型 */
	if (i != count)
	{
		if (g_wifiCmdInfo[i].cb)
		{
			ret = g_wifiCmdInfo[i].cb(pdata, (u16)datalen);
			if (M_WIFI_FAIL != ret)
			{
				g_wifiDataInfo.sendCmdType = (u8)E_WIFI_CMD_MAX;
				g_wifiDataInfo.sendCnt = 0;
				g_wifiDataInfo.sendFlag = False;
				g_wifiDataInfo.delayTime = 0;

				/* 对应的指令类型处理成功了，从队列中删除数据 */
				WifiDeleteQueueData();
			}
		}
	}
	else
	{
		/* 模块主动上传的数据 */
		ret = WifiDealOtherData(pdata, (u16)datalen);
		M_WIFI_INFO("Wifi deal recv module data info, ret : %d", ret);
	}

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* 函  数: void WifiSendDataTask(void)
* 描  述: 发送数据任务，100ms任务
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
static void WifiSendDataTask(void)
{
	u8 count = WifiGetQueueCount();
	u8 index = g_wifiDataInfo.queue.index;
	u8 outdex = g_wifiDataInfo.queue.outdex; 
	u8 i = 0;
	u8 size = (u8)sizeof(g_wifiCmdInfo) / sizeof(g_wifiCmdInfo[0]);

	M_WIFI_TRACE_IN();

	if (count > 0)
	{
		if (!g_wifiDataInfo.sendFlag)
		{
			g_wifiDataInfo.sendCmdType = g_wifiDataInfo.queue.item[index].type;
			if ((g_wifiDataInfo.queue.item[index].len > 0) && (E_WIFI_CMD_MAX != g_wifiDataInfo.sendCmdType))
			{
				if (g_wifiDataInfo.sendCnt >= g_wifiCmdInfo[g_wifiDataInfo.sendCmdType].maxCnt)
				{
					g_wifiDataInfo.sendCnt = 0;
					g_wifiDataInfo.sendCmdType = (u8)E_WIFI_CMD_MAX;
					g_wifiDataInfo.delayTime = 0;

					/* 超过次数,出队列 */
					WifiDeleteQueueData();
					return;
				}
			
				//用串口中发送数据
				//UartSendData(g_wifiDataInfo.queue.item[index].data, g_wifiDataInfo.queue.item[index].len);
				g_wifiDataInfo.sendFlag = True;
				g_wifiDataInfo.sendCnt++;
				g_wifiDataInfo.delayTime = 0;
			}
			else
			{
				/* 数据长度不对,出队列 */
				WifiDeleteQueueData();
			}
		}
		else
		{
			g_wifiDataInfo.delayTime++;
			if (g_wifiDataInfo.delayTime >= g_wifiCmdInfo[g_wifiDataInfo.sendCmdType].maxOvertime)
			{
				/* 超时 */
				g_wifiDataInfo.delayTime = 0;
				g_wifiDataInfo.sendFlag = False;
			}
		}
	}
	
	M_WIFI_TRACE_OUT();
} 

/**************************************************************************
* 函  数: int WifiReadUartData(char *pdata, int maxLen, int outTime)
* 描  述: Wifi模块处理接收的数据
* 举  例: 
* 入  参: char *pdata:接收到的数据
*		  int maxLen:允许每次接收到的最大长度
*		  int outTime：读取WIFI串口数据超时时间
* 出  参: void
* 返回值: int,M_WIFI_FAIL - 失败，> 0 成功，返回读取到的数据长度信息
**************************************************************************/
static int WifiReadUartData(char *pdata, int maxLen, int outTime)
{
	int ret = M_WIFI_FAIL;
	int datalen = 0;

	M_WIFI_TRACE_IN();
	do
	{
		if ((0 == pdata) || (0 == maxLen))
		{
			M_WIFI_ERROR("Input param error.");
			break;
		}

		//从串口中读取数据
		//datalen = UartReadData(pdata, maxLen, outTime);
		if (datalen > 2)
		{
			ret = datalen;
		}
	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
} 

/**************************************************************************
* 函  数: void WifiTask(void)
* 描  述: Wifi模块业务逻辑任务,每隔100ms读取一次数据
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
void WifiTask(void)
{
	int ret = M_WIFI_FAIL;

	M_WIFI_TRACE_IN();
	
	memset(g_wifiDataInfo.recvData, 0, sizeof(g_wifiDataInfo.recvData));
	ret = WifiReadUartData(g_wifiDataInfo.recvData, sizeof(g_wifiDataInfo.recvData), M_WIFI_MAX_OVER_TIME);
	if (M_WIFI_FAIL != ret)
	{
		(void)WifiDealRecvData(g_wifiDataInfo.recvData, ret);
	}

	WifiSendDataTask();

	M_WIFI_TRACE_OUT();
}

/**************************************************************************
* 函  数: void WifiInit(void)
* 描  述: Wifi模块初始化上电流程
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
void WifiInit(void)
{
	M_WIFI_TRACE_IN();

	memset(&g_wifiDataInfo, 0, sizeof(g_wifiDataInfo));
	g_wifiDataInfo.sendCmdType = (u8)E_WIFI_CMD_MAX;

	/* 模块驱动程序 */
	//UsartInit(115200, 0);

	M_WIFI_TRACE_OUT();
}

