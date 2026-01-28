#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "MsgNode.h"
#include "const.h"

using namespace std;

using boost::asio::ip::tcp;
class CServer;

class CSession : public std::enable_shared_from_this<CSession> {
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession() = default;

	tcp::socket& GetSocket();
	std::string& GetSessionId();
	void Start();

	void Send(char* msg, short max_length, short msgid);
	void Send(std::string msg, short msgid);

	void Close();

	void SetUserId(int uid);

	int GetUserId();

private:
	void HandleReadHead(const boost::system::error_code& error, size_t  bytes_transferred, std::shared_ptr<CSession> shared_self);

	void HandleReadMsg(const boost::system::error_code& error, size_t  bytes_transferred, std::shared_ptr<CSession> shared_self);

	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);

private:
	tcp::socket _socket;
	std::string _session_id;
	char _data[MAX_LENGTH];
	CServer* _server;
	bool _b_close;
	std::queue<shared_ptr<MsgNode> > _send_que;
	std::mutex _send_lock;

	//头部是否解析完成
	bool _b_head_parse;

	//收到的消息结构
	std::shared_ptr<RecvNode> _recv_msg_node;
	
	//收到的头部结构
	std::shared_ptr<MsgNode> _recv_head_node;

	int _user_id;
};

