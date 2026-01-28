#include "StatusGrpcClient.h"
#include "ConfigMgr.h"

StatusGrpcClient::StatusGrpcClient()
{
    auto& gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["StatusServer"]["host"];
    std::string port = gCfgMgr["StatusServer"]["port"];
    pool_.reset(new GrpcStubPool<StatusService>(5, host, port));
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    AsyncLog::getInstance().log(LogLevel::LogINFO,
                                "StatusGrpcClient::GetChatServer: uid is {}",
		                        uid);

    ClientContext context;
    GetChatServerRsp reply;
    GetChatServerReq request;
    request.set_uid(uid);
    auto stub = pool_->GetConnection();

    if(!stub)
    {
        AsyncLog::getInstance().log(LogLevel::LogERROR,
            "StatusGrpcClient::GetChatServer: failed to get stub from pool");
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
	}

    Status status = stub->GetChatServer(&context, request, &reply);

    Defer defer([&stub, this]() {   //RAII设计，保证资源一定释放
            pool_->returnConnection(std::move(stub));
        });

    if (status.ok()) 
    {
        return reply;
    }
    else {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
}

