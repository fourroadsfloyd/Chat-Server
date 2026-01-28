#pragma once

#include "const.h"
#include "GrpcStubPool.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;


class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;
public:

    GetVerifyRsp GetVerifyCode(std::string email);
    
private:
    VerifyGrpcClient();

    std::unique_ptr<GrpcStubPool<VerifyService>> _pool;

};