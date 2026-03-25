XTP行情Quote-API使用示例说明
一. 行情库介绍
1. 头文件
2. 库文件
3. 接口说明
   二. Quick Start
1. 创建Api实例
2. 初始化Api参数
   （1）设定心跳超时时间
   （2）设定UDP缓存buffer
   （3）设定UDP异步输出日志
3. 创建Spi类
   （1）继承XTP::API::QuoteSpi，创建自己的Spi类MyQuoteSpi
   （2）重写回调函数逻辑
4. 创建Spi实例
5. 注册Spi
6. 登陆行情服务器
7. 查询行情静态信息
8. 订阅行情
9. 处理回调消息
10. 断线重连
    三. 行情订阅需注意的问题


一. 行情库介绍
该文档旨在帮助开发者快速使用极速交易平台XTP的行情API，文中是Quote-API接口调用示例。


1. 头文件

文件名	详情
xtp_quote_api.h	行情接口头文件，行情订阅接口类。
xtp_api_struct.h	业务数据结构。
xtp_api_data_type.h	交易和行情的数据基本类型。
xtp_api_struct_common.h	定义业务公共数据结构。
xoms_api_fund_struct.h	定义资金划拨数据结构。
xoms_api_struct.h	定义交易所需数据结构。
xquote_api_struct.h	定义行情所需数据结构。
xquote_api_rebuild_tbt_struct.h	定义行情回补所需数据结构。
2. 库文件

适用系统	文件名
windows	xtpquoteapi.dll  xtpquoteapi.lib
linux	libxtpquoteapi.so
macos	libxtpquoteapi.dylib

3. 接口说明
   (1) 行情API提供了两个接口类：行情订阅类QuoteApi接口和行情回调类QuoteSpi接口。
   (2) 行情通知是通过异步方式提供。
   (3) 客户端应用程序可通过QuoteApi发出订阅行情请求，通过继承QuoteSpi并重写回调函数来响应后台服务，处理行情数据。
   (4) XTP的库文件目前只支持64位的，注意创建64位的工程，并且设置64位的编译器来进行编译。

二. Quick Start
1. 创建Api实例
   示例代码如下：

复制
```
XTP::API::QuoteApi* m_pQuoteApi = NULL;
uint8_t client_id = 1;//一个进程一个client id，可在[1, 99]区间内任选，并固定下来
std::string save_file_path = "./";//保存xtp api日志的路径，需要有可读写权限
XTP_LOG_LEVEL log_level = XTP_LOG_LEVEL_DEBUG;//xtp api日志的输出级别，建议调试时使用debug级别，正常运行时使用info级别
///创建QuoteApi
m_pQuoteApi = XTP::API::QuoteApi::CreateQuoteApi(client_id, save_file_path.c_str(), log_level);
if (NULL == m_pQuoteApi)
{
//创建API失败
}
```
2. 初始化Api参数
   （1）设定心跳超时时间
   示例代码如下：

复制
```
///设定行情服务器超时时间，单位为秒，默认是15s，调试时可以设定大点
uint32_t heat_beat_interval = 15;
m_pQuoteApi->SetHeartBeatInterval(heat_beat_interval);
```
（2）设定UDP缓存buffer
如果行情服务器是仅支持TCP连接的服务器，此步骤可以跳过。

示例代码如下：

复制
```
//设定UDP本地缓存buffer大小，单位为MB
int buffer_size = 512;//2.2.30.7以上版本api，建议不超过512，最大仅支持1024
m_pQuoteApi->SetUDPBufferSize(buffer_size);
```
（3）设定UDP异步输出日志
如果行情服务器是仅支持TCP连接的服务器，此步骤可以跳过。

示例代码如下：

复制
```
//设定是否输出异步日志
bool  log_output_flag = true;//刚实盘运行时，或者调试测试时，建议开启，实盘运行正常后，可以关闭
m_pQuoteApi->SetUDPSeqLogOutPutFlag(log_output_flag);
```
3. 创建Spi类
   如果想要获取行情数据，必须得有自己的回调响应类。下面仅以快照行情为例，创建了一个Spi回调响应类MyQuoteSpi，具体步骤如下：

（1）继承XTP::API::QuoteSpi，创建自己的Spi类MyQuoteSpi
quote_spi.h文件,行情回调类接口定义：

复制
```
#include "xtp_quote_api.h"

using namespace XTP::API;

class MyQuoteSpi : public QuoteSpi
{
public:
MyQuoteSpi();
~MyQuoteSpi();

	///行情服务器断线通知
	void OnDisconnected(int reason);

	///订阅快照行情应答
	void OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last);
	
	///订阅快照行情应答
	void OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last);
	
	///查询合约完整静态信息的应答
	void OnQueryAllTickersFullInfo(XTPQFI* ticker_info, XTPRI *error_info, bool is_last);

    ///快照行情通知，包含买一卖一队列
    void OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count, int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count);

	/// 快照行情中ETF的IOPV通知
	void OnETFIOPVData(IOPV *iopv);

	//TODO:如果需要订阅其他行情，请继续重写其他行情回调函数

};
```
（2）重写回调函数逻辑
quote_spi.cpp文件，行情回调类接口实现：

复制
```
#include "quote_spi.h"
#include <iostream>

using namespace std;

extern XTP::API::QuoteApi* m_pQuoteApi;

MyQuoteSpi::MyQuoteSpi()
{
}

MyQuoteSpi::~MyQuoteSpi()
{
}

void MyQuoteSpi::OnDisconnected(int reason)
{
//行情服务器断线后，此函数会被调用
//TODO:重新login，并在login成功后，再次订阅
}

void MyQuoteSpi::OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//订阅失败
cout << "OnSubMarketData -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//订阅成功
	//TODO:当is_last == true时，触发其他用户逻辑	
}

void MyQuoteSpi::OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//取消订阅失败
cout << "OnUnSubMarketData -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//退阅成功
	//TODO:当is_last == true时，触发其他用户逻辑	
}

void MyQuoteSpi::OnQueryAllTickersFullInfo(XTPQFI* ticker_info, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//查询失败
cout << "OnQueryAllTickersFullInfo -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//查询成功
	//TODO:将查询结果缓存
	
	//如果是最后一个响应，通知后续逻辑
	if (is_last == true)
	{		
		//TODO:通知后续逻辑，例如可以在查询沪市结束后，再次发起查询深市的查询请求
		if (ticker_info->exchange_id == XTP_EXCHANGE_SH)
		{
			//当沪市的静态行情查询完毕后，查询深市行情静态信息
			m_pQuoteApi->QueryAllTickersFullInfo(XTP_EXCHANGE_SZ);
		}
		else
		{
			//TODO:触发后续逻辑
		}
	}
}

void MyQuoteSpi::OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count, int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count)
{
//收到行情快照数据
//TODO:用户处理逻辑，注意此处不能仅仅保存数据的指针，指针所指向的内存数据将在此函数return后失效
}

void  MyQuoteSpi::OnETFIOPVData(IOPV *iopv)
{
//收到行情快照中ETF的IOPV信息
//TODO:用户处理逻辑
}
```
4. 创建Spi实例
   示例代码如下：

复制
```
//创建Spi实例
MyQuoteSpi* m_pQuoteSpi = new MyQuoteSpi();
```
5. 注册Spi
   示例代码如下：

复制
```
//注册Spi
m_pQuoteApi->RegisterSpi(m_pQuoteSpi);
```
6. 登陆行情服务器
   示例代码如下：

复制
```
std::string quote_server_ip = "xxx.xxx.xxx.xxx";//行情服务器ip地址
int quote_server_port = xxx;//行情服务器端口port
std::string quote_username = "xxxxxxxx";//行情服务器的登陆账户名
std::string quote_password = "xxxxxx";//行情服务器的登陆密码
XTP_PROTOCOL_TYPE protocol_type = XTP_PROTOCOL_TCP;//Level1服务器通常使用TCP，具体以运营通知为准，Level2服务器请用UDP，公网测试环境均为TCP，以实际服务器支持的类型为准
std::string local_ip = "xxx.xxx.xxx.xxx";//本地网卡对应的ip

	int ret = m_pQuoteApi->Login(quote_server_ip.c_str(), quote_server_port, quote_username.c_str(), quote_password.c_str(), protocol_type, local_ip.c_str());
	if (0 != ret)
	{
		// 登录失败，获取错误信息
		XTPRI* error_info = m_pQuoteApi->GetApiLastError();
		std::cout << "login to server error, " << error_info->error_id << " : " << error_info->error_msg << std::endl;

		return 0;
	}

	// 登录成功
	//TODO: 用户逻辑，例如查询静态数据、订阅行情等
```
7. 查询行情静态信息
   一次只能查单个市场，如果需要查沪深2市场，需要分开2次调用。
   请在一个市场查询结果全部回来后再发起另一个市场的查询，否则容易引起断线。
   示例代码如下：

复制
```
//查询沪市行情静态信息
m_pQuoteApi->QueryAllTickersFullInfo(XTP_EXCHANGE_SH);
```
8. 订阅行情
   采用TCP方式连接的行情服务器,推荐使用SubscribeMarketData()单订阅行情。
   采用UDP方式连接的行情服务器，推荐使用全订阅函数进行全市场订阅。
   下面以单订阅2只股票为例,示例代码如下：
   复制
```
   // 申请内存
   int ticker_count = 2;//需要订阅行情的证券代码数量，可根据实际订阅需求改动
   char **ppInstrumentID = new char*[ticker_count];
   for (int i = 0; i < ticker_count; i++)
   {
   ppInstrumentID[i] = new char[XTP_TICKER_LEN];
   }
   strcpy_s(ppInstrumentID[0], XTP_TICKER_LEN, "600000");
   strcpy_s(ppInstrumentID[1], XTP_TICKER_LEN, "600001");
   //订阅沪市的600000、600001两只股票快照行情
   m_pQuoteApi->SubscribeMarketData(ppInstrumentID, ticker_count, XTP_EXCHANGE_SH);

   // 释放内存
   for (int i = 0; i < ticker_count; i++) {
   delete[] ppInstrumentID[i];
   ppInstrumentID[i] = NULL;
   }
   delete[] ppInstrumentID;
   ppInstrumentID = NULL;
```

9. 处理回调消息
   发起快照订阅请求后，无论是否订阅成功，Spi都将收到快照订阅响应。可根据需要重写 OnSubMarketData() 函数逻辑。
   如果快照订阅成功，快照行情将通过 OnDepthMarketData() 回调函数推送给用户。此时用户可根据收到的行情数据重写程序逻辑。
   如果快照订阅种类为ETF，ETF的IOPV信息可以通过 OnETFIOPVData() 回调函数推送给用户。此时用户可根据收到的IOPV数据重写程序逻辑。
   其余逐笔和订单簿行情，可参照快照行情进行订阅和重写函数逻辑。
10. 断线重连
    重写 OnDisconnected() 回调函数，总体思路是调用 Login() 重连成功后，重新订阅快照行情。示例代码如下：

复制
```
void MyQuoteSpi::OnDisconnected(int reason)
{
//行情服务器断线后，此函数会被调用
//TODO:重新login，并在login成功后，再次订阅

	cout << "Disconnect from quote server. " << endl;

	//重新登陆行情服务器
	int ret = -1;
	while (0 != ret)
	{
		ret = m_pQuoteApi->Login(quote_server_ip.c_str(), quote_server_port, quote_username.c_str(), quote_password.c_str(), protocol_type);
		if (0 != ret)
		{
			// 登录失败，获取错误信息
			XTPRI* error_info = m_pQuoteApi->GetApiLastError();
			cout << "login to server error, " << error_info->error_id << " : " << error_info->error_msg << endl;

			//等待10s以后再次连接，可修改此等待时间，建议不要小于3s
#ifdef _WIN32
Sleep(10000);
#else
sleep(10);
#endif // _WIN32
}
}

	//重连成功
	cout << "login to server success. " << endl;
	//再次订阅行情快照
	subscribeMarketData();

}
```
三. 行情订阅需注意的问题
(1) Level1行情服务器通常仅支持TCP连接，请使用 TCP 连接，具体以哪种方式连接请以运营通知为准。Level2行情服务器，请使用 UDP 连接。公网测试环境均使用 TCP 连接。
(2) 登录时， local_ip 可以是NULL, 但不能是空串，最好传入要使用的网卡上的ip。
(3) 证券代码基本信息可通过 QueryAllTickersFullInfo() 查询合约的完整静态信息，并在本地建立一个证券信息索引表。为了避免因缓慢满而导致的断线，沪深交易所请分开查询，且在一个市场查询完成后，再发起查询另一个市场信息的查询请求。
(4) 在行情回调函数中，请尽快处理行情数据并返回，否则可能会因为接收缓存满而导致断线。
(5) 如果连接的是UDP行情服务器，无论是否订阅，都是行情全接收后再本地Api筛选过滤。
(6) 使用UDP行情服务器时，如果本地缓存满了，会引发丢包。请务必在Login之前调用 SetUDPBufferSize() 设置，推荐 256MB 或 512MB 。
(7) 使用订阅/退订函数，请注意 配对 使用。不支持全订阅后，再退订部分，或者订阅部分再全退订。
四. 简单Demo示例代码
下面是一个简单的代码示例，演示了行情API通过TCP方式连接Level1行情服务器的过程，包括：创建、初始化、登录行情、查询行情静态信息、订阅行情、断线重连。
main.cpp文件

复制
```
#include "xtp_quote_api.h"
#include <string>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // _WIN32
#include "quote_spi.h"

XTP::API::QuoteApi* m_pQuoteApi = NULL;//全局变量，程序共用一个api
std::string quote_server_ip = "xxx.xxx.xxx.xxx";//行情服务器ip地址
int quote_server_port = xxx;//行情服务器端口port
std::string quote_username = "xxxxxxxx";//行情服务器的登陆账户名
std::string quote_password = "xxxxxx";//行情服务器的登陆密码
XTP_PROTOCOL_TYPE protocol_type = XTP_PROTOCOL_TCP;//Level1服务器通常使用TCP，Level2服务器请用UDP，公网测试环境均为TCP，具体以运营通知为准
std::string local_ip = "xxx.xxx.xxx.xxx";//本地网卡对应的ip

int main()
{
uint8_t client_id = 1;//一个进程一个client id，可在[1, 99]区间内任选，并固定下来
std::string save_file_path = "./";//保存xtp api日志的路径，需要有可读写权限
XTP_LOG_LEVEL log_level = XTP_LOG_LEVEL_DEBUG;//xtp api日志的输出级别，建议调试时使用debug级别，正常运行时使用info级别
///创建QuoteApi
m_pQuoteApi = XTP::API::QuoteApi::CreateQuoteApi(client_id, save_file_path.c_str(), log_level);
if (NULL == m_pQuoteApi)
{
//创建API失败
return 0;
}

	///设定行情服务器超时时间，单位为秒，默认是15s，调试时可以设定大点
	uint32_t heat_beat_interval = 15;
	m_pQuoteApi->SetHeartBeatInterval(heat_beat_interval);

	//以下代码段如果是TCP连接的话,需要跳过
	//设定UDP本地缓存buffer大小，单位为MB
	//int buffer_size = 512;//2.2.30.7以上版本api，建议不超过512，最大仅支持1024
	//m_pQuoteApi->SetUDPBufferSize(buffer_size);
	//设定是否输出异步日志
	//bool  log_output_flag = true;//刚实盘运行时，或者调试测试时，建议开启，实盘运行正常后，可以关闭
	//m_pQuoteApi->SetUDPSeqLogOutPutFlag(log_output_flag);

	//创建Spi实例
	MyQuoteSpi* m_pQuoteSpi = new MyQuoteSpi();
	if (NULL == m_pQuoteSpi)
	{
		//创建行情Spi失败
		return 0;
	}

	//注册Spi
	m_pQuoteApi->RegisterSpi(m_pQuoteSpi);

	//登陆行情服务器
	int ret = m_pQuoteApi->Login(quote_server_ip.c_str(), quote_server_port, quote_username.c_str(), quote_password.c_str(), protocol_type, local_ip.c_str());
	if (0 != ret)
	{
		// 登录失败，获取错误信息
		XTPRI* error_info = m_pQuoteApi->GetApiLastError();
		std::cout << "login to server error, " << error_info->error_id << " : " << error_info->error_msg << std::endl;

		return 0;
	}

	// 登录成功
	//TODO: 用户逻辑，例如查询静态数据、订阅行情等，以下以查询沪市静态信息为例

	//查询沪市行情静态信息
	m_pQuoteApi->QueryAllTickersFullInfo(XTP_EXCHANGE_SH);

	//保持主线程，防止程序退出
	while (true)
	{
#ifdef _WIN32
Sleep(10000);
#else
sleep(10);
#endif // _WIN32		
}


	return 0;
}
```
MyQuoteSpi类相关定义和实现文件。 quote_spi.h文件

复制
```
#include "xtp_quote_api.h"

using namespace XTP::API;

class MyQuoteSpi : public QuoteSpi
{
public:
MyQuoteSpi();
~MyQuoteSpi();

	///行情服务器断线通知
	void OnDisconnected(int reason);

	///订阅快照行情应答
	void OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last);

	///订阅快照行情应答
	void OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last);

	///查询合约完整静态信息的应答
	void OnQueryAllTickersFullInfo(XTPQFI* ticker_info, XTPRI *error_info, bool is_last);

	///快照行情通知，包含买一卖一队列
	void OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count, int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count);

	//TODO:如果需要订阅其他行情，请继续重写其他行情回调函数

private:
void subscribeMarketData();//订阅行情

};
```
quote_spi.cpp文件

复制
```
#include "quote_spi.h"
#include <string>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // _WIN32

using namespace std;

extern XTP::API::QuoteApi* m_pQuoteApi;
extern std::string quote_server_ip;//行情服务器ip地址
extern int quote_server_port;//行情服务器端口port
extern std::string quote_username;//行情服务器的登陆账户名
extern std::string quote_password;//行情服务器的登陆密码
extern XTP_PROTOCOL_TYPE protocol_type;//Level1服务器使用TCP，Level2服务器请用UDP，公网测试环境均为TCP，具体以运营通知为准
extern std::string local_ip;//本地网卡对应的ip

MyQuoteSpi::MyQuoteSpi()
{
}

MyQuoteSpi::~MyQuoteSpi()
{
}

void MyQuoteSpi::OnDisconnected(int reason)
{
//行情服务器断线后，此函数会被调用
//TODO:重新login，并在login成功后，再次订阅

	cout << "Disconnect from quote server. " << endl;

	//重新登陆行情服务器
	int ret = -1;
	while (0 != ret)
	{
		ret = m_pQuoteApi->Login(quote_server_ip.c_str(), quote_server_port, quote_username.c_str(), quote_password.c_str(), protocol_type, local_ip.c_str());
		if (0 != ret)
		{
			// 登录失败，获取错误信息
			XTPRI* error_info = m_pQuoteApi->GetApiLastError();
			cout << "login to server error, " << error_info->error_id << " : " << error_info->error_msg << endl;

			//等待10s以后再次连接，可修改此等待时间，建议不要小于3s
#ifdef _WIN32
Sleep(10000);
#else
sleep(10);
#endif // _WIN32
}
}

	//重连成功
	cout << "login to server success. " << endl;
	//再次订阅行情快照
	subscribeMarketData();

}

void MyQuoteSpi::OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//订阅失败
cout << "OnSubMarketData -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//订阅成功
	//TODO:当is_last == true时，触发其他用户逻辑	
}

void MyQuoteSpi::OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//取消订阅失败
cout << "OnUnSubMarketData -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//退阅成功
	//TODO:当is_last == true时，触发其他用户逻辑	
}

void MyQuoteSpi::OnQueryAllTickersFullInfo(XTPQFI* ticker_info, XTPRI *error_info, bool is_last)
{
if (error_info && error_info->error_id != 0)
{
//查询失败
cout << "OnQueryAllTickersFullInfo -----" << "error_id = " << error_info->error_id << ", error_msg = " << error_info->error_msg << endl;
return;
}

	//查询成功
	//TODO:将查询结果缓存

	//如果是最后一个响应，通知后续逻辑
	if (is_last == true)
	{
		//TODO:通知后续逻辑，例如可以在查询沪市结束后，再次发起查询深市的查询请求
		if (ticker_info->exchange_id == XTP_EXCHANGE_SH)
		{
			//当沪市的静态行情查询完毕后，查询深市行情静态信息
			m_pQuoteApi->QueryAllTickersFullInfo(XTP_EXCHANGE_SZ);
		}
		else
		{
			//TODO:触发后续逻辑，例如可以订阅快照行情
			subscribeMarketData();
		}
	}
}

void MyQuoteSpi::OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count, int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count)
{
//收到行情快照数据
//TODO:用户处理逻辑，注意此处不能仅仅保存数据的指针，指针所指向的内存数据将在此函数return后失效
}

void MyQuoteSpi::subscribeMarketData()
{
// 申请内存，初始化订阅参数
int ticker_count = 2;//需要订阅行情的证券代码数量，可根据实际订阅需求改动
char **ppInstrumentID = new char*[ticker_count];
for (int i = 0; i < ticker_count; i++)
{
ppInstrumentID[i] = new char[XTP_TICKER_LEN];
}
strcpy_s(ppInstrumentID[0], XTP_TICKER_LEN, "600000");
strcpy_s(ppInstrumentID[1], XTP_TICKER_LEN, "600001");
//订阅沪市的600000、600001两只股票快照行情
m_pQuoteApi->SubscribeMarketData(ppInstrumentID, ticker_count, XTP_EXCHANGE_SH);

	// 释放内存
	for (int i = 0; i < ticker_count; i++) {
		delete[] ppInstrumentID[i];
		ppInstrumentID[i] = NULL;
	}
	delete[] ppInstrumentID;
	ppInstrumentID = NULL;
}
```