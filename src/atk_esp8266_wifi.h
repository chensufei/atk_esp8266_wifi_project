/**********************************************************************
**  名称: ATK_ESP8266_WIFI.h文件
*   日期: 2019/5/21
*   作者: sf
*   版本: V0.01
*   描述: 主要涉及到实现的ESP8266WiFi模块的逻辑功能有：
*		1. ......
*   修改记录: 
*	   日期		修改人		版本 		修改内容
*	2019/5/21 	  sf		V0.01       软件初始化版本
*
*********************************************************************/
#ifndef __ATK_ESP8266_WIFI_H__
#define __ATK_ESP8266_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
** 宏定义
**************************************************************************/
#ifndef False
#define False				(0)
#endif

#ifndef True
#define True				(1)
#endif


#define M_WIFI_FAIL			(-1)	/* 失败 */	
#define M_WIFI_SUCC			(0)		/* 成功 */

#define M_WIFI_DEBUG		(1)		/* debug开关 */

/* 打印重定义 */
#if M_WIFI_DEBUG > 0
#define M_WIFI_DEBUG(format,...)  	DBG_Print(LEVEL5, "[Wifi][%s][%d] debug: "format"\r\n", __func__, __LINE__, ##__VA_ARGS__)
#define M_WIFI_WARN(format,...)  	DBG_Print(LEVEL5, "[Wifi][%s][%d] warn: "format"\r\n", __func__, __LINE__, ##__VA_ARGS__)
#define M_WIFI_ERROR(format,...)  	DBG_Print(LEVEL5, "[Wifi][%s][%d] error: "format"\r\n", __func__, __LINE__, ##__VA_ARGS__)
#define M_WIFI_INFO(format,...)  	DBG_Print(LEVEL5, "[Wifi][%s][%d] info: "format"\r\n", __func__, __LINE__, ##__VA_ARGS__)
#define M_WIFI_TRACE_IN()			DBG_Print(LEVEL5, "[Wifi][%s][%d] trace in\r\n", __func__, __LINE__)
#define M_WIFI_TRACE_OUT()			DBG_Print(LEVEL5, "[Wifi][%s][%d] trace out\r\n", __func__, __LINE__)
#else
#define M_WIFI_DEBUG(format,...)
#define M_WIFI_WARN(format,...)
#define M_WIFI_ERROR(format,...)
#define M_WIFI_INFO(format,...)
#define M_WIFI_TRACE_IN()
#define M_WIFI_TRACE_OUT()
#endif

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

/**************************************************************************
** 结构体定义
**************************************************************************/
/* 指令类型 */
typedef enum _WifiCmdTypeE
{
	/* 基础AT指令 */
	E_WIFI_CMD_AT = 0,			/* 测试AT指令 */
	E_WIFI_CMD_RST, 			/* 重启模块指令 */
	E_WIFI_CMD_GMR, 			/* 版本信息 */
	E_WIFI_CMD_ATE, 			/* 开关回显功能 */
	E_WIFI_CMD_RESTORE, 		/* 恢复出厂设置 */
	E_WIFI_CMD_UART,			/* 设置串口配置 */

	/* Wifi功能AT指令 */
	E_WIFI_CMD_CWMODE,			/* 选择 WIFI 应用模式 */
	E_WIFI_CMD_CWJAP,			/* 加入 AP */
	E_WIFI_CMD_CWLAP,			/* 列出当前可用 AP */
	E_WIFI_CMD_CWQAP,			/* 退出与 AP 的连接 */
	E_WIFI_CMD_CWSAP,			/* 设置 AP 模式下的参数 */
	E_WIFI_CMD_CWLIF,			/* 查看已接入设备的 IP */
	E_WIFI_CMD_CWDHCP,			/* 设置 DHCP 开关 */
	E_WIFI_CMD_CWAUTOCONN,		/* 设置 STA 开机自动连接到 wifi */
	E_WIFI_CMD_CIPSTAMAC,		/* 设置 STA 的 MAC 地址 */
	E_WIFI_CMD_CIPAPMAC,		/* 设置 AP 的 MAC 地址 */
	E_WIFI_CMD_CIPSTA,			/* 设置 STA 的 IP 地址 */
	E_WIFI_CMD_CIPAP,			/* 设置 AP 的 IP 地址 */
	E_WIFI_CMD_SAVETRANSLINK,	/* 保存透传连接到 Flash */

	/* TCP/IP工具箱AT指令 */
	E_WIFI_CMD_CIPSTATUS,		/* 获得连接状态 */
	E_WIFI_CMD_CIPSTART,		/* 建立 TCP 连接或注册 UDP 端口号 */
	E_WIFI_CMD_CIPSEND, 		/* 发送数据 */
	E_WIFI_CMD_CIPCLOSE,		/* 关闭 TCP 或 UDP */
	E_WIFI_CMD_CIFSR,			/* 获取本地 IP 地址 */
	E_WIFI_CMD_CIPMUX,			/* 启动多连接 */
	E_WIFI_CMD_CIPSERVER,		/* 配置为服务器 */
	E_WIFI_CMD_CIPMODE, 		/* 设置模块传输模式 */
	E_WIFI_CMD_CIPSTO,			/* 设置服务器超时时间 */
	E_WIFI_CMD_CIUPDATE,		/* 网络升级固件 */
	E_WIFI_CMD_PING,			/* PING 命令 */
	E_WIFI_CMD_MAX,
} WifiCmdTypeE;

/**************************************************************************
** 函数声明
**************************************************************************/
/**************************************************************************
* 依赖接口
**************************************************************************/
//读串口数据信息
//int UartReadData(void *pdata, u32 maxLen, u32 outTime);
//向串口发送数据信息
//void UartSendData(char *data, int datalen);
//初始化串口信息
//void UsartInit(int baud);

/**************************************************************************
* 函  数: void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay)
* 描  述: 根据不同的命令类型及指令方式，组包数据，插入发送消息队列
* 举  例: 
* 入  参: WifiCmdTypeE type:命令类型
*		  u8 cmdWay:组包不同指令的方式，0-测试指令，1-查询指令，2-设置指令，3-执行指令
* 出  参: char *data:接收到的数据
* 返回值: void
**************************************************************************/
void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay);
/**************************************************************************
* 函  数: void WifiTask(void)
* 描  述: Wifi模块业务逻辑任务,每隔100ms读取一次数据
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
void WifiTask(void);

/**************************************************************************
* 函  数: void WifiInit(void)
* 描  述: Wifi模块初始化上电流程
* 举  例: 
* 入  参: void
* 出  参: void
* 返回值: void
**************************************************************************/
void WifiInit(void);

#ifdef __cplusplus
}
#endif

#endif

