#pragma once

#include "const.h"
#include <grpcpp/grpcpp.h>

template<class ServiceType>
class GrpcStubPool {
public:

	//typename用于在模板中引用依赖类型，StubType的具体类型与ServiceType相关
	using StubType = typename ServiceType::Stub;

	GrpcStubPool(size_t poolSize, std::string host, std::string port)
		: _b_stop(false), _poolSize(poolSize), _host(std::move(host)), _port(std::move(port))
	{
		for (size_t i = 0; i < _poolSize; ++i)
		{
			auto channel = grpc::CreateChannel(_host + ":" + _port, grpc::InsecureChannelCredentials());
			_conn_queue.push(ServiceType::NewStub(channel));
		}
	}

	~GrpcStubPool()
	{
		Close();

		std::lock_guard<std::mutex> lock(_mutex);
		while (!_conn_queue.empty())
		{
			_conn_queue.pop();
		}
	}

	void Close()
	{
		_b_stop = true;
		_cond.notify_all();
	}

	std::unique_ptr<StubType> GetConnection()
	{
		std::unique_lock<std::mutex> lock(_mutex);

		_cond.wait(lock, [this]() {
			return !_conn_queue.empty() || _b_stop.load();
			});

		if (_b_stop && _conn_queue.empty())
		{
			return nullptr;
		}

		auto conn = std::move(_conn_queue.front());
		_conn_queue.pop();
		return conn;
	}

	void returnConnection(std::unique_ptr<StubType> stub)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_b_stop)
		{
			return;
		}

		_conn_queue.push(std::move(stub));
		_cond.notify_one();
	}

private:
	std::atomic<bool> _b_stop;
	size_t _poolSize;
	std::string _host;
	std::string _port;

	std::queue<std::unique_ptr<StubType>> _conn_queue;
	std::condition_variable _cond;
	std::mutex _mutex;
};