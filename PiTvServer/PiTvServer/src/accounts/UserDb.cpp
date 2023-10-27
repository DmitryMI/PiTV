#include "UserDb.h"
#include "UserDbCsv.h"

std::shared_ptr<UserData> UserDb::get_userdata(std::string username) const
{
	return nullptr;
}

bool UserDb::connection_ok() const
{
	return false;
}

std::shared_ptr<UserDb> UserDb::userdb_factory(std::string userdb_str, std::shared_ptr<spdlog::logger> logger_ptr)
{
	std::shared_ptr<UserDb> ptr;

	if (userdb_str.starts_with("file://") ||
		userdb_str.starts_with("C:/") ||
		userdb_str.starts_with("C:\\") ||
		userdb_str.starts_with("/") ||
		userdb_str.starts_with("./")
		)
	{
		UserDbCsv* csv_db = new UserDbCsv(userdb_str, logger_ptr);
		ptr.reset(csv_db);
		return ptr;
	}

	logger_ptr->error("User DB connection string: not supported provider or provided not recognised!");
	return nullptr;
}
