#include "CServer.h"
#include "HttpConnect.h"
#include "AsioIOServicePool.h"

CServer::CServer(net::io_context& ioc, unsigned short& port)
	: _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
	  _ioc(ioc)
{
}

void CServer::Start()
{
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
    std::shared_ptr<HttpConnect> new_con = std::make_shared<HttpConnect>(io_context);
    _acceptor.async_accept(new_con->GetSocket(), [self, new_con](beast::error_code ec) {
            try {
                //出错则放弃这个连接，继续监听新链接
                if (ec) {
                    self->Start();
                    return;
                }

                //处理新链接，创建HpptConnection类管理新连接
                new_con->Start();
                //继续监听
                self->Start();
            }
            catch (std::exception& exp) {
                self->Start();
            }
        });
}

