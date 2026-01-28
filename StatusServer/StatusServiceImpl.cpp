#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

std::string generate_unique_string() {
    // 创建UUID对象
    boost::uuids::uuid uuid = boost::uuids::random_generator()();

    // 将UUID转换为字符串
    std::string unique_string = to_string(uuid);

    return unique_string;
}

StatusServiceImpl::StatusServiceImpl() :_server_index(0)
{
	auto& cfg = ConfigMgr::GetInstance();

	auto server_list = cfg["ChatServers"]["name"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;

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

		ChatServer server;
		server.port = cfg[word]["port"];
		server.host = cfg[word]["host"];
		server.name = cfg[word]["name"]; 
		_servers[server.name] = server;
	}
	
}

//向GateServer返回一个ChatServer的地址和端口
Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
	AsyncLog::getInstance().log(LogLevel::LogINFO, "this is StatusServer GetChatServer");

	const auto& server = getChatServer();
	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());	//将uid和token存入redis，以便ChatServer验证使用

	std::cout << "host is " << server.host << ", port is " << server.port << std::endl;

	AsyncLog::getInstance().log(LogLevel::LogINFO, "StatusServer will return Status::OK");

	return Status::OK;
}


ChatServer StatusServiceImpl::getChatServer() 
{
	AsyncLog::getInstance().log(LogLevel::LogINFO, 
		"StatusServer start select ChatServer");

	std::lock_guard<std::mutex> guard(_server_mtx);
	auto minServer = _servers.begin()->second;

	auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, minServer.name);
	if (count_str.empty()) 
	{
		//不存在则默认设置为最大
		minServer.con_count = INT_MAX;	//服务器没有找到，可能没有开启，将连接数设为最大
	}
	else 
	{
		minServer.con_count = std::stoi(count_str);
	}


	// 使用范围基于for循环
	for (auto& server : _servers) 
	{
		
		if (server.second.name == minServer.name) 
		{
			continue;
		}

		auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
		if (count_str.empty()) 
		{
			server.second.con_count = INT_MAX;
		}
		else 
		{
			server.second.con_count = std::stoi(count_str);
		}

		if (server.second.con_count < minServer.con_count) 
		{
			minServer = server.second;
		}
	}

	std::cout << "port is " << minServer.port << std::endl;

	return minServer;
}

Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* reply)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (success) {
		reply->set_error(ErrorCodes::UidInvalid);
		return Status::OK;
	}

	if (token_value != token) {
		reply->set_error(ErrorCodes::TokenInvalid);
		return Status::OK;
	}

	reply->set_error(ErrorCodes::Success);
	reply->set_uid(uid);
	reply->set_token(token);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	bool flag = RedisMgr::GetInstance()->Set(token_key, token);
	if (!flag)
	{
		AsyncLog::getInstance().log(LogLevel::LogERROR, 
									"StatusServer to redis 插入uid:{} token:{} 失败", 
									uid, 
									token);
	}
	AsyncLog::getInstance().log(LogLevel::LogINFO, "StatusServer insert uid-token to redis successfully");
}
