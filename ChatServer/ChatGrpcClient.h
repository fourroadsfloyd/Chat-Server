#pragma once

#include "const.h"
#include "GrpcStubPool.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::ChatService;
using message::GetChatServerRsp;

using message::LoginReq;
using message::LoginRsp;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::TextChatData;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;


class ChatGrpcClient : public Singleton<ChatGrpcClient> {
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient() = default;

	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);

private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<GrpcStubPool<ChatService>>> _pools;
};

