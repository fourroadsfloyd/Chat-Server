#include "ChatGrpcClient.h"
#include "ConfigMgr.h"

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req)
{
	return AddFriendRsp();
}

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
