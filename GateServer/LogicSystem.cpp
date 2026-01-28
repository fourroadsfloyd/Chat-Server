#include "LogicSystem.h"
#include "HttpConnect.h"
#include "VerifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"

LogicSystem::LogicSystem()
{
	RegGet("/get_test", [](std::shared_ptr<HttpConnect> conn) {
			beast::ostream(conn->_response.body()) << "receive get_test_req";
			int i = 0;
			for (auto& elem : conn->_get_params)
			{
				i++;
				beast::ostream(conn->_response.body()) 
					<< "param " << i << "key is " << elem.first 
					<< " value is " << elem.second << std::endl;
			}
		});

	RegPost("/get_verifycode", [](std::shared_ptr<HttpConnect> conn) {
			auto body_str = boost::beast::buffers_to_string(conn->_request.body().data());
			
			conn->_response.set(http::field::content_type, "text/json");
			Json::Reader json_reader;
			Json::Value request_json;
			Json::Value response_json;
			bool parse_success = json_reader.parse(body_str, request_json);
			if (!parse_success)	
			{
				AsyncLog::getInstance().log(
					LogLevel::LogWARNING,
					"post get_verifycode req json parse failed, body is:{}",
					body_str);

				response_json["error"] = ErrorCodes::Error_Json;
				std::string jsonstr = response_json.toStyledString();//json to string
				beast::ostream(conn->_response.body()) << jsonstr;
				return;
			}

			if (!request_json.isMember("email")) 
			{
				AsyncLog::getInstance().log(
					LogLevel::LogWARNING,
					"post get_verifycode req json no email, body is:{}",
					body_str);

				response_json["error"] = ErrorCodes::Error_Json;
				std::string jsonstr = response_json.toStyledString();//json to string
				beast::ostream(conn->_response.body()) << jsonstr;
				return;
			}

			auto email = request_json["email"].asString();
			GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
			response_json["error"] = rsp.error();
			response_json["email"] = request_json["email"];
			std::string jsonstr = response_json.toStyledString();
			beast::ostream(conn->_response.body()) << jsonstr;	
			AsyncLog::getInstance().log(
				LogLevel::LogINFO,
				"/get_verifycode post return msg is:{}",
				jsonstr);
		});	

	RegPost("/user_register", [](std::shared_ptr<HttpConnect> connection) {
			auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
			AsyncLog::getInstance().log(LogLevel::LogINFO,
				"/user_register receive body is {}",
				body_str);

			connection->_response.set(http::field::content_type, "text/json");	//设置回复类型

			Json::Value root;
			Json::Reader reader;
			Json::Value src_root;

			AsyncLog::getInstance().log(LogLevel::LogINFO, "start parse");

			bool parse_success = reader.parse(body_str, src_root);
			if (!parse_success) {
				AsyncLog::getInstance().log(LogLevel::LogERROR, "Failed to parse JSON data!");
				root["error"] = ErrorCodes::Error_Json;
				std::string jsonstr = root.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return;
			}

			auto email = src_root["email"].asString();
			auto name = src_root["user"].asString();
			auto pwd = src_root["passwd"].asString();
			auto confirm = src_root["confirm"].asString();

			AsyncLog::getInstance().log(LogLevel::LogINFO, "redis find email");

			//先查找redis中email对应的验证码是否合理
			std::string  verify_code;
			bool b_get_verify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), verify_code);
			if (!b_get_verify) {
				AsyncLog::getInstance().log(LogLevel::LogERROR, "get verify code expired");
				root["error"] = ErrorCodes::VerifyExpired;
				std::string jsonstr = root.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return;
			}

			AsyncLog::getInstance().log(LogLevel::LogINFO, "client verifycode match redis verifycode");

			if (verify_code != src_root["verifycode"].asString()) {
				AsyncLog::getInstance().log(LogLevel::LogERROR, "verify code error");
				root["error"] = ErrorCodes::VerifyCodeErr;
				std::string jsonstr = root.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return;
			}

			AsyncLog::getInstance().log(LogLevel::LogINFO, "mysql find user");

			//查找数据库判断用户是否存在
			int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd);

			if (uid == 0 || uid == -1) {
				AsyncLog::getInstance().log(LogLevel::LogINFO, "user or email had existed, uid = {}", uid);
				root["error"] = ErrorCodes::UserExist;
				std::string jsonstr = root.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return;
			}


			root["error"] = 0;
			root["uid"] = uid;
			root["email"] = email;
			root["user"] = name;
			root["passwd"] = pwd;
			root["confirm"] = confirm;
			root["verifycode"] = src_root["verifycode"].asString();
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;

			AsyncLog::getInstance().log(LogLevel::LogINFO,
				"reply message: jsonstr body is {}",
				jsonstr);

			return;
		});

	RegPost("/reset_pwd", [](std::shared_ptr<HttpConnect> conn) {
			std::string body_str = boost::beast::buffers_to_string(conn->_request.body().data());
			
			//告诉客户端（浏览器/HTTP 调用方）响应体的内容类型是 JSON，从而客户端会按 JSON 的方式解析/展示内容。
			conn->_response.set(http::field::content_type, "text/json");

			Json::Reader reader;
			Json::Value request;
			Json::Value response;

			//将http中序列化成字符串的json内容，解析到request这个json对象中
			bool parse_success = reader.parse(body_str, request);
			if (!parse_success)
			{
				//将错误码写入到json对象中
				response["error"] = ErrorCodes::Error_Json;
				//将json对象序列化成字符串
				std::string json_str = response.toStyledString();
				beast::ostream(conn->_response.body()) << json_str;
				return;
			}

			//解析request中的信息：email，name，pwd
			std::string email = request["email"].asString();
			std::string name = request["user"].asString();
			std::string pwd = request["passwd"].asString();
			std::string verify_code_json = request["verifycode"].asString();

			//先在redis中查看验证码是否存在，不存在可能是没有获取验证码，也可能是验证码过期
			std::string verify_code_redis;
			bool b_get_verify = RedisMgr::GetInstance()->Get(CODEPREFIX + email, verify_code_redis);
			if (!b_get_verify)
			{
				response["error"] = ErrorCodes::VerifyExpired;
				std::string json_str = response.toStyledString();
				beast::ostream(conn->_response.body()) << json_str;
				return;
			}

			AsyncLog::getInstance().log(LogLevel::LogINFO,
				"will match verifycode from redis and requeset");

			//判断redis中的验证码和request中的验证码是否一致
			if (verify_code_redis != verify_code_json)
			{
				response["error"] = ErrorCodes::VerifyCodeErr;
				std::string json_str = response.toStyledString();
				beast::ostream(conn->_response.body()) << json_str;
				return;
			}

			AsyncLog::getInstance().log(LogLevel::LogINFO, 
										"will match user and email");

			//查询数据库判断用户名和邮箱是否匹配, 或者该用户根本没有注册
			bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
			if (!email_valid)
			{
				response["error"] = ErrorCodes::EmailNotMatch;
				std::string json_str = response.toStyledString();
				beast::ostream(conn->_response.body()) << json_str;
				return;
			}

			AsyncLog::getInstance().log(LogLevel::LogINFO,"start update pwd");

			bool b_update_pwd = MysqlMgr::GetInstance()->UpdatePwd(email, pwd);
			if (!b_update_pwd)
			{
				response["error"] = ErrorCodes::PasswdUpFailed;
				std::string json_str = response.toStyledString();
				beast::ostream(conn->_response.body()) << json_str;
				return;
			}

			response["error"] = 0;
			response["email"] = email;
			response["user"] = name;
			response["passwd"] = pwd;
			response["verifycode"] = verify_code_json;

			std::string json_str = response.toStyledString();

			AsyncLog::getInstance().log(LogLevel::LogINFO,
										"response json is {}",
										json_str);

			beast::ostream(conn->_response.body()) << json_str;
			return;
		});

	//用户登录逻辑
	RegPost("/user_login", [](std::shared_ptr<HttpConnect> connection) {
			auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
			
			connection->_response.set(http::field::content_type, "text/json");
			
			Json::Reader reader;
			Json::Value request;
			Json::Value response;
			
			bool parse_success = reader.parse(body_str, request);
			if (!parse_success) {
				std::cout << "Failed to parse JSON data!" << std::endl;
				response["error"] = ErrorCodes::Error_Json;
				std::string jsonstr = response.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return true;
			}

			auto email = request["email"].asString();
			auto pwd = request["pwd"].asString();
			UserInfo userInfo;
			//查询数据库判断用户名和密码是否匹配
			bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
			if (!pwd_valid) {
				std::cout << " user pwd not match" << std::endl;
				response["error"] = ErrorCodes::PasswdInvalid;
				std::string jsonstr = response.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return true;
			}

			//分配聊天服务器
			//查询StatusServer找到合适的连接
			auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
			if (reply.error()) {
				std::cout << " grpc get chat server failed, error is " << reply.error() << std::endl;
				response["error"] = ErrorCodes::RPCFailed;
				std::string jsonstr = response.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
				return true;
			}

			std::cout << "succeed to load userinfo uid is " << userInfo.uid << std::endl;

			response["error"] = 0;
			response["uid"] = userInfo.uid;
			response["token"] = reply.token();
			response["host"] = reply.host();
			response["port"] = reply.port();
			std::string jsonstr = response.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		});

}

void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
	_get_handlers.insert({url, handler});
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnect> conn)
{
	if (_get_handlers.find(path) == _get_handlers.end())
	{
		return false;
	}

	_get_handlers[path](conn);
	return true;
}

void LogicSystem::RegPost(std::string url, HttpHandler handler)
{
	_post_handlers.insert({ url, handler });
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnect> conn) 
{
	if (_post_handlers.find(path) == _post_handlers.end()) {
		return false;
	}

	_post_handlers[path](conn);
	return true;
}