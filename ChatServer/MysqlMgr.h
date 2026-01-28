#pragma once
#include "const.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;
public:
    ~MysqlMgr() = default;

    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);

	bool CheckEmail(const std::string& name, const std::string& email);

    bool UpdatePwd(const std::string& email, const std::string& newpwd);

	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);

	std::shared_ptr<UserInfo> GetUser(int uid);

    std::shared_ptr<UserInfo> GetUser(std::string email);

    bool AddFriendApply(const int from_uid, const int to_uid);

    bool AuthFriendApply(const int& from, const int& to);

    bool AddFriend(const int& from, const int& to);

    bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit = 10);
    
    bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_info);

private:
    MysqlMgr() = default;

    MysqlDao  _dao;
};
