#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"

VerifyGrpcClient::VerifyGrpcClient()
{
    auto& gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["VerifyServer"]["host"];
    std::string port = gCfgMgr["VerifyServer"]["port"];
    _pool.reset(new GrpcStubPool<VerifyService>(5, host, port));
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email)
{
    ClientContext context;
    GetVerifyRsp reply;
    GetVerifyReq request;
    request.set_email(email);

    auto stub = _pool->GetConnection();

	if (!stub)
    {
        AsyncLog::getInstance().log(LogLevel::LogERROR, "stub is nullptr");
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

    Status status = stub->GetVerifyCode(&context, request, &reply);

    if(!status.ok()) 
    {
        AsyncLog::getInstance().log(LogLevel::LogERROR, "status not ok");
        reply.set_error(ErrorCodes::RPCFailed);
    }

    _pool->returnConnection(std::move(stub));
    return reply;
}



