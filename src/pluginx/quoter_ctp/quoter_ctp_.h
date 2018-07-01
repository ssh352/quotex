/*
* Copyright (c) 2018-2018 the QuoteX authors
* All rights reserved.
*
* The project sponsor and lead author is Xu Rendong.
* E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
* You can get more information at https://xurendong.github.io
* For names of other contributors see the contributors file.
*
* Commercial use of this code in source and binary forms is
* governed by a LGPL v3 license. You may get a copy from the
* root directory. Or else you should get a specific written
* permission from the project author.
*
* Individual and educational use of this code in source and
* binary forms is governed by a 3-clause BSD license. You may
* get a copy from the root directory. Certainly welcome you
* to contribute code of all sorts.
*
* Be sure to retain the above copyright notice and conditions.
*/

#ifndef QUOTER_CTP_QUOTER_CTP_P_H
#define QUOTER_CTP_QUOTER_CTP_P_H

#include "struct_ctp.h"
#include "quoter_ctp.h"

typedef boost::shared_ptr<basicx::NetServer> net_server_ptr;

class basicx::SysLog_S;
class basicx::SysCfg_S;
class basicx::SysRtm_S;
class basicx::SysDBI_S;
class basicx::Plugins;
class CThostFtdcMdSpiImpl;

class QuoterCTP_P : public basicx::NetServer_X
{
public:
	QuoterCTP_P();
	~QuoterCTP_P();

public:
	void SetGlobalPath();
	bool ReadConfig( std::string file_path );
	void LogPrint( basicx::syslog_level log_level, std::string& log_cate, std::string& log_info, int32_t log_show = 0 );

public:
	bool Initialize();
	bool InitializeExt();
	bool StartPlugin();
	bool IsPluginRun();
	bool StopPlugin();
	bool UninitializeExt();
	bool Uninitialize();
	bool AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data );

	void CreateService();
	void HandleTaskMsg();

public:
	Config m_configs;
	std::string m_location;
	std::string m_info_file_path;
	std::string m_cfg_file_path;

	service_ptr m_service;
	bool m_service_running;
	thread_ptr m_thread_service;
	int32_t m_work_thread_number;
	std::vector<thread_ptr> m_vec_work_thread;

	int32_t m_task_id;
	boost::mutex m_task_list_lock;
	std::list<TaskItem> m_list_task;

	bool m_plugin_running;

	std::string m_log_cate;
	basicx::SysLog_S* m_syslog;
	basicx::SysCfg_S* m_syscfg;
	basicx::SysRtm_S* m_sysrtm;
	basicx::SysDBI_S* m_sysdbi_s;
	basicx::Plugins* m_plugins;

// �Զ����Ա�����ͱ���
public:
	bool CheckSingleMutex(); // �������Ƽ��
	void OnTimer();
	void DoReSubscribe();
	bool CreateDumpFolder();
	void InitQuoteDataFile();
	void InitApiSpi();
	void GetSubListFromDB();
	void GetSubListFromCTP();

	void MS_AddData_SnapshotFuture( SnapshotFuture& snapshot_future_temp );

	void DumpSnapshotFuture();

	void StartNetServer();
	void OnNetServerInfo( basicx::NetServerInfo& net_server_info_temp ) override;
	void OnNetServerData( basicx::NetServerData& net_server_data_temp ) override;
	int32_t gzCompress( Bytef* data_in, uLong size_in, Bytef* data_out, uLong* size_out, int32_t level );

	void HandleUserRequest();
	std::string OnUserAddSub( Request* request );
	std::string OnUserDelSub( Request* request );
	void AddConSubAll( ConSubMan& csm_con_sub_man, int32_t identity, basicx::ConnectInfo* connect_info );
	void DelConSubAll( ConSubMan& csm_con_sub_man, int32_t identity );
	bool IsConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ); // �Ƿ���ȫ�г�����
	void AddConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity, basicx::ConnectInfo* connect_info );
	void DelConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity );
	void ClearConSubOne( ConSubMan& csm_con_sub_man, int32_t identity );
	void ClearUnavailableConSub( ConSubMan& csm_con_sub_man, bool is_idle_time ); // һ���ڿ���ʱ����
	void CommitResult( int32_t task_id, int32_t identity, int32_t code, std::string& data );
	std::string OnErrorResult( int32_t ret_func, int32_t ret_code, std::string ret_info, int32_t encode );

public:
	HANDLE m_single_mutex; // �������Ƽ��
	CThostFtdcMdApi* m_user_api;
	CThostFtdcMdSpiImpl* m_user_spi;
	thread_ptr m_thread_on_timer;
	thread_ptr m_thread_init_api_spi;
	bool m_subscribe_ok;
	std::vector<std::string> m_vec_contract;
	int32_t m_subscribe_count;
	
	QuoteCache<SnapshotFuture> m_cache_snapshot_future;

	int32_t m_data_encode;
	int32_t m_data_compress;
	net_server_ptr m_net_server_broad;
	net_server_ptr m_net_server_query;
	uint32_t m_output_buf_len_snapshot_future;
	unsigned char* m_output_buf_snapshot_future;

	thread_ptr m_thread_user_request;
	boost::mutex m_user_request_list_lock;
	std::list<Request> m_list_user_request;

	ConSubMan m_csm_snapshot_future;

	Json::Reader m_json_reader;
	Json::FastWriter m_json_writer;
};

class CThostFtdcMdSpiImpl : public CThostFtdcMdSpi
{
public:
	CThostFtdcMdSpiImpl( CThostFtdcMdApi* user_api, QuoterCTP_P* quoter_ctp_p );
	~CThostFtdcMdSpiImpl();

public:
	void OnFrontConnected() override;
	void OnFrontDisconnected( int nReason ) override;
	void OnHeartBeatWarning( int nTimeLapse ) override;
	
	void OnRspUserLogin( CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	void OnRspUserLogout( CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;

	void OnRspError( CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	void OnRspSubMarketData( CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	void OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	void OnRtnDepthMarketData( CThostFtdcDepthMarketDataField* pDepthMarketData ) override;

private:
	std::string m_log_cate;
	CThostFtdcMdApi* m_user_api;
	QuoterCTP_P* m_quoter_ctp_p;
};

class CThostFtdcTraderSpiImpl : public CThostFtdcTraderSpi
{
public:
	CThostFtdcTraderSpiImpl( CThostFtdcTraderApi* user_api, QuoterCTP_P* quoter_ctp_p );
	~CThostFtdcTraderSpiImpl();

public:
	// ���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
	void OnFrontConnected() override;
	// ���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
	void OnFrontDisconnected( int nReason ) override;
	// ������ʱ���档����ʱ��δ�յ�����ʱ���÷��������á�
	void OnHeartBeatWarning( int nTimeLapse ) override;

	// �û���¼����
	int32_t ReqUserLogin( std::string broker_id, std::string user_id, std::string password );
	// ��¼������Ӧ��
	void OnRspUserLogin( CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	// �û��ǳ�����
	int32_t ReqUserLogout( std::string broker_id, std::string user_id );
	// �ǳ�������Ӧ��
	void OnRspUserLogout( CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;
	// �����ѯ��Լ��
	int32_t ReqQryInstrument();
	// �����ѯ��Լ��Ӧ��
	void OnRspQryInstrument( CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) override;

	int32_t GetRequestID();
	std::string GetLastErrorMsg(); // ���ú� m_last_error_msg ��������

public:
	bool m_last_rsp_is_error; // ��������첽����
	std::string m_last_error_msg; // �������������Ϣ
	int32_t m_request_id; // �����ʶ���
	bool m_connect_ok; // �����ѳɹ�
	bool m_login_ok; // ��¼�ѳɹ�
	bool m_qry_instrument; // ��Լ��ѯ�ѳɹ�
	bool m_logout_ok; // �ǳ��ѳɹ�
	int32_t m_front_id; // ǰ�ñ��
	int32_t m_session_id; // �Ự���

private:
	std::string m_log_cate;
	CThostFtdcTraderApi* m_user_api;
	QuoterCTP_P* m_quoter_ctp_p;
};

#endif // QUOTER_CTP_QUOTER_CTP_P_H
