#include <csignal>
#include "CServer.h"
#include "ConfigMgr.h"
#include "LogicSystem.h"
#include "AsioIOServicePool.h"
#include "RedisMgr.h"
#include "ChatServiceImpl.h"

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
	auto& cfg = ConfigMgr::GetInstance();
	auto server_name = cfg["SelfServer"]["name"];
    try 
    {
		auto pool = AsioIOServicePool::GetInstance();
		//将登录数设置为0
		RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");
		Defer derfer([server_name]() {
			RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
			RedisMgr::GetInstance()->Close();
			});

		//定义一个GrpcServer
		std::string server_address(cfg["SelfServer"]["host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		ChatServiceImpl service;
		grpc::ServerBuilder builder;
		// 监听端口和添加服务
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);

		// 构建并启动gRPC服务器
		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		std::cout << "RPC Server listening on " << server_address << std::endl;

		////单独启动一个线程处理grpc服务
		//std::thread  grpc_server_thread([&server]() {
		//	server->Wait();
		//	});

		boost::asio::io_context io_context;

		auto port_str = cfg["SelfServer"]["port"];
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context, pool, &server](auto, auto) {
			io_context.stop();
			pool->Stop();
			server->Shutdown();
			});

		unsigned short port = static_cast<unsigned short>(std::atoi(port_str.c_str()));
		CServer cserver(io_context, port);
		io_context.run();


    }
    catch (std::exception& e) 
	{
        std::cerr << "Exception: " << e.what() << std::endl;
    }

}