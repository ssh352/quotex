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

#ifndef QUOTER_LTB_STRUCT_LTB_H
#define QUOTER_LTB_STRUCT_LTB_H

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

#include "define_ltb.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_LTB_V1
#define SNAPSHOT_INDEX_LTB_V1
#define TRANSACTION_LTB_V1

#ifdef SNAPSHOT_STOCK_LTB_V1
    #define SNAPSHOT_STOCK_LTB_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_LTB_V2
    #define SNAPSHOT_STOCK_LTB_VERSION '2'
#endif

#ifdef SNAPSHOT_INDEX_LTB_V1
    #define SNAPSHOT_INDEX_LTB_VERSION '1'
#endif
#ifdef SNAPSHOT_INDEX_LTB_V2
    #define SNAPSHOT_INDEX_LTB_VERSION '2'
#endif

#ifdef TRANSACTION_LTB_V1
    #define TRANSACTION_LTB_VERSION '1'
#endif
#ifdef TRANSACTION_LTB_V2
    #define TRANSACTION_LTB_VERSION '2'
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
	int32_t m_port;
	int32_t m_need_dump;
	std::string m_dump_path;
	int32_t m_data_compress;
	int32_t m_data_encode;
	int32_t m_dump_time;
	int32_t m_init_time;
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

struct Contract // ��֤������ֵ
{
	std::string m_code;
	std::string m_type;
	std::string m_market;

	Contract( std::string code, std::string type, std::string market )
		: m_code( code )
		, m_type( type )
		, m_market( market ) {
	}
};

#ifdef SNAPSHOT_STOCK_LTB_V1
struct SnapshotStock_LTB // ���б������ᱻ��ֵ
{
	char m_code[9]; // ֤ȯ����
	char m_name[24]; // ֤ȯ���� // ��
	char m_type[6]; // ֤ȯ���� // ����ָ���������Ϊ��
	char m_market[6]; // ֤ȯ�г� // SSE��SZE
	char m_status[2]; // ֤ȯ״̬ // 'N'
	uint32_t m_last; // ���¼� // 10000
	uint32_t m_open; // ���̼� // 10000
	uint32_t m_high; // ��߼� // 10000
	uint32_t m_low; // ��ͼ� // 10000
	uint32_t m_close; // ���̼� // 10000
	uint32_t m_pre_close; // ���ռ� // 10000 // ��
	int64_t m_volume; // �ɽ���
	int64_t m_turnover; // �ɽ��� // 10000
	uint32_t m_ask_price[10]; // ������ // 10000
	int32_t m_ask_volume[10]; // ������
	uint32_t m_bid_price[10]; // ����� // 10000
	int32_t m_bid_volume[10]; // ������
	uint32_t m_high_limit; // ��ͣ�� // 10000 // ��
	uint32_t m_low_limit; // ��ͣ�� // 10000 // ��
	int64_t m_open_interest; // �ֲ���
	int32_t m_iopv; // ����ֵ // 10000
	int32_t m_trade_count; // �ɽ�����
	uint32_t m_yield_to_maturity; // ���������� // 10000
	uint32_t m_auction_price; // ��̬�ο��۸� // 10000
	int32_t m_bid_price_level; // ������
	int32_t m_offer_price_level; // �������
	int32_t m_total_bid_volume; // ��������
	int32_t m_total_offer_volume; // ��������
	uint32_t m_weighted_avg_bid_price; // �����Ȩ���� // 10000
	uint32_t m_weighted_avg_offer_price; // ������Ȩ���� // 10000
	uint32_t m_alt_weighted_avg_bid_price; // ծȯ�����Ȩ���� // 10000
	uint32_t m_alt_weighted_avg_offer_price; // ծȯ������Ȩ���� // 10000
	char m_trading_phase[2]; // ���׽׶�
	char m_open_restriction[2]; // ��������
	int32_t	m_quote_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; //�������
	// double BidCount1; // �������һ
	// double OfferCount1; // ��������һ
	// double BidCount2; // ���������
	// double OfferCount2; // ����������
	// double BidCount3; // ���������
	// double OfferCount3; // ����������
	// double BidCount4; // ���������
	// double OfferCount4; // ����������
	// double BidCount5; // ���������
	// double OfferCount5; // ����������
	// double BidCount6; // ���������
	// double OfferCount6; // ����������
	// double BidCount7; // ���������
	// double OfferCount7; // ����������
	// double BidCount8; // ���������
	// double OfferCount8; // ����������
	// double BidCount9; // ���������
	// double OfferCount9; // ����������
	// double BidCountA; // �������ʮ
	// double OfferCountA; // ��������ʮ
};
#endif

#ifdef SNAPSHOT_STOCK_LTB_V2
struct SnapshotStock_LTB // ���б������ᱻ��ֵ
{
};
#endif

#ifdef SNAPSHOT_INDEX_LTB_V1
struct SnapshotIndex_LTB // ���б������ᱻ��ֵ
{
	char m_code[9]; // ָ������
	char m_name[24]; // ָ������ // ��
	char m_type[6]; // ָ������ // ����ָ���������Ϊ��
	char m_market[6]; // ָ���г� // SSE��SZE
	char m_status[2]; // ָ��״̬ // 'N'
	int32_t m_last; // ����ָ�� // 10000
	int32_t m_open; // ����ָ�� // 10000
	int32_t m_high; // ���ָ�� // 10000
	int32_t m_low; // ���ָ�� // 10000
	int32_t m_close; // ����ָ�� // 10000
	int32_t m_pre_close; // ����ָ�� // 10000 // ��
	int64_t	m_volume; // �ɽ���
	int64_t m_turnover; // �ɽ��� // 10000
	int32_t	m_quote_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; // �������
};
#endif

#ifdef SNAPSHOT_INDEX_LTB_V2
struct SnapshotIndex_LTB // ���б������ᱻ��ֵ
{
};
#endif

#ifdef TRANSACTION_LTB_V1
struct Transaction_LTB // ���б������ᱻ��ֵ
{
	char m_code[9]; // ֤ȯ����
	char m_name[24]; // ֤ȯ���� // ��
	char m_type[6]; // ֤ȯ���� // ����ָ���������Ϊ��
	char m_market[6]; // ֤ȯ�г� // SSE��SZE
	int32_t m_index; // �ɽ����
	uint32_t m_price; // �ɽ��� // 10000
	int32_t m_volume; // �ɽ���
	int64_t m_turnover; // �ɽ��� // 10000
	int32_t m_trade_group_id; // �ɽ���
	int32_t m_buy_index; // ��ί�����
	int32_t m_sell_index; // ����ί�����
	char m_order_kind[2]; // ��������
	char m_function_code[2]; // ������
	int32_t	m_trans_time; // �ɽ�ʱ�� // HHMMSSmmm ���ȣ�����
	int32_t	m_local_time; // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_local_index; // �������
};
#endif

#ifdef TRANSACTION_LTB_V2
struct Transaction_LTB // ���б������ᱻ��ֵ
{
};
#endif

struct CL2FAST_MD // �ӿڶ���
{
	double  LastPrice;                // ���¼�
	double  OpenPrice;                // ���̼�
	double  ClosePrice;               // ���̼�
	double  HighPrice;                // ��߼�
	double  LowPrice;                 // ��ͼ�
	double  TotalTradeVolume;         // �ɽ�����
	double  TotalTradeValue;          // �ɽ����
	double  TradeCount;               // �ɽ�����
	double  OpenInterest;             // �ֲ���
	double  IOPV;                     // ����ֵ
	double  YieldToMaturity;          // ����������
	double  AuctionPrice;             // ��̬�ο��۸�
	double  TotalBidVolume;           // ��������
	double  WeightedAvgBidPrice;      // �����Ȩ����
	double  AltWeightedAvgBidPrice;   // ծȯ�����Ȩ����
	double  TotalOfferVolume;         // ��������
	double  WeightedAvgOfferPrice;    // ������Ȩ����
	double  AltWeightedAvgOfferPrice; // ծȯ������Ȩ����
	int32_t BidPriceLevel;            // ������
	int32_t OfferPriceLevel;          // �������
	double  BidPrice1;                // �����һ
	double  BidVolume1;               // ������һ
	double  BidCount1;                // �������һ
	double  OfferPrice1;              // ������һ
	double  OfferVolume1;             // ������һ
	double  OfferCount1;              // ��������һ
	double  BidPrice2;                // ����۶�
	double  BidVolume2;               // ��������
	double  BidCount2;                // ���������
	double  OfferPrice2;              // �����۶�
	double  OfferVolume2;             // ��������
	double  OfferCount2;              // ����������
	double  BidPrice3;                // �������
	double  BidVolume3;               // ��������
	double  BidCount3;                // ���������
	double  OfferPrice3;              // ��������
	double  OfferVolume3;             // ��������
	double  OfferCount3;              // ����������
	double  BidPrice4;                // �������
	double  BidVolume4;               // ��������
	double  BidCount4;                // ���������
	double  OfferPrice4;              // ��������
	double  OfferVolume4;             // ��������
	double  OfferCount4;              // ����������
	double  BidPrice5;                // �������
	double  BidVolume5;               // ��������
	double  BidCount5;                // ���������
	double  OfferPrice5;              // ��������
	double  OfferVolume5;             // ��������
	double  OfferCount5;              // ����������
	double  BidPrice6;                // �������
	double  BidVolume6;               // ��������
	double  BidCount6;                // ���������
	double  OfferPrice6;              // ��������
	double  OfferVolume6;             // ��������
	double  OfferCount6;              // ����������
	double  BidPrice7;                // �������
	double  BidVolume7;               // ��������
	double  BidCount7;                // ���������
	double  OfferPrice7;              // ��������
	double  OfferVolume7;             // ��������
	double  OfferCount7;              // ����������
	double  BidPrice8;                // ����۰�
	double  BidVolume8;               // ��������
	double  BidCount8;                // ���������
	double  OfferPrice8;              // �����۰�
	double  OfferVolume8;             // ��������
	double  OfferCount8;              // ����������
	double  BidPrice9;                // ����۾�
	double  BidVolume9;               // ��������
	double  BidCount9;                // ���������
	double  OfferPrice9;              // �����۾�
	double  OfferVolume9;             // ��������
	double  OfferCount9;              // ����������
	double  BidPriceA;                // �����ʮ
	double  BidVolumeA;               // ������ʮ
	double  BidCountA;                // �������ʮ
	double  OfferPriceA;              // ������ʮ
	double  OfferVolumeA;             // ������ʮ
	double  OfferCountA;              // ��������ʮ
	char    ExchangeID[4];            // ����������
	char    InstrumentID[9];          // ֤ȯ����
	char    TimeStamp[13];            // ʱ���
	char    TradingPhase;             // ���׽׶�
	char    OpenRestriction;          // ��������
};

struct CL2FAST_INDEX // �ӿڶ���
{
	double  LastIndex;                // ����ָ��
	double  OpenIndex;                // ����ָ��
	double  CloseIndex;               // ����ָ��
	double  HighIndex;                // ���ָ��
	double  LowIndex;                 // ���ָ��
	double  TurnOver;                 // �ɽ����
	double  TotalVolume;              // �ɽ�����
	char    ExchangeID[4];            // ����������
	char    InstrumentID[9];          // ָ������
	char    TimeStamp[13];            // ʱ���
};

struct CL2FAST_ORDER // �ӿڶ���
{
	double  Price;                    // ί�м۸�
	double  Volume;                   // ί������
	int32_t OrderGroupID;             // ί����
	int32_t OrderIndex;               // ί�����
	char    OrderKind;                // ��������
	char    FunctionCode;             // ������
	char    ExchangeID[4];            // ����������
	char    InstrumentID[9];          // ��Լ����
	char    OrderTime[13];            // ʱ���
};

struct CL2FAST_TRADE // �ӿڶ���
{
	double  Price;                    // �ɽ��۸�
	double  Volume;                   // �ɽ�����
	int32_t TradeGroupID;             // �ɽ���
	int32_t TradeIndex;               // �ɽ����
	int32_t BuyIndex;                 // ��ί�����
	int32_t SellIndex;                // ����ί�����
	char    OrderKind;                // ��������
	char    FunctionCode;             // ������
	char    ExchangeID[4];            // ����������
	char    InstrumentID[9];          // ��Լ����
	char    TradeTime[13];            // ʱ���
};

#pragma pack( pop )

#endif // QUOTER_LTB_STRUCT_LTB_H
