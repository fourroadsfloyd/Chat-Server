#pragma once
#include "const.h"
#include "CSession.h"

class CServer : public std::enable_shared_from_this<CServer> {
public:
	CServer(net::io_context& ioc, unsigned short& port);

    void ClearSession(std::string sessionId);

private:
    void HandleAccept(std::shared_ptr<CSession>, const boost::system::error_code& error);
    void StartAccept();

private:
    tcp::acceptor  _acceptor;
    net::io_context& _ioc;
    short _port;
	std::map<std::string, std::shared_ptr<CSession>> _sessions; //使用session id作为key, value为session指针,断开连接时根据id删除
    std::mutex _mutex;
};

