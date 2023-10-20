#pragma once
#include <QString>

class ServerConfig
{
public:
	QString serverUrl;
	QString username;
	QString password;

	ServerConfig(QString url, QString username, QString password)
	{
		this->serverUrl = url;
		this->username = username;
		this->password = password;
	}

	ServerConfig()
	{
		serverUrl = "";
		username = "";
		password = "";
	}
};