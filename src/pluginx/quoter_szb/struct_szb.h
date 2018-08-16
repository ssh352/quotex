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

#ifndef QUOTER_SZB_STRUCT_SZB_H
#define QUOTER_SZB_STRUCT_SZB_H

#include <map>
#include <list>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

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

#include <common/assist.h>
#include <network/server.h>

#include "define_szb.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_SGT_V1

#ifdef SNAPSHOT_STOCK_SGT_V1
    #define SNAPSHOT_STOCK_SGT_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_SGT_V2
    #define SNAPSHOT_STOCK_SGT_VERSION '2'
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
	std::string m_username;
	std::string m_password;
	int32_t m_timeout;
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

struct MDGW_Header
{
	uint32_t m_MsgType; // ��Ϣ����
	uint32_t m_BodyLength; // ��Ϣ�峤��
};

struct MDGW_Tailer
{
	uint32_t m_Checksum; // У���
};

struct MDGW_Logon
{
	MDGW_Header m_Header; // MsgType = 1
	char m_SenderCompID[20]; // ���ͷ�����
	char m_TargetCompID[20]; // ���շ�����
	int32_t m_HeartBtInt; // �����������λ���룬�û�����ϵͳ��½ʱ�ṩ����������
	char m_Password[16]; // ����
	char m_DefaultApplVerID[32]; // ������Э��汾
	MDGW_Tailer mdgw_tailer;
};

struct MDGW_Logon_Body
{
	char m_SenderCompID[20]; // ���ͷ�����
	char m_TargetCompID[20]; // ���շ�����
	int32_t m_HeartBtInt; // �����������λ���룬�û�����ϵͳ��½ʱ�ṩ����������
	char m_Password[16]; // ����
	char m_DefaultApplVerID[32]; // ������Э��汾
};

struct MDGW_Logout
{
	MDGW_Header m_Header; // MsgType = 2
	int32_t m_SessionStatus; // �˳�ʱ�ĻỰ״̬
	char m_Text[200]; // �ı���ע��ԭ��Ľ�һ������˵��
	MDGW_Tailer mdgw_tailer;
};

struct MDGW_Logout_Body
{
	int32_t m_SessionStatus; // �˳�ʱ�ĻỰ״̬
	char m_Text[200]; // �ı���ע��ԭ��Ľ�һ������˵��
};

struct MDGW_Heartbeat
{
	MDGW_Header m_Header; // MsgType = 3��BodyLength = 0
	MDGW_Tailer mdgw_tailer;
};

#define DEF_MDGW_MSG_TYPE_LOGON 1
#define DEF_MDGW_MSG_TYPE_LOGOUT 2
#define DEF_MDGW_MSG_TYPE_HEARTBEAT 3
#define DEF_MDGW_MSG_TYPE_SNAPSHOT_STOCK_SGT 306311 // �۹�ʵʱ����

uint32_t g_size_mdgw_header = sizeof( MDGW_Header ); // 8
uint32_t g_size_mdgw_tailer = sizeof( MDGW_Tailer ); // 4
uint32_t g_size_mdgw_logon_body = sizeof( MDGW_Logon_Body ); // 92
uint32_t g_size_mdgw_logout_body = sizeof( MDGW_Logout_Body ); // 204
uint32_t g_size_mdgw_heartbeat_body = 0; // �սṹ�峤��Ϊ 1

uint32_t MDGW_GenerateCheckSum( char* buffer, uint32_t bufer_length ) {
	uint32_t check_sum = 0;
	for( size_t i = 0; i < bufer_length; ++i ) {
		check_sum += (uint32_t)buffer[i];
	}
	return ( check_sum % 256 );
}

std::string MDGW_SessionStatus( int32_t status ) {
	std::string result;
	switch( status ) {
	case 0: return result = "�Ự��Ծ";
	case 1: return result = "�Ự�����Ѹ���";
	case 2: return result = "�����ڵĻỰ����";
	case 3: return result = "�»Ự������Ϲ淶";
	case 4: return result = "�Ự�˵����";
	case 5: return result = "���Ϸ����û��������";
	case 6: return result = "�˻�����";
	case 7: return result = "��ǰʱ�䲻�����¼";
	case 8: return result = "�������";
	case 9: return result = "�յ��� MsgSeqNum(34) ̫С";
	case 10: return result = "�յ��� NextExpectedMsgSeqNum(789) ̫��";
	case 101: return result = "����";
	case 102: return result = "��Ч��Ϣ";
	default: return result = "δ֪״̬";
	}
}

#ifdef SNAPSHOT_STOCK_SGT_V1
struct SnapshotStock_SGT // ���б������ᱻ��ֵ
{
	char m_MDStreamID[6];       // ������������           C5     // MD401��MD404��MD405
	char m_SecurityID[6];       // ֤ȯ����               C5     // MD401��MD404��MD405
	char m_Symbol[33];          // ����֤ȯ���           C32    // MD401��MD404��MD405
	char m_SymbolEn[17];        // Ӣ��֤ȯ���           C15/16 // MD401��MD404��MD405

	int64_t m_TradeVolume;      // �ɽ�����               N16    // MD401
	int64_t m_TotalValueTraded; // �ɽ����               N16(3) // MD401 // 10000
	uint32_t m_PreClosePx;      // �������̼�             N11(3) // MD401 // 10000
	uint32_t m_NominalPrice;    // ���̼�                 N11(3) // MD401 // 10000
	uint32_t m_HighPrice;       // ��߼�                 N11(3) // MD401 // 10000
	uint32_t m_LowPrice;        // ��ͼ�                 N11(3) // MD401 // 10000
	uint32_t m_TradePrice;      // ���¼�                 N11(3) // MD401 // 10000
	uint32_t m_BuyPrice1;       // �����һ               N11(3) // MD401 // 10000
	int32_t m_BuyVolume1;       // ������һ               N12    // MD401
	uint32_t m_SellPrice1;      // ������һ               N11(3) // MD401 // 10000
	int32_t m_SellVolume1;      // ������һ               N12    // MD401
	int32_t m_SecTradingStatus; // ֤ȯ����״̬           C8     // MD401

	int32_t m_VCMStartTime;     // �е����ƿ�ʼʱ��       C8     // MD404 // HHMMSS000 ���ȣ���
	int32_t m_VCMEndTime;       // �е����ƽ���ʱ��       C8     // MD404 // HHMMSS000 ���ȣ���
	uint32_t m_VCMRefPrice;     // �е����Ʋο���         N11(3) // MD404 // 10000
	uint32_t m_VCMLowerPrice;   // �е��������޼�         N11(3) // MD404 // 10000
	uint32_t m_VCMUpperPrice;   // �е��������޼�         N11(3) // MD404 // 10000

	uint32_t m_CASRefPrice;     // ���̼��Ͼ���ʱ�βο��� N11(3) // MD405 // 10000
	uint32_t m_CASLowerPrice;   // ���̼��Ͼ���ʱ�����޼� N11(3) // MD405 // 10000
	uint32_t m_CASUpperPrice;   // ���̼��Ͼ���ʱ�����޼� N11(3) // MD405 // 10000
	char m_OrdImbDirection[2];  // ������������̷���     C1     // MD405
	int32_t m_OrdImbQty;        // ���������������       N12    // MD405

	int32_t m_QuoteTime;        // ����ʱ�� // HHMMSS000 ���ȣ���
	int32_t m_LocalTime;        // ����ʱ�� // HHMMSSmmm ���ȣ�����
	uint32_t m_LocalIndex;      // �������
};
#endif

#ifdef SNAPSHOT_STOCK_SGT_V2
struct SnapshotStock_SGT // ���б������ᱻ��ֵ
{
};
#endif

#pragma pack( pop )

#endif // QUOTER_SZB_STRUCT_SZB_H
