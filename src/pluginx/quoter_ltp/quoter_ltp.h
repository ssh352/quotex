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

#ifndef QUOTER_LTP_QUOTER_LTP_H
#define QUOTER_LTP_QUOTER_LTP_H

#include <string>

#include <plugins/plugins.h>

#include "../../global/compile.h"

class QuoterLTP_P;

class QUOTER_LTP_EXPIMP QuoterLTP : public basicx::Plugins_X
{
public:
	QuoterLTP();
	~QuoterLTP();

public:
	virtual bool Initialize() override;
	virtual bool InitializeExt() override;
	virtual bool StartPlugin() override;
	virtual bool IsPluginRun() override;
	virtual bool StopPlugin() override;
	virtual bool UninitializeExt() override;
	virtual bool Uninitialize() override;
	virtual bool AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) override;

private:
	QuoterLTP_P* m_quoter_ltp_p;

// 自定义成员函数和变量

};

#endif // QUOTER_LTP_QUOTER_LTP_H
