#pragma once
#include "const.h"

class AsioIOServicePool : public Singleton<AsioIOServicePool>{
	friend class Singleton<AsioIOServicePool>;

private:
	AsioIOServicePool(std::size_t size = 2 /*std::thread::hardware_concurrency()*/);
	
public:
	using IOService = boost::asio::io_context;
	using Work = boost::asio::executor_work_guard<IOService::executor_type>;
	using WorkPtr = std::unique_ptr<Work>;

	~AsioIOServicePool() = default;

	boost::asio::io_context& GetIOService();

	void Stop();

private:
	std::vector<IOService> _ioServices;
	std::vector<WorkPtr> _works;
	std::vector<std::thread> _threads;
	std::size_t _nextIOService;
};

