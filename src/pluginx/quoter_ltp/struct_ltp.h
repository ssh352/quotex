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

#ifndef QUOTER_LTP_STRUCT_LTP_H
#define QUOTER_LTP_STRUCT_LTP_H

#include <map>
#include <list>
#include <vector>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#define ZLIB_WINAPI // ���룬���򱨣��޷��������ⲿ����

#include "zlib.h"

#include <network/server.h>

#include "SecurityFtdcQueryApi.h"
#include "SecurityFtdcL2MDUserApi.h"

#include "define_ltp.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_LTP_V1
#define SNAPSHOT_INDEX_LTP_V1
#define TRANSACTION_LTP_V1

#ifdef SNAPSHOT_STOCK_LTP_V1
    #define SNAPSHOT_STOCK_LTP_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_LTP_V2
    #define SNAPSHOT_STOCK_LTP_VERSION '2'
#endif

#ifdef SNAPSHOT_INDEX_LTP_V1
    #define SNAPSHOT_INDEX_LTP_VERSION '1'
#endif
#ifdef SNAPSHOT_INDEX_LTP_V2
    #define SNAPSHOT_INDEX_LTP_VERSION '2'
#endif

#ifdef TRANSACTION_LTP_V1
    #define TRANSACTION_LTP_VERSION '1'
#endif
#ifdef TRANSACTION_LTP_V2
    #define TRANSACTION_LTP_VERSION '2'
#endif

#pragma pack( push )
#pragma pack( 1 )

struct TaskItem // ��֤������ֵ
{
	int32_t m_task_id;
	int32_t m_identity;
	int32_t m_code;
	std::string m_data;
};

struct Request // ��֤������ֵ
{
	int32_t m_task_id;
	int32_t m_identity;
	int32_t m_code;
	Json::Value m_req_json;
	// ����������л�����
};

struct ConSubOne
{
	boost::mutex m_lock_con_sub_one; // ���ܿ���
	std::map<int32_t, basicx::ConnectInfo*> m_map_identity; // ��������ظ������Ͷ��
};

struct ConSubMan
{
	boost::mutex m_lock_con_sub_all;
	std::map<int32_t, basicx::ConnectInfo*> m_map_con_sub_all;
	boost::mutex m_lock_con_sub_one;
	std::map<std::string, ConSubOne*> m_map_con_sub_one;
	boost::mutex m_lock_one_sub_con_del;
	std::list<ConSubOne*> m_list_one_sub_con_del;
};

struct Config // ��֤������ֵ
{
	std::string m_address;
	std::string m_broker_id;
	std::string m_username;
	std::string m_password;
	std::string m_sub_list_from;
	std::string m_qc_address;
	std::string m_qc_broker_id;
	std::string m_qc_username;
	std::string m_qc_password;
	std::string m_qc_user_product;
	std::string m_qc_auth_code;
	std::string m_symbol_files_folder;
	int32_t m_need_stock;
	int32_t m_need_index;
	int32_t m_need_trans;
	int32_t m_need_dump;
	std::string m_dump_path;
	int32_t m_data_compress;
	int32_t m_data_encode;
	int32_t m_dump_time;
	int32_t m_init_time_01;
	int32_t m_init_time_02;
};

template<typename T>
class QuoteCache
{
public:
	QuoteCache()
		: m_recv_num( 0 )
		, m_dump_num( 0 )
		, m_comp_num( 0 )
		, m_send_num( 0 )
		, m_local_index( 0 )
		, m_folder_path( "" )
		, m_file_path( "" ) {
		m_vec_cache_put = &m_vec_cache_01;
		m_vec_cache_out = &m_vec_cache_02;
	}

public:
	std::atomic<uint32_t> m_recv_num;
	std::atomic<uint32_t> m_dump_num;
	std::atomic<uint32_t> m_comp_num;
	std::atomic<uint32_t> m_send_num;
	std::atomic<uint32_t> m_local_index;
	std::string m_folder_path;
	std::string m_file_path;
	boost::mutex m_lock_cache;
	std::vector<T> m_vec_cache_01;
	std::vector<T> m_vec_cache_02;
	std::vector<T>* m_vec_cache_put;
	std::vector<T>* m_vec_cache_out;
};

#ifdef SNAPSHOT_STOCK_LTP_V1
struct SnapshotStock_LTP // ���б������ᱻ��ֵ
{
	char m_code[8]; // ֤ȯ����
	char m_name[24]; // ֤ȯ����
	char m_type[6]; // ֤ȯ����
	char m_market[6]; // ֤ȯ�г�
	char m_status[2]; // ֤ȯ״̬
	uint32_t m_last; // ���¼� // 10000
	uint32_t m_open; // ���̼� // 10000
	uint32_t m_high; // ��߼� // 10000
	uint32_t m_low; // ��ͼ� // 10000
	uint32_t m_close; // ���̼� // 10000
	uint32_t m_pre_close; // ���ռ� // 10000
	int64_t m_volume; // �ɽ���
	int64_t m_turnover; // �ɽ��� // 10000
	uint32_t m_ask_price[10]; // ������ // 10000
	int32_t m_ask_volume[10]; // ������
	uint32_t m_bid_price[10]; // ����� // 10000
	int32_t m_bid_volume[10]; // ������
	uint32_t m_high_limit; // ��ͣ�� // 10000
	uint32_t m_low_limit; // ��ͣ�� // 10000
	int64_t m_total_bid_vol; // ������
	int64_t m_total_ask_vol; // ������
	uint32_t m_weighted_avg_bid_price; // ��Ȩƽ��ί��۸� // 10000
	uint32_t m_weighted_avg_ask_price; // ��Ȩƽ��ί���۸� // 10000
	int32_t m_trade_count; // �ɽ�����
	int32_t m_iopv; // IOPV��ֵ��ֵ // 10000
	int32_t m_yield_rate; // ���������� // 10000
	int32_t m_pe_rate_1; // ��ӯ�� 1 // 10000
	int32_t m_pe_rate_2; // ��ӯ�� 2 // 10000
	int32_t m_units; // �Ƽ۵�λ
	int32_t	m_quote_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; // �������
	// TradingDay ������ char 9
	// AltWeightedAvgBidPrice ծȯ��Ȩƽ��ί��� double
	// AltWeightedAvgOfferPrice ծȯ��Ȩƽ��ί���۸� double
	// BidPriceLevel ������ int
	// OfferPriceLevel ������� int
};
#endif

#ifdef SNAPSHOT_STOCK_LTP_V2
struct SnapshotStock_LTP // ���б������ᱻ��ֵ
{
};
#endif

#ifdef SNAPSHOT_INDEX_LTP_V1
struct SnapshotIndex_LTP // ���б������ᱻ��ֵ
{
	char m_code[8]; // ָ������
	char m_name[24]; // ָ������
	char m_type[6]; // ָ������
	char m_market[6]; // ָ���г�
	char m_status[2]; // ָ��״̬
	int32_t m_last; // ����ָ�� // 10000
	int32_t m_open; // ����ָ�� // 10000
	int32_t m_high; // ���ָ�� // 10000
	int32_t m_low; // ���ָ�� // 10000
	int32_t m_close; // ����ָ�� // 10000
	int32_t m_pre_close; // ����ָ�� // 10000
	int64_t	m_volume; // �ɽ���
	int64_t m_turnover; // �ɽ��� // 10000
	int32_t	m_quote_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; // �������
	// TradingDay ������ char 9
};
#endif

#ifdef SNAPSHOT_INDEX_LTP_V2
struct SnapshotIndex_LTP // ���б������ᱻ��ֵ
{
};
#endif

#ifdef TRANSACTION_LTP_V1
struct Transaction_LTP // ���б������ᱻ��ֵ
{
	char m_code[8]; // ֤ȯ����
	char m_name[24]; // ֤ȯ����
	char m_type[6]; // ֤ȯ����
	char m_market[6]; // ֤ȯ�г�
	int32_t m_index; // �ɽ����
	uint32_t m_price; // �ɽ��� // 10000
	int32_t m_volume; // �ɽ���
	uint64_t m_turnover; // �ɽ��� // 10000
	int32_t	m_trans_time; // �ɽ�ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; // �������
	// TradeGroupID �ɽ��� int
	// BuyIndex ��ί����� int
	// SellIndex ����ί����� int
	// OrderKind �������� char 2
	// FunctionCode ������ char
};
#endif

#ifdef TRANSACTION_LTP_V2
struct Transaction_LTP // ���б������ᱻ��ֵ
{
};
#endif

#pragma pack( pop )

#endif // QUOTER_LTP_STRUCT_LTP_H
