#include <iostream>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include "const.h"
#include "CSession.h"
#include "CServer.h"
#include "LogicSystem.h"


CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
	_socket(io_context), _server(server), _b_close(false), _b_head_parse(false) 
{
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_session_id = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

tcp::socket& CSession::GetSocket() 
{
	return _socket;
}

std::string& CSession::GetSessionId() 
{
	return _session_id;
}

void CSession::Start() 
{
	::memset(_data, 0, MAX_LENGTH);

	boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
		[this, self = shared_from_this()](const boost::system::error_code& error, size_t bytes_transferred) {
			this->HandleReadHead(error, bytes_transferred, self);
		});
}

void CSession::Send(char* msg, short max_length, short msgid)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));
	if (send_que_size > 0) 
	{
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		[this, self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
			this->HandleWrite(error, self);
		});
}

void CSession::Send(std::string msg, short msgid)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		return;
	}

	_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) 
	{
		return;
	}

	AsyncLog::getInstance().log(LogLevel::LogINFO, "ChatServer send to client msgid={}", msgid);

	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		[this, self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
			this->HandleWrite(error, self);
		});
}

void CSession::Close() 
{
	_socket.close();
	_b_close = true;
}

void CSession::SetUserId(int uid)
{
	_user_id = uid;
}

int CSession::GetUserId()
{
	return _user_id;
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) 
{
	try
	{

		if (!error)
		{
			std::lock_guard<std::mutex> lock(_send_lock);
			//cout << "send data " << _send_que.front()->_data + HEAD_LENGTH << endl;
			//Close();
			_send_que.pop();
			if (!_send_que.empty())
			{
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
					[this, self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
						this->HandleWrite(error, self);
					});
				//std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else
		{
			std::cout << "handle write failed, error is " << error.what() << endl;
			Close();
			_server->ClearSession(_session_id);
		}
	}
	catch(std::exception& e)
	{
		std::cout << "Exception code : " << e.what() << std::endl;
	}
}

void CSession::HandleReadHead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self)
{
	if (!error)
	{
		// 1. 严格校验头部长度
		if (bytes_transferred != HEAD_TOTAL_LEN)
		{
			std::cout << "[" << _session_id << "] HEAD length error: expected "
				<< HEAD_TOTAL_LEN << ", got " << bytes_transferred << std::endl;
			Close();
			_server->ClearSession(_session_id);
			return;
		}

		short MSG_ID = 0;
		memcpy(&MSG_ID, _recv_head_node->_data, HEAD_ID_LEN);
		MSG_ID = boost::asio::detail::socket_ops::network_to_host_short(MSG_ID);

		short MSG_LEN = 0;
		memcpy(&MSG_LEN, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
		MSG_LEN = boost::asio::detail::socket_ops::network_to_host_short(MSG_LEN);

		// 2. 检查 <= 0 和超大值
		if (MSG_LEN <= 0 || MSG_LEN > MAX_LENGTH)
		{
			std::cout << "[" << _session_id << "] Invalid MSG_LEN=" << MSG_LEN
				<< " (MSG_ID=" << MSG_ID << ")" << std::endl;
			Close();
			_server->ClearSession(_session_id);
			return;
		}

		_recv_msg_node = make_shared<RecvNode>(MSG_LEN, MSG_ID);

		// 3. （可选）设置读超时
		// _read_timer.expires_after(std::chrono::seconds(30));
		// _read_timer.async_wait([this, self](auto ec){ if(!ec) Close(); });

		boost::asio::async_read(_socket, boost::asio::buffer(_recv_msg_node->_data, MSG_LEN),
			[this, self = shared_from_this()](const boost::system::error_code& error, size_t bytes_transferred) {
				this->HandleReadMsg(error, bytes_transferred, self);
			});
	}
	else
	{
		std::cout << "[" << _session_id << "] Read head failed: " << error.what() << std::endl;
		Close();
		_server->ClearSession(_session_id);
	}
}

void CSession::HandleReadMsg(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self)
{
	if (!error)
	{
		// 4. 验证实际读到的长度
		if (bytes_transferred != static_cast<size_t>(_recv_msg_node->_total_len))
		{
			Close();
			_server->ClearSession(_session_id);
			return;
		}

		LogicSystem::GetInstance()->PostMsgToQue(std::make_shared<LogicNode>(shared_self, _recv_msg_node));

		AsyncLog::getInstance().log(LogLevel::LogINFO, "client req to ChatServer MQ");

		_recv_head_node->Clear();
		boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
			[this, self = shared_from_this()](const boost::system::error_code& error, size_t bytes_transferred) {
				this->HandleReadHead(error, bytes_transferred, self);
			});
	}
	else
	{
		Close();
		_server->ClearSession(_session_id);
	}
}
