#include "MysqlMgr.h"

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
    return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email)
{
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& email, const std::string& newpwd)
{
    return _dao.UpdatePwd(email, newpwd);
}

bool MysqlMgr::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo)
{
	return _dao.CheckPwd(name, pwd, userInfo);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
    return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(std::string email)
{
    return _dao.GetUser(email);
}

bool MysqlMgr::AddFriendApply(const int from_uid, const int to_uid)
{
    return _dao.AddFriendApply(from_uid, to_uid);
}

bool MysqlMgr::AuthFriendApply(const int& from, const int& to)
{
    return _dao.AuthFriendApply(from, to);
}

bool MysqlMgr::AddFriend(const int& from, const int& to)
{
    return _dao.AddFriend(from, to);
}

bool MysqlMgr::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit)
{
	return _dao.GetApplyList(touid, applyList, begin, limit);
}

bool MysqlMgr::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_info)
{
	return _dao.GetFriendList(self_id, user_info);
}
