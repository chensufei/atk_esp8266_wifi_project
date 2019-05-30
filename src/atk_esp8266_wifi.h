/**********************************************************************
**  ����: ATK_ESP8266_WIFI.h�ļ�
*   ����: 2019/5/21
*   ����: sf
*   �汾: V0.01
*   ����: ��Ҫ�漰��ʵ�ֵ�ESP8266WiFiģ����߼������У�
*		1. ......
*   �޸ļ�¼: 
*	   ����		�޸���		�汾 		�޸�����
*	2019/5/21 	  sf		V0.01       �����ʼ���汾
*
*********************************************************************/
#ifndef __ATK_ESP8266_WIFI_H__
#define __ATK_ESP8266_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
** �궨��
**************************************************************************/
#ifndef False
#define False				(0)
#endif

#ifndef True
#define True				(1)
#endif


#define M_WIFI_FAIL			(-1)	/* ʧ�� */	
#define M_WIFI_SUCC			(0)		/* �ɹ� */

#define M_WIFI_DEBUG		(1)		/* debug���� */

/* ��ӡ�ض��� */
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
** �ṹ�嶨��
**************************************************************************/
/* ָ������ */
typedef enum _WifiCmdTypeE
{
	/* ����ATָ�� */
	E_WIFI_CMD_AT = 0,			/* ����ATָ�� */
	E_WIFI_CMD_RST, 			/* ����ģ��ָ�� */
	E_WIFI_CMD_GMR, 			/* �汾��Ϣ */
	E_WIFI_CMD_ATE, 			/* ���ػ��Թ��� */
	E_WIFI_CMD_RESTORE, 		/* �ָ��������� */
	E_WIFI_CMD_UART,			/* ���ô������� */

	/* Wifi����ATָ�� */
	E_WIFI_CMD_CWMODE,			/* ѡ�� WIFI Ӧ��ģʽ */
	E_WIFI_CMD_CWJAP,			/* ���� AP */
	E_WIFI_CMD_CWLAP,			/* �г���ǰ���� AP */
	E_WIFI_CMD_CWQAP,			/* �˳��� AP ������ */
	E_WIFI_CMD_CWSAP,			/* ���� AP ģʽ�µĲ��� */
	E_WIFI_CMD_CWLIF,			/* �鿴�ѽ����豸�� IP */
	E_WIFI_CMD_CWDHCP,			/* ���� DHCP ���� */
	E_WIFI_CMD_CWAUTOCONN,		/* ���� STA �����Զ����ӵ� wifi */
	E_WIFI_CMD_CIPSTAMAC,		/* ���� STA �� MAC ��ַ */
	E_WIFI_CMD_CIPAPMAC,		/* ���� AP �� MAC ��ַ */
	E_WIFI_CMD_CIPSTA,			/* ���� STA �� IP ��ַ */
	E_WIFI_CMD_CIPAP,			/* ���� AP �� IP ��ַ */
	E_WIFI_CMD_SAVETRANSLINK,	/* ����͸�����ӵ� Flash */

	/* TCP/IP������ATָ�� */
	E_WIFI_CMD_CIPSTATUS,		/* �������״̬ */
	E_WIFI_CMD_CIPSTART,		/* ���� TCP ���ӻ�ע�� UDP �˿ں� */
	E_WIFI_CMD_CIPSEND, 		/* �������� */
	E_WIFI_CMD_CIPCLOSE,		/* �ر� TCP �� UDP */
	E_WIFI_CMD_CIFSR,			/* ��ȡ���� IP ��ַ */
	E_WIFI_CMD_CIPMUX,			/* ���������� */
	E_WIFI_CMD_CIPSERVER,		/* ����Ϊ������ */
	E_WIFI_CMD_CIPMODE, 		/* ����ģ�鴫��ģʽ */
	E_WIFI_CMD_CIPSTO,			/* ���÷�������ʱʱ�� */
	E_WIFI_CMD_CIUPDATE,		/* ���������̼� */
	E_WIFI_CMD_PING,			/* PING ���� */
	E_WIFI_CMD_MAX,
} WifiCmdTypeE;

/**************************************************************************
** ��������
**************************************************************************/
/**************************************************************************
* �����ӿ�
**************************************************************************/
//������������Ϣ
//int UartReadData(void *pdata, u32 maxLen, u32 outTime);
//�򴮿ڷ���������Ϣ
//void UartSendData(char *data, int datalen);
//��ʼ��������Ϣ
//void UsartInit(int baud);

/**************************************************************************
* ��  ��: void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay)
* ��  ��: ���ݲ�ͬ���������ͼ�ָ�ʽ��������ݣ����뷢����Ϣ����
* ��  ��: 
* ��  ��: WifiCmdTypeE type:��������
*		  u8 cmdWay:�����ָͬ��ķ�ʽ��0-����ָ�1-��ѯָ�2-����ָ�3-ִ��ָ��
* ��  ��: char *data:���յ�������
* ����ֵ: void
**************************************************************************/
void WifiSendCmdData(WifiCmdTypeE type, u8 cmdWay);
/**************************************************************************
* ��  ��: void WifiTask(void)
* ��  ��: Wifiģ��ҵ���߼�����,ÿ��100ms��ȡһ������
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
**************************************************************************/
void WifiTask(void);

/**************************************************************************
* ��  ��: void WifiInit(void)
* ��  ��: Wifiģ���ʼ���ϵ�����
* ��  ��: 
* ��  ��: void
* ��  ��: void
* ����ֵ: void
**************************************************************************/
void WifiInit(void);

#ifdef __cplusplus
}
#endif

#endif

