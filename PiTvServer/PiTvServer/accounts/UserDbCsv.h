#pragma once

#include "UserDb.h"
#include <vector>
#include <spdlog/spdlog.h>

class UserDbCsv : public UserDb
{
private:
	std::string csv_separator = ",";

	std::shared_ptr<spdlog::logger> logger_ptr;
	std::string csv_file_path;

	static int64_t index_of_str(const std::vector<std::string>& vec, std::string str);

	bool read_csv_entry(std::ifstream& fin, std::vector<std::string>& row_values) const;

public:
	UserDbCsv(std::string csv_file_path, std::shared_ptr<spdlog::logger> logger_ptr);

	virtual std::shared_ptr<UserData> get_userdata(std::string username) const override;

	void set_csv_separator(std::string separator);

	std::string get_csv_separator() const;

	virtual bool connection_ok() const override;
};