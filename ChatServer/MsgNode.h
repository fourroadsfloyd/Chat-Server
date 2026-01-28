#pragma once

#include "const.h"

using namespace std;
using boost::asio::ip::tcp;

class MsgNode {
public:
	MsgNode(short max_len) :_total_len(max_len), _cur_len(0), _msg_id(-1)
	{
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}

	MsgNode(short max_len, short msg_id) :_total_len(max_len), _cur_len(0), _msg_id(msg_id) 
	{
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}

	void Clear() 
	{
		::memset(_data, 0, _total_len);
		_cur_len = 0;
	}

	virtual ~MsgNode()
	{
		delete[] _data;
	}

	short _cur_len;
	short _total_len;
	char* _data;
	short _msg_id;
};

class RecvNode :public MsgNode {
public:
	RecvNode(short max_len, short msg_id);
};

class SendNode :public MsgNode {
public:
	SendNode(const char* msg, short max_len, short msg_id);
};

