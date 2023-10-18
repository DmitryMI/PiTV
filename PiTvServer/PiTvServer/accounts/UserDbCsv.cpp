#include "UserDbCsv.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <boost/algorithm/string.hpp>

int64_t UserDbCsv::index_of_str(const std::vector<std::string>& vec, std::string str)
{
	auto it = std::find(vec.begin(), vec.end(), str);

	int64_t index = -1;

	if (it != vec.end())
	{
		index = it - vec.begin();
	}
	return index;
}


bool UserDbCsv::read_csv_entry(std::ifstream& fin, std::vector<std::string>& row_values) const
{
	std::string line;
	std::getline(fin, line);
	if (!fin.good())
	{
		logger_ptr->error("Failed to read CSV entry of {}!", csv_file_path);
		return false;
	}

	if (line.starts_with("#"))
	{
		row_values.clear();
		return true;
	}

	if (line.find_first_not_of(' ') == std::string::npos)
	{
		row_values.clear();
		return true;
	}

	std::stringstream values_stream(line);
	std::string value;

	while (std::getline(values_stream, value, csv_separator[0]))
	{
		boost::trim(value);
		row_values.push_back(value);
	}

	return true;
}

UserDbCsv::UserDbCsv(std::string csv_file_path, std::shared_ptr<spdlog::logger> logger_ptr)
{
	this->csv_file_path = csv_file_path;
	this->logger_ptr = logger_ptr;
}

std::shared_ptr<UserData> UserDbCsv::get_userdata(std::string username) const
{
	if (!std::filesystem::exists(csv_file_path))
	{
		logger_ptr->error("Failed to get userdata for {}: file {} not found!", username, csv_file_path);
		return nullptr;
	}

	std::ifstream csv_file(csv_file_path);

	if (!csv_file.is_open()) throw std::runtime_error("Could not open file");

	std::vector<std::string> column_names;
	if (!read_csv_entry(csv_file, column_names))
	{
		logger_ptr->error("Failed to get userdata for {}: failed to read CSV header from {}!", username, csv_file_path);
		csv_file.close();
		return nullptr;
	}

	int64_t username_index = index_of_str(column_names, "username");
	int64_t password_index = index_of_str(column_names, "password");
	int64_t role_index = index_of_str(column_names, "role");

	if (username_index == -1 || password_index == -1 || role_index == -1)
	{
		logger_ptr->error("Failed to get userdata for {}: CSV {} is malformed!", username, csv_file_path);
		csv_file.close();
		return nullptr;
	}

	bool user_found = false;
	std::string password;
	std::string role;

	int csv_line_num = 2;
	while (!csv_file.eof() && !user_found)
	{
		std::vector<std::string> row_values;
		if (!read_csv_entry(csv_file, row_values))
		{
			logger_ptr->info("Cannot read next CSV entry in {}. Probably EOF.", csv_file_path);
			csv_line_num++;
			continue;
		}

		if (row_values.size() == 0)
		{
			logger_ptr->debug("CSV comment or empty row encountered at {}:{}", username, csv_file_path, csv_line_num);
			csv_line_num++;
			continue;
		}

		if (row_values.size() <= (size_t)username_index ||
			row_values.size() <= (size_t)password_index ||
			row_values.size() <= (size_t)role_index)
		{
			logger_ptr->error("Malformed CSV row detected at {}:{}", csv_file_path, csv_line_num);
			csv_line_num++;
			continue;
		}

		std::string username_entry = row_values[username_index];
		if (username_entry != username)
		{
			csv_line_num++;
			continue;
		}

		password = row_values[password_index];
		role = row_values[role_index];
		user_found = true;
	}
	
	csv_file.close();

	if (!user_found)
	{
		return nullptr;
	}

	return std::make_shared<UserData>(username, password, role);
}

void UserDbCsv::set_csv_separator(std::string separator)
{
	csv_separator = separator;
	assert(csv_separator.length() == 1);
}

std::string UserDbCsv::get_csv_separator() const
{
	return csv_separator;
}
