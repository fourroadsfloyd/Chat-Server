#pragma once
#include <string>

struct UserInfo {
	UserInfo() :uid(0), name(""), email(""), icon("") {}

	int uid;
	std::string name;
	std::string email;
	std::string icon;
};

struct ApplyInfo {
	ApplyInfo(int uid, std::string name, std::string email,
		std::string icon, int status)
		:_uid(uid), _name(name), _email(email), _icon(icon), _status(status) 
	{
	}

	int _uid;
	std::string _name;
	std::string _email;
	std::string _icon;
	int _status;
};
