#pragma once

#include <spdlog/spdlog.h>
#include <string>
#include <memory>

struct UserData
{
	std::string username;
	std::string password;
	std::string role;

	UserData(std::string username, std::string password, std::string role)
	{
		this->username = username;
		this->password = password;
		this->role = role;
	}
};

class UserDb
{
public:
	virtual std::shared_ptr<UserData> get_userdata(std::string username) const;
	virtual bool connection_ok() const;
	static std::shared_ptr<UserDb> userdb_factory(std::string userdb_str, std::shared_ptr<spdlog::logger> logger_ptr);
};