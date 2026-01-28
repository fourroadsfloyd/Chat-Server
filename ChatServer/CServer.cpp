#include "CServer.h"
#include "AsioIOServicePool.h"
#include "UserMgr.h"

CServer::CServer(net::io_context& ioc, unsigned short& port)
	: _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
	  _ioc(ioc)
{
    StartAccept();
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error)
{
	if (!error) {
		new_session->Start();
		_sessions.insert({ new_session->GetSessionId(), new_session });
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

void CServer::StartAccept()
{
	auto& new_ioc = AsioIOServicePool::GetInstance()->GetIOService();
	std::shared_ptr<CSession> new_session = make_shared<CSession>(new_ioc, this);
	_acceptor.async_accept(new_session->GetSocket(),
		[this, new_session](const boost::system::error_code& error) {
			this->HandleAccept(new_session, error);
		});
}

void CServer::ClearSession(std::string sessionId)
{
	if (_sessions.find(sessionId) != _sessions.end())
	{
		UserMgr::GetInstance()->RmvUserSession(_sessions[sessionId]->GetUserId());
	}
	_sessions.erase(sessionId);
}
