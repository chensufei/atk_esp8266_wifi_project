/**********************************************************************
**  ����: ATK_ESP8266_WIFI.c�ļ�
*   ����: 2019/5/20
*   ����: sf
*   �汾: V0.01
*   ����: ��Ҫ�漰��ʵ�ֵ�ESP8266WiFiģ����߼������У�
*		1. ......
*   �޸ļ�¼: 
*	   ����		�޸���		�汾 		�޸�����
*	2019/5/20 	sf			V0.01       �����ʼ���汾
*
*********************************************************************/
#include "Atk_Esp8266_wifi.h"

/**************************************************************************
** �궨��
**************************************************************************/
#define M_WIFI_SSID_MAX_LEN				(64)	/* wifi�����������󳤶� */
#define M_WIFI_PSWD_MAX_LEN				(32) 	/* wifi������󳤶� */
#define M_WIFI_MAC_MAX_LEN				(20)	/* Wifi�����Ľ�����mac��ַ */
#define M_WIFI_LIST_MAX_CNT				(20)	/* ���֧��20���������WIFI��Ϣ */
#define M_WIFI_IP_MAX_LEN				(16)	/* ���IP��Ϣ���� */
#define M_WIFI_MAX_IP_CNT				(5)		/* ���֧��5·IP */
#define M_WIFI_MAX_RECV_LEN				(1024)	/* �����ճ��� */
#define M_WIFI_MAX_OVER_TIME			(500)	/* ���ʱʱ��(ms) */

#define M_WIFI_MAX_CMD_LEN				(64)	/* ����ָ����󳤶� */
#define M_WIFI_MAX_QUEUE_CNT			(11)	/* ���Ͷ��������� */

#define M_WIFI_SET_BIT(n, x)			(n | ((1ul) << x))	/* x��0��ʼ */
#define M_WIFI_GET_BIT(n, x)			(n & ((1ul) << x))
#define M_WIFI_CLS_BIT(n, x)			(n & ~((1ul) << x))

/* ����ָ��Ļص����� */
typedef int (*WifiDealCmdCb)(char *data, u16 len);

/* �����Ϣ�Ļص����� */
typedef int (*WifiPackDataCb)(char *data, u8 cmdWay);

/**************************************************************************
** �ṹ�嶨��
**************************************************************************/
/* ��Ӧwifiָ�����Ϣ */
typedef struct _WifiCmdInfoSt
{
	WifiCmdTypeE type;			/* wifiָ������ */
	u8 maxCnt;					/* ����ָ�������� */
	u16 maxOvertime;			/* ÿ���������ʱʱ��,��λ100ms */
	char *cmdStr;				/* ָ���ַ��� */
	WifiDealCmdCb cb;			/* wifiָ���Ļص����� */
	WifiPackDataCb packCb;		/* wifi���������Ϣ */
} WifiCmdInfoSt;

/* WIFIӦ��ģʽ */
typedef enum _WifiAppModeE
{
	E_WIFI_MODE_STA = 1,		/* Station ģʽ�����豸ģʽ */
	E_WIFI_MODE_AP,				/* APģʽ���ȵ�ģʽ */
	E_WIFI_MODE_AP_STA,			/* AP+Stationģʽ */
} WifiAppModeE;

/* wifiģ���õ��Ĵ���������Ϣ */
typedef struct _WifiUartInfoSt
{
	int baud;					/* ������ */
	u8 dataBits:3;				/* ����λ,ȡֵ5,6,7,8 */
	u8 stopBits:2;				/* ֹͣλ,ȡֵ1-1bit,2-1.5bits,3-2bits */
	u8 parityBits:2;			/* У��λ,ȡֵ0-None,1-Odd,2-Even */
	u8 flowCtrl:1;				/* ���أ�0-���������أ�1-ͬʱʹ��RTS��CTS */
} WifiUartInfoSt;

/* wifiģ���õ���������Ϣ����Ҫ�洢��flash�� */
typedef struct _WifiFlashDataSt
{
	/* ����������Ϣ */
	WifiUartInfoSt uart;		/* ����������Ϣ */

	/* �����Ĳ��� */
	WifiAppModeE mode;			/* wifiӦ��ģʽ */

	u8 apSw:1;					/* apģʽ���أ�0-�رգ�1-���� */
	u8 staSw:1;					/* staģʽ���أ�0-�رգ�1-���� */
	u8 apstaSw:1;				/* ap��staģʽ���أ�0-�رգ�1-���� */
	u8 autoConnFlag:1;			/* ����ʹ��STAģʽ�Զ����ӣ�0-��ֹ��1-ʹ�� */
	u8 restore:4;				/* Ԥ��5bits */

	char apIp[M_WIFI_IP_MAX_LEN];	/* Apģʽ�µ�IP��ַ */ 
	char apMac[M_WIFI_MAC_MAX_LEN];	/* Apģʽ�µ�mac��ַ */ 

	char staIp[M_WIFI_IP_MAX_LEN];	/* Staģʽ�µ�IP��ַ */ 
	char staMac[M_WIFI_MAC_MAX_LEN];/* Staģʽ�µ�mac��ַ */ 

	char ssid[M_WIFI_SSID_MAX_LEN];	/* wifi��������� */
	char pswd[M_WIFI_PSWD_MAX_LEN];	/* wifi���� */
} WifiFlashDataSt;

/* wifi�����ļ������� */
typedef enum _WifiEncryptTypeE
{
	E_WIFI_ENPT_TYPE_OPEN = 0,		/* ����Ҫ���� */
	E_WIFI_ENPT_TYPE_WEP,			/* WEP���ܷ�ʽ */
	E_WIFI_ENPT_TYPE_WPA_PSK,		/* WPA_PSK���ܷ�ʽ */
	E_WIFI_ENPT_TYPE_WPA2_PSK,		/* WPA2_PSK���ܷ�ʽ */
	E_WIFI_ENPT_TYPE_WPA_WPA2_PSK,	/* WPA_WPA2_PSK���ܷ�ʽ */
} WifiEncryptTypeE;

/* ��ѯ��ǰ����wifi��Ϣ���������ƣ��ź�ǿ�ȣ�mac��ַ��ͨ���� */
typedef struct _WifiListInfoSt
{
	WifiEncryptTypeE eptyType;		/* �������� */
	s8 rssi;						/* �ź�ǿ�� */
	u8 channel;						/* ͨ�� */
	char ssid[M_WIFI_SSID_MAX_LEN];	/* wifi��������� */
	char mac[M_WIFI_MAC_MAX_LEN];	/* mac��ַ */
} WifiListInfoSt;

/* Serverģʽ�£������·IP����Ϣ */
typedef struct _WifiMuxIpInfoSt
{
	u8 sockId;						/* socket id��0-4��֧��5· */
	u8 sta;							/* ��ǰ��socked idͨ����״̬��2-��ȡIp,3-�������ӣ�4-ʧȥ���� */
	u8 sockType;					/* socket type, 0 - tcp, 1- udp */
	char remoteIp[M_WIFI_IP_MAX_LEN];/* ����ģ���Զ���豸IP��ַ */
	u16 remotePort;					/* ����ģ���Զ���豸�Ķ˿� */
} WifiMuxIpInfoSt;

/* ����ָ�ʽ */
typedef enum _WifiSendCmdWayE
{
	E_WIFI_CMD_TEST = 0,			/* ����ָ�� */
	E_WIFI_CMD_QUERY,				/* ��ѯָ�� */
	E_WIFI_CMD_SET,					/* ����ָ�� */
	E_WIFI_CMD_EXE,					/* ִ��ָ�� */
} WifiSendCmdWayE;

/* ÿһ��ָ������������Ϣ */
typedef struct _WifiCmdItemDataSt
{
	u8 way;							/* ָ�ʽ,0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ�� */
	u8 type;						/* ����ָ������ */
	u8 len;							/* ����� */
	char data[M_WIFI_MAX_CMD_LEN];	/* ����ָ������ */
} WifiCmdItemDataSt;

/* �������ݶ��нṹ�� */
typedef struct _WifiCmdQueueSt
{
	u8 index;						/* ��������� */
	u8 outdex;						/* ���������� */
	WifiCmdItemDataSt item[M_WIFI_MAX_QUEUE_CNT];	/* �������ݶ��� */
} WifiCmdQueueSt;

/* wifiģ���õ���������Ϣ�ṹ�� */
typedef struct _WifiDataInfoSt
{
	u8 ateSw:1;						/* ���Կ���,0-�رջ��ԣ�1-�������� */
	u8 muxIpFlag:1;					/* ֧�ֶ�·IP�ı�־��1 - �����·��0-��·��Ĭ�ϵ�· */
	u8 serverModeFlag:1;			/* ������ģʽ��0 - �رշ�����ģʽ��1-����������ģʽ��ֻ���ڶ�·IP��������£���֧�ֿ�ʼ������ģʽ */
	u8 sendFlag:1;					/* ���ͱ�־,0-δ���ͣ�1-�ѷ��� */
	u8 workFlag:1;					/* wifiģ�鹤��״̬��0-δ������1-���� */

	u8 restore:3;
	u8 sendCmdWay;					/* ����ָ�ʽ */
	u8 sendCmdType;					/* ����ָ������ */
	u8 sendCnt;						/* ���ʹ��� */
	u8 delayTime;					/* ��ʱʱ�� */

	WifiCmdQueueSt queue;			/* �������� */
	char recvData[M_WIFI_MAX_RECV_LEN];			/* ����WIFIģ������� */
	WifiMuxIpInfoSt muxIp[M_WIFI_MAX_IP_CNT];	/* ��Ϊ������ģʽ�£�������client */
	WifiListInfoSt list[M_WIFI_LIST_MAX_CNT];	/* ����������WIFI��Ϣ */
} WifiDataInfoSt;

/**************************************************************************
** ��������
**************************************************************************/
/* �����Ӧָ��Ľ��ջص����� */
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

/* ������������ */
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
** ȫ�ֱ�������
**************************************************************************/
/* ����Wifi ATָ����Ϣ */
static const WifiCmdInfoSt g_wifiCmdInfo[] =
{
	/* ����ATָ�� */
	{E_WIFI_CMD_AT, 			10,	5,	"AT",				WifiDealAt,				WifiPackCmdAt},			/* AT����ָ�� */
	{E_WIFI_CMD_RST, 			3,	10,	"AT+RST",			WifiDealRst,			WifiPackCmdRst},		/* ����ָ�� */
	{E_WIFI_CMD_GMR, 			3,	5,	"AT+GMR",			WifiDealGmr,			WifiPackCmdGmr},		/* �汾��Ϣ */
	{E_WIFI_CMD_ATE,			3,	5,	"ATE",				WifiDealAte,			WifiPackCmdAte},		/* ���ػ��Թ��� */
	{E_WIFI_CMD_RESTORE,		3,	10,	"AT+RESTORE",		WifiDealRestore,		WifiPackCmdRestore},	/* �ָ���������,ͬʱģ������� */
	{E_WIFI_CMD_UART,			3,	10,	"AT+UART",			WifiDealUart,			WifiPackCmdUart},		/* ���ô������� */

	/* Wifi����ATָ�� */
	{E_WIFI_CMD_CWMODE,			3,	10,	"AT+CWMODE",		WifiDealCwmode,			WifiPackCmdCwmode},			/* ѡ�� WIFI Ӧ��ģʽ */
	{E_WIFI_CMD_CWJAP,			3,	10,	"AT+CWJAP",			WifiDealCwjap,			WifiPackCmdCwjap},			/* ���� AP */
	{E_WIFI_CMD_CWLAP,			3,	10,	"AT+CWLAP",			WifiDealCwlap,			WifiPackCmdCwlap},			/* �г���ǰ���� AP */
	{E_WIFI_CMD_CWQAP,			3,	10,	"AT+CWQAP",			WifiDealCwqap,			WifiPackCmdCwqap},			/* �˳��� AP ������ */
	{E_WIFI_CMD_CWSAP,			3,	10,	"AT+CWSAP",			WifiDealCwsap,			WifiPackCmdCwsap},			/* ���� AP ģʽ�µĲ��� */
	{E_WIFI_CMD_CWLIF,			3,	10,	"AT+CWLIF",			WifiDealCwlif,			WifiPackCmdCwlif},			/* �鿴�ѽ����豸�� IP */
	{E_WIFI_CMD_CWDHCP,			3,	10,	"AT+CWDHCP",		WifiDealCwdhcp,			WifiPackCmdCwdhcp},			/* ���� DHCP ���� */
	{E_WIFI_CMD_CWAUTOCONN,		3,	10,	"AT+CWAUTOCONN",	WifiDealCwautoconn,		WifiPackCmdCwautoconn},		/* ���� STA �����Զ����ӵ� wifi */
	{E_WIFI_CMD_CIPSTAMAC,		3,	10,	"AT+CIPSTAMAC",		WifiDealCipstamac,		WifiPackCmdCipstamac},		/* ���� STA �� MAC ��ַ */
	{E_WIFI_CMD_CIPAPMAC,		3,	10,	"AT+CIPAPMAC",		WifiDealCipapmac,		WifiPackCmdCipapmac},		/* ���� AP �� MAC ��ַ */
	{E_WIFI_CMD_CIPSTA,			3,	10,	"AT+CIPSTA",		WifiDealCipsta,			WifiPackCmdCipsta},			/* ���� STA �� IP ��ַ */
	{E_WIFI_CMD_CIPAP,			3,	10,	"AT+CIPAP",			WifiDealCipap,			WifiPackCmdCipap},			/* ���� AP �� IP ��ַ */
	{E_WIFI_CMD_SAVETRANSLINK,	3,	10,	"AT+SAVETRANSLINK",	WifiDealSavetranslink,	WifiPackCmdSavetranslink},	/* ����͸�����ӵ� Flash */

	/* TCP/IP������ATָ�� */
	{E_WIFI_CMD_CIPSTATUS,		3,	10,	"AT+CIPSTATUS",		WifiDealCipstatus,		WifiPackCmdCipstatus},	/* �������״̬ */
	{E_WIFI_CMD_CIPSTART,		3,	10,	"AT+CIPSTART",		WifiDealCipstart,		WifiPackCmdCipstart},	/* ���� TCP ���ӻ�ע�� UDP �˿ں� */
	{E_WIFI_CMD_CIPSEND,		3,	10,	"AT+CIPSEND",		WifiDealCipsend,		WifiPackCmdCipsend},	/* �������� */
	{E_WIFI_CMD_CIPCLOSE,		3,	10,	"AT+CIPCLOSE",		WifiDealCipclose,		WifiPackCmdCipclose},	/* �ر� TCP �� UDP */
	{E_WIFI_CMD_CIFSR,			3,	10,	"AT+CIFSR",			WifiDealCifsr,			WifiPackCmdCifsr},		/* ��ȡ���� IP ��ַ */
	{E_WIFI_CMD_CIPMUX,			3,	10,	"AT+CIPMUX",		WifiDealCipmux,			WifiPackCmdCipmux},		/* ���������� */
	{E_WIFI_CMD_CIPSERVER,		3,	10,	"AT+CIPSERVER",		WifiDealCpserver,		WifiPackCmdCpserver},	/* ����Ϊ������ */
	{E_WIFI_CMD_CIPMODE,		3,	10,	"AT+CIPMODE",		WifiDealCipmode,		WifiPackCmdCipmode},	/* ����ģ�鴫��ģʽ */
	{E_WIFI_CMD_CIPSTO,			3,	10,	"AT+CIPSTO",		WifiDealCipsto,			WifiPackCmdCipsto},		/* ���÷�������ʱʱ�� */
	{E_WIFI_CMD_CIUPDATE,		3,	10,	"AT+CIUPDATE",		WifiDealCiupdate,		WifiPackCmdCiupdate},	/* ���������̼� */
	{E_WIFI_CMD_PING,			3,	10,	"AT+PING",			WifiDealPing,			WifiPackCmdPing},		/* PING ���� */

};

/* wifi������Ϣ */
static WifiDataInfoSt g_wifiDataInfo;

/**************************************************************************
** ��������
**************************************************************************/
/**************************************************************************
* ��  ��: u8 WifiGetQueueCount(void)
* ��  ��: ��ȡ���и�����
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: u8�������еĸ���
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
* ��  ��: void WifiInsertQueueData(char *data, u8 len, u8 cmdtype, u8 cmdWay)
* ��  ��: �������ݵ�����
* ��  ��: 
* ��  ��: char *data��ָ������
*		  u8 len:ָ�����ݳ���
*		  u8 cmdtype:ָ������
		  u8 cmdWay:ָ�ʽ
* ��  ��: void
* ����ֵ: void
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
* ��  ��: void WifiDeleteQueueData(void)
* ��  ��: �Ѷ�������������ݴӶ���ɾ��
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
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
* ��  ��: int WifiPackCmdAt(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdRst(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdGmr(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdAte(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdRestore(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdUart(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwmode(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwjap(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwlap(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwqap(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwsap(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwlif(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwdhcp(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCwautoconn(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipstamac(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipapmac(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipsta(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipap(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdSavetranslink(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipstatus(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipstart(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipsend(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipclose(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCifsr(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipmux(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCpserver(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipmode(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCipsto(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdCiupdate(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: int WifiPackCmdPing(char *data, u8 cmdWay)
* ��  ��: ���ָ��������Ϣ
* ��  ��: 
* ��  ��: u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�0> ������ݳ��� - �ɹ�
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
* ��  ��: void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay)
* ��  ��: ���ݲ�ͬ���������ͼ�ָ�ʽ��������ݣ����뷢����Ϣ����
* ��  ��: 
* ��  ��: WifiCmdTypeE type:��������
*		  u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: void
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
* ��  ��: int WifiDealAt(char *pdata, u16 datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  u16 datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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

		/* ����ģ������������� */
		g_wifiDataInfo.workFlag = True;

		ret = M_WIFI_SUCC;
	} while(0);

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* ��  ��: int WifiDealRst(char *pdata, u16 datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  u16 datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealGmr(char *pdata, u16 datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  u16 datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealAte(char *pdata, u16 datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  u16 datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, u16 datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  u16 datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealOtherData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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
* ��  ��: int WifiDealRecvData(char *pdata, int datalen)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int datalen:���յ������ݳ���
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�M_WIFI_SUCC - �ɹ�
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

	/* ���ҵ���Ӧ��ָ������ */
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

				/* ��Ӧ��ָ�����ʹ���ɹ��ˣ��Ӷ�����ɾ������ */
				WifiDeleteQueueData();
			}
		}
	}
	else
	{
		/* ģ�������ϴ������� */
		ret = WifiDealOtherData(pdata, (u16)datalen);
		M_WIFI_INFO("Wifi deal recv module data info, ret : %d", ret);
	}

	M_WIFI_TRACE_OUT();

	return ret;
}

/**************************************************************************
* ��  ��: void WifiSendDataTask(void)
* ��  ��: ������������100ms����
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
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

					/* ��������,������ */
					WifiDeleteQueueData();
					return;
				}
			
				//�ô����з�������
				//UartSendData(g_wifiDataInfo.queue.item[index].data, g_wifiDataInfo.queue.item[index].len);
				g_wifiDataInfo.sendFlag = True;
				g_wifiDataInfo.sendCnt++;
				g_wifiDataInfo.delayTime = 0;
			}
			else
			{
				/* ���ݳ��Ȳ���,������ */
				WifiDeleteQueueData();
			}
		}
		else
		{
			g_wifiDataInfo.delayTime++;
			if (g_wifiDataInfo.delayTime >= g_wifiCmdInfo[g_wifiDataInfo.sendCmdType].maxOvertime)
			{
				/* ��ʱ */
				g_wifiDataInfo.delayTime = 0;
				g_wifiDataInfo.sendFlag = False;
			}
		}
	}
	
	M_WIFI_TRACE_OUT();
} 

/**************************************************************************
* ��  ��: int WifiReadUartData(char *pdata, int maxLen, int outTime)
* ��  ��: Wifiģ�鴦����յ�����
* ��  ��: 
* ��  ��: char *pdata:���յ�������
*		  int maxLen:����ÿ�ν��յ�����󳤶�
*		  int outTime����ȡWIFI�������ݳ�ʱʱ��
* ��  ��: void
* ����ֵ: int,M_WIFI_FAIL - ʧ�ܣ�> 0 �ɹ������ض�ȡ�������ݳ�����Ϣ
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

		//�Ӵ����ж�ȡ����
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
* ��  ��: void WifiTask(void)
* ��  ��: Wifiģ��ҵ���߼�����,ÿ��100ms��ȡһ������
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
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
* ��  ��: void WifiInit(void)
* ��  ��: Wifiģ���ʼ���ϵ�����
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
**************************************************************************/
void WifiInit(void)
{
	M_WIFI_TRACE_IN();

	memset(&g_wifiDataInfo, 0, sizeof(g_wifiDataInfo));
	g_wifiDataInfo.sendCmdType = (u8)E_WIFI_CMD_MAX;

	/* ģ���������� */
	//UsartInit(115200, 0);

	M_WIFI_TRACE_OUT();
}

