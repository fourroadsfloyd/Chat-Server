#pragma once
#include "const.h"

class HttpConnect;

typedef std::function<void(std::shared_ptr<HttpConnect>)> HttpHandler;

class LogicSystem : public Singleton<LogicSystem>, 
					public std::enable_shared_from_this<LogicSystem> {
	friend class Singleton<LogicSystem>;
private:
	LogicSystem();

public:
	~LogicSystem() = default;

	void RegGet(std::string url, HttpHandler handler);

	bool HandleGet(std::string path, std::shared_ptr<HttpConnect> conn);

	void RegPost(std::string url, HttpHandler handler);

	bool HandlePost(std::string path, std::shared_ptr<HttpConnect> conn);

private:
	std::map<std::string, HttpHandler> _post_handlers;

	std::map<std::string, HttpHandler> _get_handlers;
};

