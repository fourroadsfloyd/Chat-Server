#pragma once

#include "const.h"
#include "GrpcStubPool.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class StatusGrpcClient :public Singleton<StatusGrpcClient>
{
    friend class Singleton<StatusGrpcClient>;
public:
    ~StatusGrpcClient() = default;

    GetChatServerRsp GetChatServer(int uid);

private:
    StatusGrpcClient();

    std::unique_ptr<GrpcStubPool<StatusService>> pool_;
};