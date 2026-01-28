#include "CServer.h"
#include "ConfigMgr.h"
int main()
{
	ConfigMgr& g_config_mgr = ConfigMgr::GetInstance();
	std::string gate_port_str = g_config_mgr["GateServer"]["port"];
	int gate_port = std::stoi(gate_port_str);
	if (gate_port < 1 || gate_port > 65535)
	{  
		AsyncLog::getInstance().log(
			LogLevel::LogERROR,
			"GateServer port config invalid, port is:{}",
			gate_port);
		return -1;	//port invalid
	}

	try
	{
		unsigned short port = static_cast<unsigned short>(gate_port);
		net::io_context ioc{ 1 };
		
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc](const boost::system::error_code& ec, int signo) {
				if(ec) return;

				ioc.stop();
			});

		std::make_shared<CServer>(ioc, port)->Start();

		AsyncLog::getInstance().log(
			LogLevel::LogINFO,
			"GateServer started on port:{}",
			port);

		ioc.run();
	}
	catch (std::exception& e)
	{
		AsyncLog::getInstance().log(
			LogLevel::LogERROR,
			"GateServer main exception is:{}",
			e.what());
	}
	return 0;
}