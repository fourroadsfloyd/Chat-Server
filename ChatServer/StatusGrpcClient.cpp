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
    ClientContext context;
    GetChatServerRsp reply;
    GetChatServerReq request;
    request.set_uid(uid);
    auto stub = pool_->GetConnection();
    Status status = stub->GetChatServer(&context, request, &reply);

    Defer defer([&stub, this]() {   //RAII设计，保证资源一定释放
            pool_->returnConnection(std::move(stub));
        });

    if (status.ok()) {
        return reply;
    }
    else {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
    ClientContext context;
    LoginRsp reply;
    LoginReq request;
    request.set_uid(uid);
    request.set_token(token);

    auto stub = pool_->GetConnection();
    Status status = stub->Login(&context, request, &reply);
    Defer defer([&stub, this]() {
        pool_->returnConnection(std::move(stub));
        });
    if (status.ok()) {
        return reply;
    }
    else {
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
}

