#include "ChatGrpcClient.h"
#include "ConfigMgr.h"

ChatGrpcClient::ChatGrpcClient()
{
	auto& cfg = ConfigMgr::GetInstance();
	auto server_list = cfg["PeerServer"]["servers"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;					//当存在多个聊天服务器时，需要处理逗号分隔的字符串

	while (std::getline(ss, word, ','))
	{
		words.push_back(word);
	}

	for (auto& word : words)
	{
		if (cfg[word]["name"].empty())
		{
			continue;
		}
		_pools[cfg[word]["name"]] = std::make_unique<GrpcStubPool<ChatService>>(5, cfg[word]["host"], cfg[word]["port"]);
	}
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req)
{
	AddFriendRsp rsp;
	Defer defer([&rsp, &req]() {
		rsp.set_error(ErrorCodes::Success);
		rsp.set_applyuid(req.applyuid());
		rsp.set_touid(req.touid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyAddFriend(&context, req, &rsp);	//rpc调用
	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req) 
{
	AuthFriendRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &req]() {
			rsp.set_fromuid(req.fromuid());
			rsp.set_touid(req.touid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) 
	{
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyAuthFriend(&context, req, &rsp);
	Defer defercon([&stub, this, &pool]() {
			pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) 
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue) 
{

	TextChatMsgRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &req]() {
			rsp.set_fromuid(req.fromuid());
			rsp.set_touid(req.touid());
			for (const auto& text_data : req.textmsgs()) 
			{
				TextChatData* new_msg = rsp.add_textmsgs();
				new_msg->set_msgid(text_data.msgid());
				new_msg->set_msgcontent(text_data.msgcontent());
			}

		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) 
	{
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->GetConnection();
	Status status = stub->NotifyTextChatMsg(&context, req, &rsp);
	Defer defercon([&stub, this, &pool]() {
			pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) 
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}



