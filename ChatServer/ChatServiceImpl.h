#pragma once

#include "const.h"
#include "CServer.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using message::ChatService;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

class ChatServiceImpl final : public ChatService::Service {
public:
	ChatServiceImpl() = default;

	Status NotifyAddFriend(ServerContext* context, const AddFriendReq* request,
		AddFriendRsp* reply) override;

	Status NotifyAuthFriend(ServerContext* context,
		const AuthFriendReq* request, AuthFriendRsp* reply) override;

	Status NotifyTextChatMsg(ServerContext* context,
		const TextChatMsgReq* request, TextChatMsgRsp* reply) override;

	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);


private:
	std::shared_ptr<CServer> _p_server;
};

