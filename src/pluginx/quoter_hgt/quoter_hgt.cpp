/*
* Copyright (c) 2018-2018 the QuoteX authors
* All rights reserved.
*
* The project sponsor and lead author is Xu Rendong.
* E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
* See the contributors file for names of other contributors.
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

#include <fstream>
#include <share.h> // _fsopen
#include <algorithm> // transform

#include <common/winver.h>
#include <common/common.h>
#include <syslog/syslog.h>
#include <syscfg/syscfg.h>
#include <sysrtm/sysrtm.h>
#include <plugins/plugins.h>

#include "define_hgt.h"
#include "quoter_hgt.h"
#include "quoter_hgt_.h"

#define FILE_LOG_ONLY 1

QuoterHGT the_app; // 必须

// 预设 QuoterHGT_P 函数实现

QuoterHGT_P::QuoterHGT_P()
	: NetServer_X()
	, m_location( "" )
	, m_info_file_path( "" )
	, m_cfg_file_path( "" )
	, m_service_running( false )
	, m_work_thread_number( CFG_WORK_THREAD_NUM )
	, m_plugin_running( false )
	, m_task_id( 0 )
	, m_local_date( 0 )
	, m_market_data_cache( false )
	, m_market_data_time( 0 )
	, m_market_data_cache_size( 0 )
	, m_market_data_file_path( "" )
	, m_log_cate( "<Quoter_HGT>" ) {
	m_json_reader_builder["collectComments"] = false;
	m_json_reader = m_json_reader_builder.newCharReader();
	m_syslog = basicx::SysLog_S::GetInstance();
	m_syscfg = basicx::SysCfg_S::GetInstance();
	m_sysrtm = basicx::SysRtm_S::GetInstance();
	m_plugins = basicx::Plugins::GetInstance();
}

QuoterHGT_P::~QuoterHGT_P() {
	if( m_output_buf_snapshot_stock != nullptr ) {
		delete[] m_output_buf_snapshot_stock;
		m_output_buf_snapshot_stock = nullptr;
	}

	for( auto it = m_csm_snapshot_stock.m_map_con_sub_one.begin(); it != m_csm_snapshot_stock.m_map_con_sub_one.end(); it++ ) {
		if( it->second != nullptr ) {
			delete it->second;
		}
	}
	m_csm_snapshot_stock.m_map_con_sub_one.clear();

	for( auto it = m_csm_snapshot_stock.m_list_one_sub_con_del.begin(); it != m_csm_snapshot_stock.m_list_one_sub_con_del.end(); it++ ) {
		if( (*it) != nullptr ) {
			delete (*it);
		}
	}
	m_csm_snapshot_stock.m_list_one_sub_con_del.clear();

	if( m_market_data_cache != nullptr ) {
		delete[] m_market_data_cache;
		m_market_data_cache = nullptr;
	}
}

void QuoterHGT_P::SetGlobalPath() {
	m_location = m_plugins->GetPluginLocationByName( PLUGIN_NAME );
	m_cfg_file_path = m_plugins->GetPluginCfgFilePathByName( PLUGIN_NAME );
	m_info_file_path = m_plugins->GetPluginInfoFilePathByName( PLUGIN_NAME );
}

bool QuoterHGT_P::ReadConfig( std::string file_path ) {
	std::string log_info;

	pugi::xml_document document;
	if( !document.load_file( file_path.c_str() ) ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "打开插件参数配置信息文件失败！{0}", file_path );
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}

	pugi::xml_node node_plugin = document.document_element();
	if( node_plugin.empty() ) {
		log_info = "获取插件参数配置信息 根节点 失败！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}
	
	m_configs.m_market_data_folder = node_plugin.child_value( "MarketDataFolder" );
	m_market_data_file_path = m_configs.m_market_data_folder + "\\mktdt04.txt";

	m_configs.m_need_dump = atoi( node_plugin.child_value( "NeedDump" ) );
	m_configs.m_dump_path = node_plugin.child_value( "DumpPath" );
	m_configs.m_data_compress = atoi( node_plugin.child_value( "DataCompress" ) );
	m_configs.m_data_encode = atoi( node_plugin.child_value( "DataEncode" ) );
	m_configs.m_dump_time = atoi( node_plugin.child_value( "DumpTime" ) );
	m_configs.m_init_time = atoi( node_plugin.child_value( "InitTime" ) );

	m_configs.m_source_time_start = atoi( node_plugin.child_value( "SourceTimeStart" ) );
	m_configs.m_source_time_stop = atoi( node_plugin.child_value( "SourceTimeStop" ) );

	//FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} {1} {2} {3} {4} {5} {6}", m_configs.m_market_data_folder, 
	//	m_configs.m_need_dump, m_configs.m_dump_path, m_configs.m_data_compress, m_configs.m_data_encode, m_configs.m_dump_time, m_configs.m_init_time );
	//LogPrint( basicx::syslog_level::c_debug, log_info );

	log_info = "插件参数配置信息读取完成。";
	LogPrint( basicx::syslog_level::c_info, log_info );

	return true;
}

void QuoterHGT_P::LogPrint( basicx::syslog_level log_level, std::string& log_info, int32_t log_show/* = 0*/ ) {
	if( 0 == log_show ) {
		m_syslog->LogPrint( log_level, m_log_cate, "LOG>: HGT " + log_info ); // 控制台
		m_sysrtm->LogTrans( log_level, m_log_cate, log_info ); // 远程监控
	}
	m_syslog->LogWrite( log_level, m_log_cate, log_info );
}

bool QuoterHGT_P::Initialize() {
	SetGlobalPath();
	ReadConfig( m_cfg_file_path );

	m_thread_service = boost::make_shared<boost::thread>( boost::bind( &QuoterHGT_P::CreateService, this ) );
	m_thread_user_request = boost::make_shared<boost::thread>( boost::bind( &QuoterHGT_P::HandleUserRequest, this ) );

	return true;
}

bool QuoterHGT_P::InitializeExt() {
	return true;
}

bool QuoterHGT_P::StartPlugin() {
	std::string log_info;

	try {
		log_info = "开始启用插件 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		if( CheckSingleMutex() == false ) { // 单例限制检测
		    return false;
		}

		StartNetServer();
		m_thread_on_timer = boost::make_shared<boost::thread>( boost::bind( &QuoterHGT_P::OnTimer, this ) );

		// TODO：添加更多初始化任务

		log_info = "插件启用完成。";
		LogPrint( basicx::syslog_level::c_info, log_info );

		m_plugin_running = true; // 需要在创建线程前赋值为真

		return true;
	} // try
	catch( ... ) {
		log_info = "插件启用时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

bool QuoterHGT_P::IsPluginRun() {
	return m_plugin_running;
}

bool QuoterHGT_P::StopPlugin() {
	std::string log_info;

	try {
		log_info = "开始停止插件 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		// TODO：添加更多反初始化任务

		log_info = "插件停止完成。";
		LogPrint( basicx::syslog_level::c_info, log_info );

		m_plugin_running = false;

		return true;
	} // try
	catch( ... ) {
		log_info = "插件停止时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

bool QuoterHGT_P::UninitializeExt() {
	return true;
}

bool QuoterHGT_P::Uninitialize() {
	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}
	
	return true;
}

bool QuoterHGT_P::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	try {
		m_task_list_lock.lock();
		bool write_in_progress = !m_list_task.empty();
		TaskItem task_item_temp;
		m_list_task.push_back( task_item_temp );
		TaskItem& task_item_ref = m_list_task.back();
		task_item_ref.m_task_id = task_id;
		task_item_ref.m_identity = identity;
		task_item_ref.m_code = code;
		task_item_ref.m_data = data;
		m_task_list_lock.unlock();
		
		if( !write_in_progress && true == m_service_running ) {
			m_service->post( boost::bind( &QuoterHGT_P::HandleTaskMsg, this ) );
		}

		return true;
	}
	catch( std::exception& ex ) {
		std::string log_info;
		FormatLibrary::StandardLibrary::FormatTo( log_info, "添加 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

void QuoterHGT_P::CreateService() {
	std::string log_info;

	log_info = "创建输入输出服务线程完成, 开始进行输入输出服务 ...";
	LogPrint( basicx::syslog_level::c_info, log_info );

	try {
		try {
			m_service = boost::make_shared<boost::asio::io_service>();
			boost::asio::io_service::work work( *m_service );

			for( size_t i = 0; i < (size_t)m_work_thread_number; i++ ) {
				thread_ptr thread_service( new boost::thread( boost::bind( &boost::asio::io_service::run, m_service ) ) );
				m_vec_work_thread.push_back( thread_service );
			}

			m_service_running = true;

			for( size_t i = 0; i < m_vec_work_thread.size(); i++ ) { // 等待所有线程退出
				m_vec_work_thread[i]->join();
			}
		}
		catch( std::exception& ex ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "输入输出服务 初始化 异常：{0}", ex.what() );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	} // try
	catch( ... ) {
		log_info = "输入输出服务线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}

	log_info = "输入输出服务线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

void QuoterHGT_P::HandleTaskMsg() {
	std::string log_info;

	try {
		std::string result_data;
		TaskItem* task_item = &m_list_task.front(); // 肯定有

		//FormatLibrary::StandardLibrary::FormatTo( log_info, "开始处理 {0} 号任务 ...", task_item->m_task_id );
		//LogPrint( basicx::syslog_level::c_info, log_info );

		try {
			Request request_temp;
			request_temp.m_task_id = task_item->m_task_id;
			request_temp.m_identity = task_item->m_identity;
			request_temp.m_code = task_item->m_code;

			int32_t ret_func = 0;
			if( NW_MSG_CODE_JSON == task_item->m_code ) {

				std::string errors_json;
				if( m_json_reader->parse( task_item->m_data.c_str(), task_item->m_data.c_str() + task_item->m_data.length(), &request_temp.m_req_json, &errors_json ) ) { // 含中文：std::string strUser = StringToGB2312( jsRootR["user"].asString() );
					ret_func = request_temp.m_req_json["function"].asInt();
				}
				else {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时数据 JSON 解析失败！{1}", task_item->m_task_id, errors_json );
					result_data = OnErrorResult( ret_func, -1, log_info, task_item->m_code );
				}
			}
			else {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时数据编码格式异常！", task_item->m_task_id );
				task_item->m_code = NW_MSG_CODE_JSON; // 编码格式未知时默认使用
				result_data = OnErrorResult( ret_func, -1, log_info, task_item->m_code );
			}

			if( ret_func > 0 ) {
				switch( ret_func ) {
				case TD_FUNC_QUOTE_ADDSUB: // 订阅行情
					m_user_request_list_lock.lock();
					m_list_user_request.push_back( request_temp );
					m_user_request_list_lock.unlock();
					break;
				case TD_FUNC_QUOTE_DELSUB: // 退订行情
					m_user_request_list_lock.lock();
					m_list_user_request.push_back( request_temp );
					m_user_request_list_lock.unlock();
					break;
				default: // 会话自处理请求
					if( "" == result_data ) { // 避免 result_data 覆盖
						FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时功能编号未知！function：{1}", task_item->m_task_id, ret_func );
						result_data = OnErrorResult( ret_func, -1, log_info, request_temp.m_code );
					}
				}
			}
			else {
				if( "" == result_data ) { // 避免 result_data 覆盖
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时功能编号异常！function：{1}", task_item->m_task_id, ret_func );
					result_data = OnErrorResult( ret_func, -1, log_info, request_temp.m_code );
				}
			}
		}
		catch( ... ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时发生错误，可能编码格式异常！", task_item->m_task_id );
			task_item->m_code = NW_MSG_CODE_JSON; // 编码格式未知时默认使用
			result_data = OnErrorResult( 0, -1, log_info, task_item->m_code );
		}

		if( result_data != "" ) { // 在任务转发前就发生错误了
			CommitResult( task_item->m_task_id, task_item->m_identity, task_item->m_code, result_data );
		}

		FormatLibrary::StandardLibrary::FormatTo( log_info, "处理 {0} 号任务完成。", task_item->m_task_id );
		//LogPrint( basicx::syslog_level::c_info, log_info );

		m_task_list_lock.lock();
		m_list_task.pop_front();
		bool write_on_progress = !m_list_task.empty();
		m_task_list_lock.unlock();

		if( write_on_progress && true == m_service_running ) {
			m_service->post( boost::bind( &QuoterHGT_P::HandleTaskMsg, this ) );
		}
	}
	catch( std::exception& ex ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "处理 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

// 自定义 QuoterHGT_P 函数实现

bool QuoterHGT_P::CheckSingleMutex() { // 单例限制检测
	m_single_mutex = ::CreateMutex( NULL, FALSE, L"quoter_hgt" );
	if( GetLastError() == ERROR_ALREADY_EXISTS ) {
		std::string log_info = "在一台服务器上只允许运行本插件一个实例！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		CloseHandle( m_single_mutex );
		return false;
	}
	return true;
}

void QuoterHGT_P::OnTimer() {
	std::string log_info;

	try {
		if( CreateDumpFolder() ) { // 含首次开启时文件创建
			m_thread_handle_market_data = boost::make_shared<boost::thread>( boost::bind( &QuoterHGT_P::HandleMarketData, this ) );

			bool force_dump = false; // 避免瞬间多次调用 Dump 操作
			bool init_some_thing = false;
			bool init_quote_data_file = false;
			while( true ) {
				for( size_t i = 0; i < (size_t)m_configs.m_dump_time; i++ ) { // 间隔需小于60秒
					std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
					//if( 前置行情服务器启动 )
					//{
					//	DumpSnapshotStock();
					//	force_dump = true;
					//}
				}
				if( false == force_dump ) {
					DumpSnapshotStock();
				}
				force_dump = false;

				tm now_time_t = basicx::GetNowTime();
				int32_t now_time = now_time_t.tm_hour * 100 + now_time_t.tm_min;

				char time_temp[32];
				strftime( time_temp, 32, "%H%M%S", &now_time_t);
				FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} SnapshotStock：R:{1}，W:{2}，C:{3}，S:{4}。", 
					time_temp, m_cache_snapshot_stock.m_recv_num.load(), m_cache_snapshot_stock.m_dump_num.load(), m_cache_snapshot_stock.m_comp_num.load(), m_cache_snapshot_stock.m_send_num.load() );
				LogPrint( basicx::syslog_level::c_info, log_info );

				m_cache_snapshot_stock.m_dump_num = 0;
				m_cache_snapshot_stock.m_comp_num = 0;
				m_cache_snapshot_stock.m_send_num = 0;

				if( now_time == m_configs.m_init_time && false == init_some_thing ) {
					// TODO：init some thing
					init_some_thing = true;
				}

				if( 600 == now_time && false == init_quote_data_file ) {
					InitQuoteDataFile();
					init_quote_data_file = true;

					ClearUnavailableConSub( m_csm_snapshot_stock, true );
					log_info = "清理无效订阅。";
					LogPrint( basicx::syslog_level::c_info, log_info );
				}

				if( 500 == now_time ) {
					init_some_thing = false;
					init_quote_data_file = false;
				}
			}
		}
	} // try
	catch( ... ) {
		log_info = "插件定时线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	log_info = "插件定时线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

bool QuoterHGT_P::CreateDumpFolder() {
	std::string log_info;

	std::string dump_data_folder_path = m_configs.m_dump_path;
	basicx::StringRightTrim( dump_data_folder_path, std::string( "\\" ) );

	WIN32_FIND_DATA find_dump_folder;
	HANDLE handle_dump_folder = FindFirstFile( basicx::StringToWideChar( dump_data_folder_path ).c_str(), &find_dump_folder );
	if( INVALID_HANDLE_VALUE == handle_dump_folder ) {
		FindClose( handle_dump_folder );
		FormatLibrary::StandardLibrary::FormatTo( log_info, "实时行情数据存储文件夹不存在！{0}", dump_data_folder_path );
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}
	FindClose( handle_dump_folder );

	dump_data_folder_path += "\\Stock_HGT";
	CreateDirectoryA( dump_data_folder_path.c_str(), NULL );

	m_cache_snapshot_stock.m_folder_path = dump_data_folder_path + "\\Stock";
	CreateDirectoryA( m_cache_snapshot_stock.m_folder_path.c_str(), NULL );

	InitQuoteDataFile(); // 首次启动时

	return true;
}

void QuoterHGT_P::InitQuoteDataFile() {
	char date_temp[9];
	tm now_time_t = basicx::GetNowTime();
	strftime( date_temp, 9, "%Y%m%d", &now_time_t);
	FormatLibrary::StandardLibrary::FormatTo( m_cache_snapshot_stock.m_file_path, "{0}\\{1}.hq", m_cache_snapshot_stock.m_folder_path, date_temp );

	bool is_new_file = false;
	WIN32_FIND_DATA find_file_temp;
	HANDLE handle_find_file = FindFirstFile( basicx::StringToWideChar( m_cache_snapshot_stock.m_file_path ).c_str(), &find_file_temp );
	if( INVALID_HANDLE_VALUE == handle_find_file ) { // 数据文件不存在
		is_new_file = true;
	}
	FindClose( handle_find_file );

	if( true == is_new_file ) { // 创建并写入 32 字节文件头部
		std::ofstream file_snapshot_stock;
		file_snapshot_stock.open( m_cache_snapshot_stock.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_stock ) {
			char head_temp[32] = { 0 };
			head_temp[0] = SNAPSHOT_STOCK_HGT_VERSION; // 结构体版本
			file_snapshot_stock.write( head_temp, 32 );
			file_snapshot_stock.close();
		}
		else {
			std::string log_info;
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotStock：创建转储文件失败！{0}", m_cache_snapshot_stock.m_file_path );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	}

	m_cache_snapshot_stock.m_recv_num = 0;
	m_cache_snapshot_stock.m_dump_num = 0;
	m_cache_snapshot_stock.m_comp_num = 0;
	m_cache_snapshot_stock.m_send_num = 0;
	m_cache_snapshot_stock.m_local_index = 0;
}

void QuoterHGT_P::MS_AddData_SnapshotStock( SnapshotStock_HGT& snapshot_stock_temp ) {
	//if( m_net_server_broad->Server_GetConnectCount() > 0 )
	if( m_csm_snapshot_stock.m_map_con_sub_all.size() > 0 || m_csm_snapshot_stock.m_map_con_sub_one.size() > 0 ) { // 全市场和单证券可以分别压缩以减少CPU压力提高效率，但一般还是全市场订阅的多
		int32_t output_buf_len_snapshot_stock = m_output_buf_len_snapshot_stock; // 必须每次重新赋值，否则压缩出错
		memset( m_output_buf_snapshot_stock, 0, m_output_buf_len_snapshot_stock );
		int32_t result = gzCompress( (unsigned char*)&snapshot_stock_temp, sizeof( snapshot_stock_temp ), m_output_buf_snapshot_stock, (uLongf*)&output_buf_len_snapshot_stock, m_data_compress );
		if( Z_OK == result ) { // 数据已压缩
			m_cache_snapshot_stock.m_comp_num++;
			std::string quote_data( (char*)m_output_buf_snapshot_stock, output_buf_len_snapshot_stock );
			quote_data = TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_HGT_HEAD + quote_data;

			// 先给全市场的吧，全市场订阅者可能还要自己进行过滤

			{ // 广播给全市场订阅者
				m_csm_snapshot_stock.m_lock_con_sub_all.lock();
				std::map<int32_t, basicx::ConnectInfo*> map_con_sub_all = m_csm_snapshot_stock.m_map_con_sub_all;
				m_csm_snapshot_stock.m_lock_con_sub_all.unlock();

				for( auto it = map_con_sub_all.begin(); it != map_con_sub_all.end(); it++ ) {
					if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
						m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
						m_cache_snapshot_stock.m_send_num++;
					}
				}
			}

			{ // 广播给单证券订阅者
				m_csm_snapshot_stock.m_lock_con_sub_one.lock();
				std::map<std::string, ConSubOne*> map_con_sub_one = m_csm_snapshot_stock.m_map_con_sub_one;
				m_csm_snapshot_stock.m_lock_con_sub_one.unlock();

				std::map<std::string, ConSubOne*>::iterator it_cso = map_con_sub_one.find( snapshot_stock_temp.m_code );
				if( it_cso != map_con_sub_one.end() ) {
					ConSubOne* con_sub_one = it_cso->second; // 运行期间 con_sub_one 所指对象不会被删除

					con_sub_one->m_lock_con_sub_one.lock();
					std::map<int32_t, basicx::ConnectInfo*> map_identity = con_sub_one->m_map_identity;
					con_sub_one->m_lock_con_sub_one.unlock();

					for( auto it = map_identity.begin(); it != map_identity.end(); it++ ) {
						if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
							m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
							m_cache_snapshot_stock.m_send_num++;
						}
					}
				}
			}
		}
	}

	if( 1 == m_configs.m_need_dump ) {
		m_cache_snapshot_stock.m_lock_cache.lock();
		m_cache_snapshot_stock.m_vec_cache_put->push_back( snapshot_stock_temp );
		m_cache_snapshot_stock.m_lock_cache.unlock();
	}
}

void QuoterHGT_P::DumpSnapshotStock() {
	std::string log_info;

	m_cache_snapshot_stock.m_lock_cache.lock();
	if( m_cache_snapshot_stock.m_vec_cache_put == &m_cache_snapshot_stock.m_vec_cache_01 ) {
		m_cache_snapshot_stock.m_vec_cache_put = &m_cache_snapshot_stock.m_vec_cache_02;
		m_cache_snapshot_stock.m_vec_cache_put->clear();
		m_cache_snapshot_stock.m_vec_cache_out = &m_cache_snapshot_stock.m_vec_cache_01;
	}
	else {
		m_cache_snapshot_stock.m_vec_cache_put = &m_cache_snapshot_stock.m_vec_cache_01;
		m_cache_snapshot_stock.m_vec_cache_put->clear();
		m_cache_snapshot_stock.m_vec_cache_out = &m_cache_snapshot_stock.m_vec_cache_02;
	}
	m_cache_snapshot_stock.m_lock_cache.unlock();

	size_t out_data_number = m_cache_snapshot_stock.m_vec_cache_out->size();
	if( out_data_number > 0 ) { // 避免无新数据时操作文件
		std::ofstream file_snapshot_stock;
		file_snapshot_stock.open( m_cache_snapshot_stock.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_stock ) {
			for( size_t i = 0; i < out_data_number; ++i ) {
				file_snapshot_stock.write( (const char*)(&(*m_cache_snapshot_stock.m_vec_cache_out)[i]), sizeof( SnapshotStock_HGT ) );
			}
			file_snapshot_stock.close(); // 已含 flush() 动作
			m_cache_snapshot_stock.m_dump_num += out_data_number;
		}
		else {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotStock：打开转储文件失败！{0}", m_cache_snapshot_stock.m_file_path );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	}
}

void QuoterHGT_P::StartNetServer() {
	m_data_encode = m_configs.m_data_encode;
	m_data_compress = m_configs.m_data_compress;

	m_output_buf_len_snapshot_stock = sizeof( SnapshotStock_HGT ) + sizeof( SnapshotStock_HGT ) / 3 + 128; // > ( nInputLen + 12 ) * 1.001;
	m_output_buf_snapshot_stock = new unsigned char[m_output_buf_len_snapshot_stock]; // 需足够大

	basicx::CfgBasic* cfg_basic = m_syscfg->GetCfgBasic();

	m_net_server_broad = boost::make_shared<basicx::NetServer>();
	m_net_server_query = boost::make_shared<basicx::NetServer>();
	m_net_server_broad->ComponentInstance( this );
	m_net_server_query->ComponentInstance( this );
	basicx::NetServerCfg net_server_cfg_temp;
	net_server_cfg_temp.m_log_test = cfg_basic->m_debug_infos_server;
	net_server_cfg_temp.m_heart_check_time = cfg_basic->m_heart_check_server;
	net_server_cfg_temp.m_max_msg_cache_number = cfg_basic->m_max_msg_cache_server;
	net_server_cfg_temp.m_io_work_thread_number = cfg_basic->m_work_thread_server;
	net_server_cfg_temp.m_client_connect_timeout = cfg_basic->m_con_time_out_server;
	net_server_cfg_temp.m_max_connect_total_s = cfg_basic->m_con_max_server_server;
	net_server_cfg_temp.m_max_data_length_s = cfg_basic->m_data_length_server;
	m_net_server_broad->StartNetwork( net_server_cfg_temp );
	m_net_server_query->StartNetwork( net_server_cfg_temp );

	for( size_t i = 0; i < cfg_basic->m_vec_server_server.size(); i++ ) {
		if( "broad_hgt" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_broad->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
		if( "query_hgt" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_query->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
	}
}

void QuoterHGT_P::OnNetServerInfo( basicx::NetServerInfo& net_server_info_temp ) {
	// TODO：清理全市场订阅和单证券订阅
}

void QuoterHGT_P::OnNetServerData( basicx::NetServerData& net_server_data_temp ) {
	try {
		if( m_task_id >= 214748300/*0*/ ) { // 2 ^ 31 = 2147483648
			m_task_id = 0;
		}
		m_task_id++;

		if( "broad_hgt" == net_server_data_temp.m_node_type ) { // 1 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 1, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 1
		}
		else if( "query_hgt" == net_server_data_temp.m_node_type ) { // 2 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 2, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 2
		}
	}
	catch( ... ) {
		std::string log_info = "转发网络数据 发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

int32_t QuoterHGT_P::gzCompress( Bytef* data_in, uLong size_in, Bytef* data_out, uLong* size_out, int32_t level ) {
	int32_t error = 0;
	z_stream c_stream;

	if( data_in && size_in > 0 ) {
		c_stream.opaque = (voidpf)0;
		c_stream.zfree = (free_func)0;
		c_stream.zalloc = (alloc_func)0;

		// 默认 level 为 Z_DEFAULT_COMPRESSION，windowBits 须为 MAX_WBITS+16 而不是 -MAX_WBITS，否则压缩的数据 WinRAR 解不开
		if( deflateInit2( &c_stream, level, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY ) != Z_OK ) {
			return -1;
		}

		c_stream.next_in = data_in;
		c_stream.avail_in = size_in;
		c_stream.next_out = data_out;
		c_stream.avail_out = *size_out;

		while( c_stream.avail_in != 0 && c_stream.total_out < *size_out ) {
			if( deflate( &c_stream, Z_NO_FLUSH ) != Z_OK ) {
				return -1;
			}
		}

		if( c_stream.avail_in != 0 ) {
			return c_stream.avail_in;
		}

		for( ; ; ) {
			if( ( error = deflate( &c_stream, Z_FINISH ) ) == Z_STREAM_END ) {
				break;
			}
			if( error != Z_OK ) {
				return -1;
			}
		}

		if( deflateEnd( &c_stream ) != Z_OK ) {
			return -1;
		}

		*size_out = c_stream.total_out;

		return 0;
	}

	return -1;
}

void QuoterHGT_P::HandleUserRequest() {
	std::string log_info;

	log_info = "创建用户请求处理线程完成, 开始进行用户请求处理 ...";
	LogPrint( basicx::syslog_level::c_info, log_info );

	try {
		while( 1 ) {
			if( !m_list_user_request.empty() ) {
				std::string result_data = "";
				Request* request = &m_list_user_request.front();

				try {
					int32_t ret_func = 0;
					if( NW_MSG_CODE_JSON == request->m_code ) {
						ret_func = request->m_req_json["function"].asInt();
					}

					// 只可能为 TD_FUNC_QUOTE_ADDSUB、TD_FUNC_QUOTE_DELSUB、
					switch( ret_func ) {
					case TD_FUNC_QUOTE_ADDSUB: // 订阅行情
						result_data = OnUserAddSub( request );
						break;
					case TD_FUNC_QUOTE_DELSUB: // 退订行情
						result_data = OnUserDelSub( request );
						break;
					}
				}
				catch( ... ) {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 用户请求时发生未知错误！", request->m_task_id );
					result_data = OnErrorResult( 0, -1, log_info, request->m_code );
				}

				CommitResult( request->m_task_id, request->m_identity, request->m_code, result_data );

				m_user_request_list_lock.lock();
				m_list_user_request.pop_front();
				m_user_request_list_lock.unlock();
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	} // try
	catch( ... ) {
		log_info = "用户请求处理线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	log_info = "用户请求处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

std::string QuoterHGT_P::OnUserAddSub( Request* request ) {
	std::string log_info;

	int32_t quote_type = 0;
	std::string quote_list = "";
	if( NW_MSG_CODE_JSON == request->m_code ) {
		quote_type = request->m_req_json["quote_type"].asInt();
		quote_list = request->m_req_json["quote_list"].asString();
	}
	if( 0 == quote_type ) {
		log_info = "行情订阅时 行情类型 异常！";
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	basicx::ConnectInfo* connect_info = m_net_server_broad->Server_GetConnect( request->m_identity );
	if( nullptr == connect_info ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅时 获取 {0} 连接信息 异常！", request->m_identity );
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	if( TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_HGT == quote_type ) {
		if( "" == quote_list ) { // 订阅全市场
			AddConSubAll( m_csm_snapshot_stock, request->m_identity, connect_info );
			ClearConSubOne( m_csm_snapshot_stock, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_stock, request->m_identity ) == false ) { // 未全市场订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						AddConSubOne( m_csm_snapshot_stock, vec_filter_temp[i], request->m_identity, connect_info );
					}
				}
			}
		}
	}
	else {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅时 行情类型 未知！{0}", quote_type );
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅成功。{0}", quote_type );
	LogPrint( basicx::syslog_level::c_info, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_ADDSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

std::string QuoterHGT_P::OnUserDelSub( Request* request ) {
	std::string log_info;

	int32_t quote_type = 0;
	std::string quote_list = "";
	if( NW_MSG_CODE_JSON == request->m_code ) {
		quote_type = request->m_req_json["quote_type"].asInt();
		quote_list = request->m_req_json["quote_list"].asString();
	}
	if( 0 == quote_type ) {
		log_info = "行情退订时 行情类型 异常！";
		return OnErrorResult( TD_FUNC_QUOTE_DELSUB, -1, log_info, request->m_code );
	}

	if( TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_HGT == quote_type ) {
		if( "" == quote_list ) { // 退订全市场
			DelConSubAll( m_csm_snapshot_stock, request->m_identity );
			ClearConSubOne( m_csm_snapshot_stock, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_stock, request->m_identity ) == false ) { // 未全市场订阅，已订阅全市场的话不可能还有单证券订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						DelConSubOne( m_csm_snapshot_stock, vec_filter_temp[i], request->m_identity );
					}
				}
			}
		}
	}
	else {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情退订时 行情类型 未知！{0}", quote_type );
		return OnErrorResult( TD_FUNC_QUOTE_DELSUB, -1, log_info, request->m_code );
	}

	FormatLibrary::StandardLibrary::FormatTo( log_info, "行情退订成功。{0}", quote_type );
	LogPrint( basicx::syslog_level::c_info, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_DELSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

void QuoterHGT_P::AddConSubAll( ConSubMan& csm_con_sub_man, int32_t identity, basicx::ConnectInfo* connect_info ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all[identity] = connect_info;
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

void QuoterHGT_P::DelConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all.erase( identity );
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "退订 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

bool QuoterHGT_P::IsConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	bool is_all_sub_already = false;
	csm_con_sub_man.m_lock_con_sub_all.lock();
	std::map<int32_t, basicx::ConnectInfo*>::iterator it_csa = csm_con_sub_man.m_map_con_sub_all.find( identity );
	if( it_csa != csm_con_sub_man.m_map_con_sub_all.end() ) {
		is_all_sub_already = true;
	}
	csm_con_sub_man.m_lock_con_sub_all.unlock();
	return is_all_sub_already;
}

void QuoterHGT_P::AddConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity, basicx::ConnectInfo* connect_info ) {
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	std::map<std::string, ConSubOne*>::iterator it_cso = csm_con_sub_man.m_map_con_sub_one.find( code );
	if( it_cso != csm_con_sub_man.m_map_con_sub_one.end() ) {
		con_sub_one = it_cso->second;
	}
	else {
		con_sub_one = new ConSubOne;
		csm_con_sub_man.m_map_con_sub_one[code] = con_sub_one;
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();

	con_sub_one->m_lock_con_sub_one.lock();
	con_sub_one->m_map_identity[identity] = connect_info;
	con_sub_one->m_lock_con_sub_one.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅 单证券：{0} {1}", code, identity );
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

void QuoterHGT_P::DelConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity ) {
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	std::map<std::string, ConSubOne*>::iterator it_cso = csm_con_sub_man.m_map_con_sub_one.find( code );
	if( it_cso != csm_con_sub_man.m_map_con_sub_one.end() ) {
		con_sub_one = it_cso->second;
		con_sub_one->m_lock_con_sub_one.lock();
		con_sub_one->m_map_identity.erase( identity );
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			csm_con_sub_man.m_map_con_sub_one.erase( code );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}

		//std::string log_info;
		//FormatLibrary::StandardLibrary::FormatTo( log_info, "退订 单证券：{0} {1}", code, identity );
		//LogPrint( basicx::syslog_level::c_debug, log_info );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();
}

void QuoterHGT_P::ClearConSubOne( ConSubMan& csm_con_sub_man, int32_t identity ) {
	std::vector<std::string> vec_del_key_temp;
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	for( auto it = csm_con_sub_man.m_map_con_sub_one.begin(); it != csm_con_sub_man.m_map_con_sub_one.end(); it++ ) {
		con_sub_one = it->second;
		con_sub_one->m_lock_con_sub_one.lock();
		con_sub_one->m_map_identity.erase( identity );
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			vec_del_key_temp.push_back( it->first );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}
	}
	for( size_t i = 0; i < vec_del_key_temp.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_one.erase( vec_del_key_temp[i] );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();
}

void QuoterHGT_P::ClearUnavailableConSub( ConSubMan& csm_con_sub_man, bool is_idle_time ) { // 清理所有已经失效的连接订阅，一般在空闲时进行
	std::vector<int32_t> vec_del_key_csa;
	csm_con_sub_man.m_lock_con_sub_all.lock();
	for( auto it = csm_con_sub_man.m_map_con_sub_all.begin(); it != csm_con_sub_man.m_map_con_sub_all.end(); it++ ) {
		if( false == m_net_server_broad->IsConnectAvailable( it->second ) ) { // 已失效
			vec_del_key_csa.push_back( it->first );
		}
	}
	for( size_t i = 0; i < vec_del_key_csa.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_all.erase( vec_del_key_csa[i] );
	}
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	std::vector<std::string> vec_del_key_cso;
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	for( auto it_cso = csm_con_sub_man.m_map_con_sub_one.begin(); it_cso != csm_con_sub_man.m_map_con_sub_one.end(); it_cso++ ) {
		con_sub_one = it_cso->second;
		std::vector<int32_t> vec_del_key_id;
		con_sub_one->m_lock_con_sub_one.lock();
		for( auto it_id = con_sub_one->m_map_identity.begin(); it_id != con_sub_one->m_map_identity.end(); it_id++ ) {
			if( false == m_net_server_broad->IsConnectAvailable( it_id->second ) ) { // 已失效
				vec_del_key_id.push_back( it_id->first );
			}
		}
		for( size_t i = 0; i < vec_del_key_id.size(); i++ ) {
			con_sub_one->m_map_identity.erase( vec_del_key_id[i] );
		}
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			vec_del_key_cso.push_back( it_cso->first );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}
	}
	for( size_t i = 0; i < vec_del_key_cso.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_one.erase( vec_del_key_cso[i] );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();

	if( true == is_idle_time ) { // 如果确定是在空闲时执行，则 m_list_one_sub_con_del 也可以清理
		csm_con_sub_man.m_lock_one_sub_con_del.lock();
		for( auto it = csm_con_sub_man.m_list_one_sub_con_del.begin(); it != csm_con_sub_man.m_list_one_sub_con_del.end(); it++ ) {
			if( (*it) != nullptr ) {
				delete (*it);
			}
		}
		csm_con_sub_man.m_list_one_sub_con_del.clear();
		csm_con_sub_man.m_lock_one_sub_con_del.unlock();
	}
}

void QuoterHGT_P::CommitResult( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	int32_t net_flag = task_id % 10;
	if( 1 == net_flag ) { // broad：1
		basicx::ConnectInfo* connect_info = m_net_server_broad->Server_GetConnect( identity );
		if( connect_info != nullptr ) {
			m_net_server_broad->Server_SendData( connect_info, NW_MSG_TYPE_USER_DATA, code, data );
		}
	}
	else if( 2 == net_flag ) { // query：2
		basicx::ConnectInfo* connect_info = m_net_server_query->Server_GetConnect( identity );
		if( connect_info != nullptr ) {
			m_net_server_query->Server_SendData( connect_info, NW_MSG_TYPE_USER_DATA, code, data );
		}
	}
}

std::string QuoterHGT_P::OnErrorResult( int32_t ret_func, int32_t ret_code, std::string ret_info, int32_t encode ) {
	LogPrint( basicx::syslog_level::c_error, ret_info );

	if( NW_MSG_CODE_JSON == encode ) {
		Json::Value results_json;
		results_json["ret_func"] = ret_func;
		results_json["ret_code"] = ret_code;
		results_json["ret_info"] = basicx::StringToUTF8( ret_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

void QuoterHGT_P::HandleMarketData() {
	std::string log_info;

	FILE* market_data_file = nullptr;

	SnapshotStock_HGT snapshot_stock_temp;

	while( true == m_plugin_running ) {
		SYSTEMTIME sys_time;
		GetLocalTime( &sys_time );
		tm now_time_t = basicx::GetNowTime();
		//char now_date_temp[9];
		//strftime( now_date_temp, 9, "%Y%m%d", &now_time_t);

		int32_t local_date = ( now_time_t.tm_year + 1900 ) * 10000 + ( now_time_t.tm_mon + 1 ) * 100 + now_time_t.tm_mday; // YYYYMMDD
		int32_t local_time = now_time_t.tm_hour * 100 + now_time_t.tm_min; // HHMM

		if( ( local_time < m_configs.m_source_time_start ) || ( local_time >= m_configs.m_source_time_stop ) ) { // 此时间段外不进行读取更新
			std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
			continue; // while
		}

		if( ( m_local_date < local_date ) && ( local_time >= m_configs.m_init_time ) ) { // 重新打开文件，直到打开成功
			if( market_data_file != nullptr ) {
				fclose( market_data_file ); //
				market_data_file = nullptr;
			}
			//fopen_s( &market_data_file, m_market_data_file_path.c_str(), "rb" ); // 非共享读 // 只读模式下才共享读，其他不共享
			market_data_file = _fsopen( m_market_data_file_path.c_str(), "rb", _SH_DENYNO );
			if( market_data_file != nullptr ) {
				m_local_date = local_date; // 完成
				FormatLibrary::StandardLibrary::FormatTo( log_info, "行情数据源文件打开成功。{0}", m_market_data_file_path );
				LogPrint( basicx::syslog_level::c_info, log_info );
			}
		}

		if( market_data_file != nullptr ) {
			try {
				fseek( market_data_file, 0, SEEK_END ); // 移动到文件末尾 // 必须
				size_t file_length = ftell( market_data_file ); // 获取文件长度
				//std::cout << file_length << std::endl;
				if( ( file_length + 1 ) > m_market_data_cache_size ) {
					if( m_market_data_cache != nullptr ) {
						delete[] m_market_data_cache;
						m_market_data_cache = nullptr;
					}
					m_market_data_cache_size = file_length + 1;
					m_market_data_cache = new char[m_market_data_cache_size];
				}

				Define_Head define_head;
				if( file_length > define_head.m_line_size ) {
					size_t cache_offset = 0; // 缓存位置偏移
					memset( m_market_data_cache, 0, m_market_data_cache_size ); //
					fseek( market_data_file, 0, SEEK_SET ); // 移动到文件开始
					fread( m_market_data_cache, file_length, 1, market_data_file ); // 一次性读取

					define_head.FillData( m_market_data_cache, cache_offset );
					cache_offset += define_head.m_line_size; //
					//define_head.Print();
					//std::cout << m_market_data_time << " " << define_head.m_market_data_time << " " << define_head.m_TotNumTradeReports << std::endl;

					if( define_head.m_market_data_time > m_market_data_time ) { // 文件已更新
						m_market_data_time = define_head.m_market_data_time;

						for( int32_t i = 0; i < define_head.m_TotNumTradeReports; ++i ) {
							Define_Type define_type;
							if( ( file_length - cache_offset ) > define_type.m_line_size ) {
								define_type.FillData( m_market_data_cache, cache_offset );
								cache_offset += define_type.m_line_size;
							    //define_type.Print();

								memset( &snapshot_stock_temp, 0, sizeof( SnapshotStock_HGT ) );
								if( "MD401" == define_type.m_MDStreamID ) {
									Define_MD401 define_md;
									if( ( file_length - cache_offset ) > define_md.m_line_size ) {
										define_md.FillData( m_market_data_cache, cache_offset );
										cache_offset += define_md.m_line_size; //
										//Result_MD401 result_md;
										//result_md.FillData( define_md );
										//result_md.Print();
										FillMarketData( "MD401", snapshot_stock_temp, define_md );
									}
									else {
										break; // for
									}
								}
								else if( "MD404" == define_type.m_MDStreamID ) {
									Define_MD404 define_md;
									if( ( file_length - cache_offset ) > define_md.m_line_size ) {
										define_md.FillData( m_market_data_cache, cache_offset );
										cache_offset += define_md.m_line_size; //
										//Result_MD404 result_md;
										//result_md.FillData( define_md );
										//result_md.Print();
										FillMarketData( "MD404", snapshot_stock_temp, define_md );
									}
									else {
										break; // for
									}
								}
								else if( "MD405" == define_type.m_MDStreamID ) {
									Define_MD405 define_md;
									if( ( file_length - cache_offset ) > define_md.m_line_size ) {
										define_md.FillData( m_market_data_cache, cache_offset );
										cache_offset += define_md.m_line_size; //
										//Result_MD405 result_md;
										//result_md.FillData( define_md );
										//result_md.Print();
										FillMarketData( "MD405", snapshot_stock_temp, define_md );
									}
									else {
										break; // for
									}
								}
								snapshot_stock_temp.m_local_time = now_time_t.tm_hour * 10000000 + now_time_t.tm_min * 100000 + now_time_t.tm_sec * 1000 + sys_time.wMilliseconds; // 本地时间 // HHMMSSmmm 精度：毫秒
								m_cache_snapshot_stock.m_local_index++;
								snapshot_stock_temp.m_local_index = m_cache_snapshot_stock.m_local_index; // 本地序号

								m_cache_snapshot_stock.m_recv_num++;

								MS_AddData_SnapshotStock( snapshot_stock_temp );

								//std::string log_info;
								//FormatLibrary::StandardLibrary::FormatTo( log_info, "行情反馈：代码:{0} 名称:{1} 最新:{2} 昨收:{3} 时间:{4}", 
								//	snapshot_stock_temp.m_code, snapshot_stock_temp.m_name, snapshot_stock_temp.m_last, snapshot_stock_temp.m_pre_close, snapshot_stock_temp.m_quote_time );
								//LogPrint( basicx::syslog_level::c_debug, log_info );
							}
							else {
								break; // for
							}
						}

						Define_Tail define_tail;
						if( ( file_length - cache_offset ) >= define_tail.m_line_size ) {
							define_tail.FillData( m_market_data_cache, cache_offset );
							cache_offset += define_tail.m_line_size; //
							//define_tail.Print();
						}
					}
				}
			}
			catch( ... ) {
				std::string log_info = "行情数据处理发生未知异常！";
				LogPrint( basicx::syslog_level::c_error, log_info );
			}
			std::this_thread::sleep_for( std::chrono::seconds( 3 ) );
		}
		else {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "行情数据源文件打开失败！{0}", m_market_data_file_path );
			LogPrint( basicx::syslog_level::c_error, log_info );
			std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
		}
	} // while

	log_info = "行情数据处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

void QuoterHGT_P::FillMarketData( std::string data_type, SnapshotStock_HGT& snapshot_stock, Define_MD401& market_data ) {
	try { // 防 m_txt 空
		//memcpy( snapshot_stock.m_MDStreamID, data_type.c_str(), data_type.length() ); // 行情数据类型 // C5
		//memcpy( snapshot_stock.m_SecurityID, market_data.m_item_02.m_txt, 5 ); // 证券代码 // C5
		//std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		//memcpy( snapshot_stock.m_Symbol, symbol.c_str(), symbol.length() ); // 中文证券简称 // C32
		//std::string symbol_en = basicx::StringTrim( market_data.m_item_04.m_txt, " " );
		//memcpy( snapshot_stock.m_SymbolEn, symbol_en.c_str(), symbol_en.length() ); // 英文证券简称 // C15/16
		//snapshot_stock.m_TradeVolume = atoll( market_data.m_item_05.m_txt ); // 成交数量
		//snapshot_stock.m_TotalValueTraded = (int64_t)(atof( market_data.m_item_06.m_txt ) * 10000); // 成交金额 // 10000
		//snapshot_stock.m_PreClosePx = (uint32_t)(atof( market_data.m_item_07.m_txt ) * 10000); // 昨日收盘价 // 10000
		//snapshot_stock.m_NominalPrice = (uint32_t)(atof( market_data.m_item_08.m_txt ) * 10000); // 按盘价 // 10000
		//snapshot_stock.m_HighPrice = (uint32_t)(atof( market_data.m_item_09.m_txt ) * 10000); // 最高价 // 10000
		//snapshot_stock.m_LowPrice = (uint32_t)(atof( market_data.m_item_10.m_txt ) * 10000); // 最低价 // 10000
		//snapshot_stock.m_TradePrice = (uint32_t)(atof( market_data.m_item_11.m_txt ) * 10000); // 最新价 // 10000
		//snapshot_stock.m_BuyPrice1 = (uint32_t)(atof( market_data.m_item_12.m_txt ) * 10000); // 申买价一 // 10000
		//snapshot_stock.m_BuyVolume1 = atoi( market_data.m_item_13.m_txt ); // 申买量一
		//snapshot_stock.m_SellPrice1 = (uint32_t)(atof( market_data.m_item_14.m_txt ) * 10000); // 申卖价一 // 10000
		//snapshot_stock.m_SellVolume1 = atoi( market_data.m_item_15.m_txt ); // 申卖量一
		//snapshot_stock.m_SecTradingStatus = atoi( market_data.m_item_16.m_txt ); // 证券交易状态

		memcpy( snapshot_stock.m_code, market_data.m_item_02.m_txt, 5 ); // 证券代码
		std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		memcpy( snapshot_stock.m_name, symbol.c_str(), symbol.length() ); // 证券名称 // 中文名称
		strcpy_s( snapshot_stock.m_type, "H" ); // 证券类型
		strcpy_s( snapshot_stock.m_market, "HK" ); // 证券市场
		int32_t status = atoi( market_data.m_item_16.m_txt );
		if( 0 == status ) { // 正常
			strcpy_s( snapshot_stock.m_status, "N" ); // 证券状态 // "N"、"S"、"X"
		}
		else if( 1 == status ) { // 暂停交易
			strcpy_s( snapshot_stock.m_status, "S" ); // 证券状态 // "N"、"S"、"X"
		}
		else {
			strcpy_s( snapshot_stock.m_status, "X" ); // 证券状态 // "N"、"S"、"X"
		}
		snapshot_stock.m_last = (uint32_t)( atof( market_data.m_item_11.m_txt ) * 10000 ); // 最新价 // 10000
		//snapshot_stock.m_open; // 开盘价 // 10000 // 无
		snapshot_stock.m_high = (uint32_t)( atof( market_data.m_item_09.m_txt ) * 10000 ); // 最高价 // 10000
		snapshot_stock.m_low = (uint32_t)( atof( market_data.m_item_10.m_txt ) * 10000 ); // 最低价 // 10000
		snapshot_stock.m_close = (uint32_t)( atof( market_data.m_item_08.m_txt ) * 10000 ); // 收盘价 // 10000 // 按盘价
		snapshot_stock.m_pre_close = (uint32_t)( atof( market_data.m_item_07.m_txt ) * 10000 ); // 昨收价 // 10000
		snapshot_stock.m_volume = atoll( market_data.m_item_05.m_txt ); // 成交量
		snapshot_stock.m_turnover = (int64_t)( atof( market_data.m_item_06.m_txt ) * 10000 ); // 成交额 // 10000
		snapshot_stock.m_ask_price[0] = (uint32_t)( atof( market_data.m_item_14.m_txt ) * 10000 ); // 申卖价一 // 10000
		snapshot_stock.m_ask_volume[0] = atoi( market_data.m_item_15.m_txt ); // 申卖量一
		snapshot_stock.m_bid_price[0] = (uint32_t)( atof( market_data.m_item_12.m_txt ) * 10000 ); // 申买价一 // 10000
		snapshot_stock.m_bid_volume[0] = atoi( market_data.m_item_13.m_txt ); // 申买量一
		//snapshot_stock.m_ask_price[1-9]; // 申卖价 // 10000
		//snapshot_stock.m_ask_volume[1-9]; // 申卖量
		//snapshot_stock.m_bid_price[1-9]; // 申买价 // 10000
		//snapshot_stock.m_bid_volume[1-9]; // 申买量
		//snapshot_stock.m_high_limit; // 涨停价 // 10000 // 无
		//snapshot_stock.m_low_limit; // 跌停价 // 10000 // 无
		//snapshot_stock.m_trade_count; // 成交笔数 // 无
		//snapshot_stock.m_vcm_start_time; // 冷静期开始时间 // HHMMSS000 精度：秒
		//snapshot_stock.m_vcm_end_time; // 冷静期结束时间 // HHMMSS000 精度：秒
		std::string timestamp = market_data.m_item_17.m_txt;
		if( timestamp != "" ) { // HH:MM:SS.000
			int32_t md_hour = atoi( timestamp.substr( 0, 2 ).c_str() );
			int32_t md_minute = atoi( timestamp.substr( 3, 2 ).c_str() );
			int32_t md_second = atoi( timestamp.substr( 6, 2 ).c_str() );
			snapshot_stock.m_quote_time = md_hour * 10000000 + md_minute * 100000 + md_second * 1000; // 行情时间 // HHMMSS000 精度：秒
		}
	}
	catch( ... ) {
		std::string log_info = "处理 MD401 数据发生未知异常！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

void QuoterHGT_P::FillMarketData( std::string data_type, SnapshotStock_HGT& snapshot_stock, Define_MD404& market_data ) {
	try { // 防 m_txt 空
		//memcpy( snapshot_stock.m_MDStreamID, data_type.c_str(), data_type.length() ); // 行情数据类型 // C5
		//memcpy( snapshot_stock.m_SecurityID, market_data.m_item_02.m_txt, 5 ); // 证券代码 // C5
		//std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		//memcpy( snapshot_stock.m_Symbol, symbol.c_str(), symbol.length() ); // 中文证券简称 // C32
		//std::string symbol_en = basicx::StringTrim( market_data.m_item_04.m_txt, " " );
		//memcpy( snapshot_stock.m_SymbolEn, symbol_en.c_str(), symbol_en.length() ); // 英文证券简称 // C15/16
		//snapshot_stock.m_VCMRefPrice = (uint32_t)( atof( market_data.m_item_07.m_txt ) * 10000 ); // 市调机制参考价 // 10000
		//snapshot_stock.m_VCMLowerPrice = (uint32_t)( atof( market_data.m_item_08.m_txt ) * 10000 ); // 市调机制下限价 // 10000
		//snapshot_stock.m_VCMUpperPrice = (uint32_t)( atof( market_data.m_item_09.m_txt ) * 10000 ); // 市调机制上限价 // 10000

		memcpy( snapshot_stock.m_code, market_data.m_item_02.m_txt, 5 ); // 证券代码
		std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		memcpy( snapshot_stock.m_name, symbol.c_str(), symbol.length() ); // 证券名称 // 中文名称
		strcpy_s( snapshot_stock.m_type, "H" ); // 证券类型 // "H"
		strcpy_s( snapshot_stock.m_market, "HK" ); // 证券市场 // "HK"
		strcpy_s( snapshot_stock.m_status, "N" ); // 证券状态 // "N"、"S"、"X"
		//snapshot_stock.m_last; // 最新价 // 10000
		//snapshot_stock.m_nominal; // 按盘价 // 10000
		//snapshot_stock.m_high; // 最高价 // 10000
		//snapshot_stock.m_low; // 最低价 // 10000
		//snapshot_stock.m_close; // 收盘价 // 10000
		//snapshot_stock.m_pre_close; // 昨收价 // 10000
		//snapshot_stock.m_volume; // 成交量
		//snapshot_stock.m_turnover; // 成交额 // 10000
		snapshot_stock.m_ask_price[0] = (uint32_t)( atof( market_data.m_item_07.m_txt ) * 10000 ); // 市调机制参考价 // 10000
		//snapshot_stock.m_ask_price[1-9]; // 申卖价 // 10000
		//snapshot_stock.m_ask_volume[10]; // 申卖量
		snapshot_stock.m_bid_price[0] = (uint32_t)( atof( market_data.m_item_07.m_txt ) * 10000 ); // 市调机制参考价 // 10000
		//snapshot_stock.m_bid_price[1-9]; // 申买价 // 10000
		//snapshot_stock.m_bid_volume[10]; // 申买量
		snapshot_stock.m_high_limit = (uint32_t)( atof( market_data.m_item_09.m_txt ) * 10000 ); // 市调机制上限价 // 10000
		snapshot_stock.m_low_limit = (uint32_t)( atof( market_data.m_item_08.m_txt ) * 10000 ); // 市调机制下限价 // 10000
		//snapshot_stock.m_trade_count; // 成交笔数 // 无
		std::string vcm_start_time = market_data.m_item_05.m_txt;
		if( vcm_start_time != "" ) { // HH:MM:SS
			int32_t vcm_start_hour = atoi( vcm_start_time.substr( 0, 2 ).c_str() );
			int32_t vcm_start_minute = atoi( vcm_start_time.substr( 3, 2 ).c_str() );
			int32_t vcm_start_second = atoi( vcm_start_time.substr( 6, 2 ).c_str() );
			snapshot_stock.m_vcm_start_time = vcm_start_hour * 10000000 + vcm_start_minute * 100000 + vcm_start_second * 1000; // 冷静期开始时间 // HHMMSS000 精度：秒
		}
		std::string vcm_end_time = market_data.m_item_06.m_txt;
		if( vcm_end_time != "" ) { // HH:MM:SS
			int32_t vcm_end_hour = atoi( vcm_end_time.substr( 0, 2 ).c_str() );
			int32_t vcm_end_minute = atoi( vcm_end_time.substr( 3, 2 ).c_str() );
			int32_t vcm_end_second = atoi( vcm_end_time.substr( 6, 2 ).c_str() );
			snapshot_stock.m_vcm_end_time = vcm_end_hour * 10000000 + vcm_end_minute * 100000 + vcm_end_second * 1000; // 冷静期结束时间 // HHMMSS000 精度：秒
		}
		std::string timestamp = market_data.m_item_10.m_txt;
		if( timestamp != "" ) { // HH:MM:SS.000
			int32_t md_hour = atoi( timestamp.substr( 0, 2 ).c_str() );
			int32_t md_minute = atoi( timestamp.substr( 3, 2 ).c_str() );
			int32_t md_second = atoi( timestamp.substr( 6, 2 ).c_str() );
			snapshot_stock.m_quote_time = md_hour * 10000000 + md_minute * 100000 + md_second * 1000; // 行情时间 // HHMMSS000 精度：秒
		}
	}
	catch( ... ) {
		std::string log_info = "处理 MD404 数据发生未知异常！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

void QuoterHGT_P::FillMarketData( std::string data_type, SnapshotStock_HGT& snapshot_stock, Define_MD405& market_data ) {
	try { // 防 m_txt 空
		//memcpy( snapshot_stock.m_MDStreamID, data_type.c_str(), data_type.length() ); // 行情数据类型 // C5
		//memcpy( snapshot_stock.m_SecurityID, market_data.m_item_02.m_txt, 5 ); // 证券代码 // C5
		//std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		//memcpy( snapshot_stock.m_Symbol, symbol.c_str(), symbol.length() ); // 中文证券简称 // C32
		//std::string symbol_en = basicx::StringTrim( market_data.m_item_04.m_txt, " " );
		//memcpy( snapshot_stock.m_SymbolEn, symbol_en.c_str(), symbol_en.length() ); // 英文证券简称 // C15/16
		//snapshot_stock.m_CASRefPrice = (uint32_t)(atof( market_data.m_item_05.m_txt ) * 10000); // 收盘集合竞价时段参考价 // 10000
		//snapshot_stock.m_CASLowerPrice = (uint32_t)(atof( market_data.m_item_06.m_txt ) * 10000); // 收盘集合竞价时段下限价 // 10000
		//snapshot_stock.m_CASUpperPrice = (uint32_t)(atof( market_data.m_item_07.m_txt ) * 10000); // 收盘集合竞价时段上限价 // 10000
		//std::string ord_imb_direction = basicx::StringTrim( market_data.m_item_08.m_txt, " " );
		//memcpy( snapshot_stock.m_OrdImbDirection, ord_imb_direction.c_str(), ord_imb_direction.length() ); // 不能配对买卖盘方向 // C1
		//snapshot_stock.m_OrdImbQty = atoi( market_data.m_item_09.m_txt ); // 不能配对买卖盘量

		memcpy( snapshot_stock.m_code, market_data.m_item_02.m_txt, 5 ); // 证券代码
		std::string symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( market_data.m_item_03.m_txt ) ), "　" ) ); // "　"
		memcpy( snapshot_stock.m_name, symbol.c_str(), symbol.length() ); // 证券名称 // 中文名称
		strcpy_s( snapshot_stock.m_type, "H" ); // 证券类型
		strcpy_s( snapshot_stock.m_market, "HK" ); // 证券市场
		strcpy_s( snapshot_stock.m_status, "N" ); // 证券状态 // "N"、"S"、"X"
		//snapshot_stock.m_last; // 最新价 // 10000
		//snapshot_stock.m_nominal; // 按盘价 // 10000
		//snapshot_stock.m_high; // 最高价 // 10000
		//snapshot_stock.m_low; // 最低价 // 10000
		//snapshot_stock.m_close; // 收盘价 // 10000
		//snapshot_stock.m_pre_close; // 昨收价 // 10000
		//snapshot_stock.m_volume; // 成交量
		//snapshot_stock.m_turnover; // 成交额 // 10000
		snapshot_stock.m_ask_price[0] = (uint32_t)( atof( market_data.m_item_05.m_txt ) * 10000 ); // 收盘集合竞价时段参考价 // 10000
		//snapshot_stock.m_ask_price[1-9]; // 申卖价 // 10000
		//snapshot_stock.m_ask_volume[10]; // 申卖量
		snapshot_stock.m_bid_price[0] = (uint32_t)( atof( market_data.m_item_05.m_txt ) * 10000 ); // 收盘集合竞价时段参考价 // 10000
		//snapshot_stock.m_bid_price[1-9]; // 申买价 // 10000
		//snapshot_stock.m_bid_volume[10]; // 申买量
		std::string ord_imb_direction = basicx::StringTrim( market_data.m_item_08.m_txt, " " );
		if( "N" == ord_imb_direction ) { // 买卖盘量相等
		}
		else if( "B" == ord_imb_direction ) { // 买盘比卖盘多
			snapshot_stock.m_bid_volume[0] = atoi( market_data.m_item_09.m_txt ); // 不能配对买卖盘量
		}
		else if( "S" == ord_imb_direction ) { // 卖盘比买盘多
			snapshot_stock.m_ask_volume[0] = atoi( market_data.m_item_09.m_txt ); // 不能配对买卖盘量
		}
		snapshot_stock.m_high_limit = (uint32_t)( atof( market_data.m_item_07.m_txt ) * 10000 ); // 收盘集合竞价时段上限价 // 10000
		snapshot_stock.m_low_limit = (uint32_t)( atof( market_data.m_item_06.m_txt ) * 10000 ); // 收盘集合竞价时段下限价 // 10000
		//snapshot_stock.m_trade_count; // 成交笔数 // 无
		//snapshot_stock.m_vcm_start_time; // 冷静期开始时间 // HHMMSS000 精度：秒
		//snapshot_stock.m_vcm_end_time; // 冷静期结束时间 // HHMMSS000 精度：秒
		std::string timestamp = market_data.m_item_10.m_txt;
		if( timestamp != "" ) { // HH:MM:SS.000
			int32_t md_hour = atoi( timestamp.substr( 0, 2 ).c_str() );
			int32_t md_minute = atoi( timestamp.substr( 3, 2 ).c_str() );
			int32_t md_second = atoi( timestamp.substr( 6, 2 ).c_str() );
			snapshot_stock.m_quote_time = md_hour * 10000000 + md_minute * 100000 + md_second * 1000; // 行情时间 // HHMMSS000 精度：秒
		}
	}
	catch( ... ) {
		std::string log_info = "处理 MD405 数据发生未知异常！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

// 预设 QuoterHGT 函数实现

QuoterHGT::QuoterHGT()
	: basicx::Plugins_X( PLUGIN_NAME )
	, m_quoter_hgt_p( nullptr ) {
	try {
		m_quoter_hgt_p = new QuoterHGT_P();
	}
	catch( ... ) {
	}
}

QuoterHGT::~QuoterHGT() {
	if( m_quoter_hgt_p != nullptr ) {
		delete m_quoter_hgt_p;
		m_quoter_hgt_p = nullptr;
	}
}

bool QuoterHGT::Initialize() {
	return m_quoter_hgt_p->Initialize();
}

bool QuoterHGT::InitializeExt() {
	return m_quoter_hgt_p->InitializeExt();
}

bool QuoterHGT::StartPlugin() {
	return m_quoter_hgt_p->StartPlugin();
}

bool QuoterHGT::IsPluginRun() {
	return m_quoter_hgt_p->IsPluginRun();
}

bool QuoterHGT::StopPlugin() {
	return m_quoter_hgt_p->StopPlugin();
}

bool QuoterHGT::UninitializeExt() {
	return m_quoter_hgt_p->UninitializeExt();
}

bool QuoterHGT::Uninitialize() {
	return m_quoter_hgt_p->Uninitialize();
}

bool QuoterHGT::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	return m_quoter_hgt_p->AssignTask( task_id, identity, code, data );
}

// 自定义 QuoterHGT 函数实现
