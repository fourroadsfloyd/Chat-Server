#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include <hiredis/hiredis.h>

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"

#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>

#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <thread>
#include <functional>

#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <string>
#include <cstring>

#include <iostream>

#include "AsyncLog.h"
#include "Singleton.h"
#include "data.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC请求错误
	VerifyExpired = 1003, //验证码过期
	VerifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已经存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码更新失败
	TokenInvalid = 1010,   //Token失效
	NoFindUser = 1011,  //uid无效
};

// Defer类
class Defer {
public:
	// 接受一个lambda表达式或者函数指针
	Defer(std::function<void()> func) : func_(func) {}

	// 析构函数中执行传入的函数
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};

#define CODEPREFIX  "code_"

#define MAX_LENGTH  1024*2
//头部总长度
#define HEAD_TOTAL_LEN 4
//头部id长度
#define HEAD_ID_LEN 2
//头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000

enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
	ID_SEARCH_USER_REQ = 1007, //用户搜索请求

	ID_SEARCH_USER_RSP = 1008, //搜索用户回包

	ID_ADD_FRIEND_REQ = 1009, //申请添加好友请求					A向B发送添加好友请求
	ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复					这是A收到的回复，表示申请发送成功
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请		当B收到A的添加好友请求后，B收到这个请求，这是rpc中的通知请求
	ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求					B同意A的好友申请后，B向A发送认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复					B同意A的好友申请后，B收到的回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请     B同意A的好友申请后，A收到这个请求，这是rpc中的通知请求

	ID_TEXT_CHAT_MSG_REQ = 1017, //文本聊天信息请求
	ID_TEXT_CHAT_MSG_RSP = 1018, //文本聊天信息回复
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息

	ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
	ID_HEART_BEAT_REQ = 1023,      //心跳请求
	ID_HEARTBEAT_RSP = 1024,       //心跳回复
};

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define EMAIL_INFO  "emailinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"

//分布式锁的持有时间
#define LOCK_TIME_OUT 10
//分布式锁的重试时间
#define ACQUIRE_TIME_OUT 5