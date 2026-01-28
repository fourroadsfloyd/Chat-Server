#include "LogicSystem.h"
#include "LogicNode.h"
#include "CSession.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "UserMgr.h"

LogicSystem::LogicSystem() :_b_stop(false), _p_server(nullptr) 
{
	RegisterCallBacks();
	_worker_thread = std::thread(&LogicSystem::DealMsg, this);	//开启逻辑处理线程
}

LogicSystem::~LogicSystem() 
{
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg) 
{
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);

	//由0变为1则发送通知信号
	if (_msg_que.size() == 1) 
	{
		unique_lk.unlock();
		_consume.notify_one();
	}
}

//void LogicSystem::SetServer(std::shared_ptr<CServer> pserver) {
//	_p_server = pserver;
//}

void LogicSystem::DealMsg()
{
	for (;;)
	{
		std::unique_lock<std::mutex> unique_lk(_mutex);

		//true:退出wait，false:继续等待
		//当被notify时，lamda表达式仍会被执行一次，以确保条件满足，这样可以避免虚假唤醒带来的问题
		_consume.wait(unique_lk, [this]() {
				return !_msg_que.empty() || _b_stop;
			});

		//判断是否为关闭状态，把所有逻辑执行完后则退出循环
		if (_b_stop)
		{
			while (!_msg_que.empty())
			{
				auto msg_node = _msg_que.front();
				
				auto call_back_iter = _func_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter != _func_callbacks.end())
				{
					call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
						std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_total_len));
				}
				_msg_que.pop();
			}
			break;
		}

		//如果没有停服，且说明队列中有数据
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;

		auto call_back_iter = _func_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _func_callbacks.end())
		{
			_msg_que.pop();
			continue;
		}

		//根据消息ID，调用对应的处理函数
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_total_len));

		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() 
{
	_func_callbacks[MSG_IDS::MSG_CHAT_LOGIN] = 
		[this](std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data) {
			this->LoginHandler(session, msg_id, msg_data);
		};

	_func_callbacks[MSG_IDS::ID_SEARCH_USER_REQ] =
		[this](std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data) {
			this->SearchInfo(session, msg_id, msg_data);
		}; 

	_func_callbacks[MSG_IDS::ID_ADD_FRIEND_REQ] =
		[this](std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data) {
			this->AddFriendApply(session, msg_id, msg_data);
		};

	_func_callbacks[MSG_IDS::ID_AUTH_FRIEND_REQ] =
		[this](std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data) {
			this->AuthFriendApply(session, msg_id, msg_data);
		};

	_func_callbacks[MSG_IDS::ID_TEXT_CHAT_MSG_REQ] =
		[this](std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data) {
			this->DealChatTextMsg(session, msg_id, msg_data);
		};
	
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) 
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();

	AsyncLog::getInstance().log(LogLevel::LogINFO,
								"ChatServer LoginHandler, uid={},token={}",
								uid,
								token);

	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]() {
			std::string return_str = rtvalue.toStyledString();
			AsyncLog::getInstance().log(LogLevel::LogINFO, "ChatServer rsp to client");
			session->Send(return_str, MSG_CHAT_LOGIN_RSP);	//用户登陆回包
		});


	//直接从redis中获取token,与客户端传过来的token进行对比
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) 
	{
		rtvalue["error"] = ErrorCodes::NoFindUser;
		return;
	}

	if (token_value != token) 
	{
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}

	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);

	rtvalue["uid"] = uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["icon"] = user_info->icon;
	rtvalue["error"] = ErrorCodes::Success;

	//从数据库获取申请列表和好友列表 
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) 
	{
		for (auto& apply : apply_list) 
		{
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["email"] = apply->_email;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	//获取好友列表
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) 
	{
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["email"] = friend_ele->email;
		rtvalue["friend_list"].append(obj);
	}

	//更新redis中该服务器的登录人数
	auto server_name = ConfigMgr::GetInstance()["SelfServer"]["name"];
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty())
	{
		count = std::stoi(rd_res);
	}

	count++;

	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, std::to_string(count));

	//为用户设置登录ip server的名字
	std::string ipkey = USERIPPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(ipkey, server_name);

	//将session和uid进行绑定
	session->SetUserId(uid);
	UserMgr::GetInstance()->SetUserSession(uid, session);

	/*std::string  uid_session_key = USER_SESSION_PREFIX + uid_str;
	RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());*/
}

bool LogicSystem::isPureDigit(const std::string& str)
{
	for (char c : str) 
	{
		if (!std::isdigit(c)) 
		{
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	//优先查redis中查询用户信息
	/*std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto email = root["email"].asString();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " email is " << email << " icon is " << icon << endl;

		rtvalue["uid"] = uid;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["icon"] = icon;
		return;
	}*/

	auto uid = std::stoi(uid_str);
	//redis中没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::NoFindUser;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["icon"] = user_info->icon;
}

void LogicSystem::GetUserByEmail(std::string email, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = EMAIL_INFO + email;

	//优先查redis中查询用户信息
	/*std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) 
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto email = root["email"].asString();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " email is " << email << "icon is" << icon << std::endl;

		rtvalue["uid"] = uid;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["icom"] = icon;
		return;
	}*/

	//redis中没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(email);
	if (user_info == nullptr) 
	{
		rtvalue["error"] = ErrorCodes::NoFindUser;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["icon"] = user_info->icon;
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);

	b_base = false;	//测试代码，强制从mysql中获取

	if (b_base) 
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->email = root["email"].asString();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " email is " << userinfo->email << "icon is" << userinfo->icon << std::endl;
	}
	else 
	{
		//redis中没有则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}

	return true;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto searchkey = root["searchkey"].asString();
	std::cout << "user SearchInfo uid is  " << searchkey << endl;

	Json::Value rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
		});

	bool b_digit = isPureDigit(searchkey);	//判断是否是uid还是email
	if (b_digit) 
	{
		GetUserByUid(searchkey, rtvalue);
	}
	else 
	{
		GetUserByEmail(searchkey, rtvalue);
	}
}

void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["from_uid"].asInt();
	auto touid = root["to_uid"].asInt();

	std::cout << "user login uid is  " << uid << " touid is " << touid << std::endl;

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;	//初始化为错误，防止后续逻辑出错未赋值
	Defer defer([this, &rtvalue, session]() {
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_ADD_FRIEND_RSP);
		});

	//先更新数据库，将申请记录插入数据库，防止对方离线时丢失申请记录
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid);

	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::GetInstance();
	auto self_name = cfg["SelfServer"]["name"];

	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	//两个用户在一个服务器，直接通知对方有申请消息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			Json::Value notify;
			notify["uid"] = uid;
			if (b_info) 
			{
				notify["name"] = apply_info->name;
				notify["icon"] = apply_info->icon;
				notify["email"] = apply_info->email;
				notify["status"] = 0;	//未认证
				notify["error"] = ErrorCodes::Success;
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);	//通知对方有好友申请
		}

		return;
	}

	//两个用户不在一个服务器，发送rpc通知
	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);

	if (b_info) 
	{
		add_req.set_name(apply_info->name);
		add_req.set_icon(apply_info->icon);
		add_req.set_email(apply_info->email);
	}

	//发送通知
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["from_uid"].asInt();
	auto touid = root["to_uid"].asInt();

	AsyncLog::getInstance().log(LogLevel::LogINFO,
								"ChatServer AuthFriendApply, B={}, A={}",
								uid,
								touid);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info) 
	{
		rtvalue["name"] = user_info->name;
		rtvalue["email"] = user_info->email;
		rtvalue["icon"] = user_info->icon;
		rtvalue["uid"] = touid;
	}
	else 
	{
		rtvalue["error"] = ErrorCodes::NoFindUser;
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP);
		});

	MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	MysqlMgr::GetInstance()->AddFriend(uid, touid);

	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::GetInstance();
	auto self_name = cfg["SelfServer"]["name"];

	if (to_ip_value == self_name) 
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) 
		{
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["uid"] = uid;
			//notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info) 
			{
				notify["name"] = user_info->name;
				notify["email"] = user_info->email;
				notify["icon"] = user_info->icon;

			}
			else 
			{
				notify["error"] = ErrorCodes::NoFindUser;
			}


			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}

		return;
	}


	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);

	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["from_uid"].asInt();
	auto touid = root["to_uid"].asInt();

	const Json::Value arrays = root["text_array"];

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["text_array"] = arrays;
	rtvalue["from_uid"] = uid;
	rtvalue["to_uid"] = touid;

	Defer defer([this, &rtvalue, session]() {
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);	
		});


	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::GetInstance();
	auto self_name = cfg["SelfServer"]["name"];
	//直接通知对方有认证通过消息
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
		}

		return;
	}

	TextChatMsgReq text_msg_req;
	text_msg_req.set_fromuid(uid);
	text_msg_req.set_touid(touid);
	for (const auto& txt_obj : arrays)
	{
		auto content = txt_obj["content"].asString();
		auto msgid = txt_obj["msgid"].asString();
		std::cout << "content is " << content << std::endl;
		std::cout << "msgid is " << msgid << std::endl;
		auto* text_msg = text_msg_req.add_textmsgs();
		text_msg->set_msgid(msgid);
		text_msg->set_msgcontent(content);
	}

	//发送通知 todo...
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) 
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	std::cout << "receive heart beat msg, uid is " << uid << std::endl;
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list) 
{
	//从mysql获取好友申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, -1, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) 
{
	//从mysql获取好友列表
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}
