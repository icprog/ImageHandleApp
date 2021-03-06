#include "UDPCommunications.h"
#include <atlconv.h>

/***************************************************************************
* 函数名称：   UDPCommunications
* 摘　　要：   
* 全局影响：   public 
* 返回值　：   
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
UDPCommunications::UDPCommunications()
{
}

/***************************************************************************
* 函数名称：   ~UDPCommunications
* 摘　　要：   
* 全局影响：   virtual public 
* 返回值　：   
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
UDPCommunications::~UDPCommunications()
{
	ReleaseCom();
}

/***************************************************************************
* 函数名称：   InitCom
* 摘　　要：   
* 全局影响：   virtual public 
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::InitCom()                    // 初始化相关函数
{
	int iInitialError;								// 函数返回值
	WSADATA data;       							// 初始化套接字
	iInitialError = WSAStartup(MAKEWORD(1, 1), &data);	// 2.2版本套接字

	//  initiates use of WS2_32.DLL
	if (0 != iInitialError)
		return FALSE;

	// 判断加载的套接字版本是否正确
	if (LOBYTE(data.wVersion) != 1 || HIBYTE(data.wVersion) != 1)
	{
		// release use of WS2_32.DLL
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}

/***************************************************************************
* 函数名称：   Send
* 摘　　要：   
* 全局影响：   virtual public 
* 参　　数：   [in]  BYTE abyDestIp[IP_BYTE_LENGTH]
* 参　　数：   [in]  UINT uiDesPort
* 参　　数：   [in]  byte * pbyBuffer
* 参　　数：   [in]  UINT uiDataLength
* 参　　数：   [in]  UINT uiFrameNum
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::Send(BYTE abyDestIp[IP_BYTE_LENGTH], UINT uiDesPort,
	byte *pbyBuffer, size_t uiDataLength, size_t uiFrameNum)
{
	USES_CONVERSION;

	char pcDesIp[IP_CHAR_LENGTH];
	ByteIpToStringIp(abyDestIp, pcDesIp);

	sockaddr_in sdServeraddr;                    //目的服务器端地址簇
	sdServeraddr.sin_family = AF_INET;
	sdServeraddr.sin_addr.S_un.S_addr = inet_addr(pcDesIp);
	sdServeraddr.sin_port = htons(uiDesPort);    //端口号,主机字节序转换为网络字节序，等待发送

	//开辟发送缓存区大小为MAXLENGTH字节
	PST_PACKAGE pstSendPacket = new ST_PACKAGE;

	//初始化发送缓存
	ZeroMemory(pstSendPacket->abyData, DATA_BUF_LENGTH);	// 数据缓存
	pstSendPacket->uiDataLength = -1;						// 数据包中数据长度
	pstSendPacket->uiFrameNum = uiFrameNum;			        // 数据包帧号
	pstSendPacket->uiPackageNum = -1;						// 数据包包号
	pstSendPacket->uiTotalDataLength = uiDataLength;		// 待发送数据的总长度（由于UDP本身的限制，在发送过程中将会发生切包）
	memset(pstSendPacket->abySourceIp, 0, IP_BYTE_LENGTH);
	pstSendPacket->uiSourcePort = 0;

	//如果数据未发送完，则循环发送数据
	while (uiDataLength != 0)
	{
		size_t uipacknum = 0;                       //用于计数数据包的个数，以填充发送的数据包包号

		//数据包总长度大于一个数据包中数据的容量
		while (uiDataLength > DATA_BUF_LENGTH)
		{

			// 填充数据包包号
			pstSendPacket->uiPackageNum = uipacknum;
			// 当前数据包中数据长度
			pstSendPacket->uiDataLength = DATA_BUF_LENGTH;
			// UDP包数据
			memcpy(pstSendPacket->abyData, pbyBuffer + DATA_BUF_LENGTH * uipacknum, DATA_BUF_LENGTH); //循环复制缓冲区数据到数据包结构体

			// 每次发送DATA_BUF个字节
			int reti = sendto(m_sktUDPSender, (const char*)pstSendPacket, sizeof(ST_PACKAGE),
				0, (SOCKADDR*)&sdServeraddr, sizeof(SOCKADDR)); //发送数据包

			// 判断是否发送成功
			if (SOCKET_ERROR == reti) // CCsccLog::DebugPrint("数据发送出错！\n");
				return FALSE;

			// 一帧数据剩余的数据量
			uiDataLength -= DATA_BUF_LENGTH;
			// 数据包包号加一
			uipacknum++;
		}

		if (uiDataLength > 0)//剩余字节发送
		{
			//最后一个数据包会比DATA_BUF小，故需要将内存全部格式化为0
			ZeroMemory(pstSendPacket->abyData, DATA_BUF_LENGTH); //清空缓存,使未填满的数据也初始化为0
			//填充数据包包号
			pstSendPacket->uiPackageNum = uipacknum;

			// 当前数据包中数据长度
			pstSendPacket->uiDataLength = uiDataLength;

			//拷贝发送数据至数据包
			memcpy(pstSendPacket->abyData, pbyBuffer + uipacknum * DATA_BUF_LENGTH, uiDataLength); //将剩余数据拷贝到数据包中准备发送

			//发送最后一个数据包，数据包中的数据长度为剩余所有字节，且不大于DATA_BUF
			int reti = sendto(m_sktUDPSender, (const char*)pstSendPacket, sizeof(ST_PACKAGE),
				0, (sockaddr*)&sdServeraddr, sizeof(sockaddr));

			//判断是否发送成功
			if (SOCKET_ERROR == reti)
				return FALSE;

			uiDataLength = 0; //表示数据已经发送完成
			uipacknum++;     //数据包包号加一
		}
	}

	delete pstSendPacket;   //删除数据缓存指针
	pstSendPacket = NULL;   //置空数据缓存指针

	return TRUE;
}

/***************************************************************************
* 函数名称：   SetSender
* 摘　　要：   
* 全局影响：   virtual public 
* 参　　数：   [in]  BYTE abyLocalIp[IP_BYTE_LENGTH]
* 参　　数：   [in]  UINT uiLocalPort
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::SetSender(BYTE abyLocalIp[IP_BYTE_LENGTH], UINT uiLocalPort)
{
	USES_CONVERSION;

	if (m_sktUDPSender != INVALID_SOCKET)
		closesocket(m_sktUDPSender);
	// 创建UDP发送套接字
	m_sktUDPSender = socket(AF_INET,      // 协议地址家族，windows下一般为AF_INET
		SOCK_DGRAM,   // 协议套接字类型，此处UDP采用数据报型
		IPPROTO_UDP); // 协议，此处采用UDP协议

	// 判断发送套接字是否创建成功
	if (INVALID_SOCKET == m_sktUDPSender)
	{
		// 得到错误代码
		int iSocketErr = WSAGetLastError();

		// 判断是否是套接字版本不正确
		if (iSocketErr == WSANOTINITIALISED)// 创建套接字失败，返回不可用的套接字
			return WSA_CREATE_SOCKET_ERROR;
	}

	int iErrCode;						// 定义错误代码
	UINT uiBuflen;						// 待设置的套接字缓冲区大小  
	UINT uiOptlen = sizeof(uiBuflen);	// 缓冲区大小

	// 获取发送缓冲区参数
	iErrCode = getsockopt(m_sktUDPSender,			// 套接字
		SOL_SOCKET,			// 套接字级别，此处选择套接字所在的应用层
		SO_SNDBUF,			// 将要设置的套接字选项名称，此处为缓冲区大小
		(char *)&uiBuflen,	// 设置的套接字选项值
		(int*)&uiOptlen);		// 缓冲区大小

	// 判断获取缓冲区参数是否异常
	if (SOCKET_ERROR == iErrCode)
		return WSA_GET_SOCKET_OPT_ERROR;

	// 将原先的缓冲区大小扩大十倍，系统默认为8192byte
	uiBuflen *= BUFFER_SIZE_TIMES;

	//设定缓冲区大小                                                           
	iErrCode = setsockopt(m_sktUDPSender, SOL_SOCKET, SO_RCVBUF, (char*)&uiBuflen, uiOptlen);

	//判断设置缓冲区大小是否异常
	if (SOCKET_ERROR == iErrCode)
		return WSA_SET_SOCKET_OPT_ERROR;

	//检查缓冲区大小是否修改成功
	unsigned int uiNewRcvBuf;
	iErrCode = getsockopt(m_sktUDPSender, SOL_SOCKET, SO_SNDBUF, (char*)&uiNewRcvBuf, (int*)&uiOptlen);
	if (SOCKET_ERROR == iErrCode || uiNewRcvBuf == uiBuflen)
		//CCsccLog::DebugPrint("修改系统发送数据缓冲区失败！\n");
		return WSA_GET_SOCKET_OPT_ERROR;

	//设置发送端端口重用
	BOOL bIsReusePort;
	int iReusePortLen = sizeof(bIsReusePort);
	iErrCode = setsockopt(m_sktUDPSender, SOL_SOCKET, SO_REUSEADDR, (char*)&bIsReusePort, iReusePortLen);
	if (SOCKET_ERROR == iErrCode)//CCsccLog::DebugPrint("设置端口重用失败！\n");
		return WSA_SET_SOCKET_OPT_ERROR;

	// 本地IP
	char pcLocalIp[IP_CHAR_LENGTH];
	ByteIpToStringIp(abyLocalIp, pcLocalIp);

	//绑定本地IP地址
	sockaddr_in sdClientAddr;

	//发送本机IP
	sdClientAddr.sin_addr.S_un.S_addr = inet_addr(pcLocalIp);	//TODO:此处是否需要在DLL内部自动获取本机的IP地址
	sdClientAddr.sin_family = AF_INET;							//本地地址族
	sdClientAddr.sin_port = htons(uiLocalPort);					//端口号,主机字节序转换为网络字节序，等待发送

	//绑定套接字d
	int iBindErr = bind(m_sktUDPSender, (LPSOCKADDR)&sdClientAddr, sizeof(sdClientAddr));
	if (iBindErr == SOCKET_ERROR)
		return WSA_BIND_ERROR;

	return TRUE;
}


/***************************************************************************
* 函数名称：   Receive
* 摘　　要：   
* 全局影响：   virtual public 
* 参　　数：   [inout]  byte *  & pbyBuffer
* 参　　数：   [inout]  size_t & uiDataLength
* 参　　数：   [inout]  size_t & uiFrameNum
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::Receive(byte* &pbyBuffer, size_t& uiDataLength, size_t& uiFrameNum)                 
{
	// init the data
	PST_PACKAGE pstReceivePacket = new ST_PACKAGE;
	sockaddr stReceiveAddr;
	int nSize = sizeof(stReceiveAddr);
	size_t packageNum = 0;
	memset(pbyBuffer, 0, uiDataLength);

	// get the data
	while (true)
	{
		int retVal = recvfrom(m_sktReceiveSender, (char*)pstReceivePacket, sizeof(ST_PACKAGE),
			0, (struct sockaddr*)(&stReceiveAddr), &nSize);
		if (retVal == SOCKET_ERROR)
		{
			closesocket(m_sktReceiveSender);
			WSACleanup();
			delete pstReceivePacket;
			return FALSE;
		}

		// 说明是第一个包
		if (0 == pstReceivePacket->uiPackageNum)
			packageNum = 0;

		// 查看输出的空间是否足够
		if (uiDataLength < pstReceivePacket->uiTotalDataLength)
		{
			if (NULL != pbyBuffer)
				delete[] pbyBuffer;
			pbyBuffer = new byte[pstReceivePacket->uiTotalDataLength];
			uiDataLength = pstReceivePacket->uiTotalDataLength;
			memset(pbyBuffer, 0, uiDataLength);
		}

		// 检查包号是否正确
		if (packageNum != pstReceivePacket->uiPackageNum)
		{
			if (NULL != pbyBuffer)
				delete[] pbyBuffer;
			pbyBuffer = NULL;
			uiDataLength = 0;
			delete pstReceivePacket;
			return FALSE;
		}

		// 组上剩余的包
		memcpy(pbyBuffer + packageNum*DATA_BUF_LENGTH, 
			pstReceivePacket->abyData, pstReceivePacket->uiDataLength);

		// 判断是否已经完全组完了
		if (DATA_BUF_LENGTH != pstReceivePacket->uiDataLength)
			break;

		packageNum++;
	}

	delete pstReceivePacket;
	return TRUE;
}


/***************************************************************************
* 函数名称：   ReleaseCom
* 摘　　要：   
* 全局影响：   virtual protected 
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::ReleaseCom()        // 完成释放代码
{

	if (m_sktUDPSender != INVALID_SOCKET)
		closesocket(m_sktUDPSender);

	if (m_sktReceiveSender != INVALID_SOCKET)
		closesocket(m_sktReceiveSender);

	int iUnInitialErr = 0;					// 函数返回值
	// 释放套接字库
	iUnInitialErr = WSACleanup();

	// 判断是否成功释放
	if (0 != iUnInitialErr)
	{
		// 判断错误代码是否是WSANOTINITIALISED
		if (WSANOTINITIALISED == WSAGetLastError())
			return false;

		return true;
	}
	else
		return false;
}


/***************************************************************************
* 函数名称：   AddToAccepter
* 摘　　要：   
* 全局影响：   virtual public 
* 参　　数：   [in]  BYTE abyServerIP[IP_BYTE_LENGTH]
* 参　　数：   [in]  UINT uiServerPort
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::AddToAccepter(BYTE abyServerIP[IP_BYTE_LENGTH], UINT uiServerPort)
{
	if (AddServerObj(abyServerIP, uiServerPort))
		return TRUE;
	else
		return FALSE;
}


/***************************************************************************
* 函数名称：   AddServerObj
* 摘　　要：   
* 全局影响：   protected 
* 参　　数：   [in]  BYTE abyServerIp[IP_BYTE_LENGTH]
* 参　　数：   [in]  UINT uiServerPort
* 返回值　：   BOOL
*
* 修改记录：
*  [日期]     [作者/修改者]  [修改原因]
*2017/01/04      饶智博        添加
***************************************************************************/
BOOL UDPCommunications::AddServerObj(BYTE abyServerIp[IP_BYTE_LENGTH], UINT uiServerPort)
{
	USES_CONVERSION;

	char pcServerIp[IP_CHAR_LENGTH];
	ByteIpToStringIp(abyServerIp, pcServerIp);

	if (m_sktReceiveSender != INVALID_SOCKET)
		closesocket(m_sktReceiveSender);
	//创建UDP通信套接字
	m_sktReceiveSender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (INVALID_SOCKET == m_sktReceiveSender)   // CCsccLog::DebugPrint("创建服务器端UDP通信套接字错误！\n");
		return FALSE;

	//定义错误代码
	int iErrCode;

	//待设置的套接字缓冲区大小
	UINT uiBuflen;
	//缓冲区大小
	UINT uiOptlen = sizeof(uiBuflen);

	//获取发送缓冲区参数
	iErrCode = getsockopt(m_sktReceiveSender,		//套接字
		SOL_SOCKET,			//套接字级别，此处选择套接字所在的应用层
		SO_SNDBUF,			//将要设置的套接字选项名称，此处为缓冲区大小
		(char *)&uiBuflen,	//设置的套接字选项值
		(int*)&uiOptlen);	//缓冲区大小

	//判断获取缓冲区参数是否异常
	if (SOCKET_ERROR == iErrCode)
		return FALSE;

	//将原先的缓冲区大小扩大十倍，系统默认为8192byte(8k)
	uiBuflen *= BUFFER_SIZE_TIMES;

	//设定缓冲区大小                                                           
	iErrCode = setsockopt(m_sktReceiveSender, SOL_SOCKET, SO_RCVBUF, (char *)&uiBuflen, uiOptlen);

	//判断设置缓冲区大小是否异常
	if (SOCKET_ERROR == iErrCode)
		return FALSE;

	// 检查缓冲区大小是否修改成功
	unsigned int uiNewRcvBuf;
	iErrCode = getsockopt(m_sktReceiveSender, SOL_SOCKET, SO_SNDBUF, (char*)&uiNewRcvBuf, (int*)&uiOptlen);
	if (SOCKET_ERROR == iErrCode || uiNewRcvBuf == uiBuflen) // CCsccLog::DebugPrint("修改系统发送数据缓冲区失败！\n");
		return FALSE;

	// 绑定服务器IP地址
	sockaddr_in sdServerAddr;									// 服务器端地址簇
	sdServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);	    // 发送本机IP
	sdServerAddr.sin_family = AF_INET;
	sdServerAddr.sin_port = htons(uiServerPort);				// 端口号,主机字节序三转换为网络字节序

	//绑定套接字
	int iBindErr = bind(m_sktReceiveSender, (LPSOCKADDR)&sdServerAddr, sizeof(sdServerAddr));
	if (iBindErr == SOCKET_ERROR)   // CCsccLog::DebugPrint("绑定接收套接字出错！\n");
		return FALSE;

	//SetServerObj(m_sktReceiveSender, abyServerIp, uiServerPort);			// 将UDP通信套接字加入到服务器端对象中
	return TRUE;
}
