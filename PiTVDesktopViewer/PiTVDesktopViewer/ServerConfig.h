#pragma once
#include <QString>

class ServerConfig
{
public:
	QString serverUrl;
	QString username;
	QString password;
	QString localUdpEndpoint;

	ServerConfig(QString url, QString username, QString password, QString localUdpEndpoint)
	{
		this->serverUrl = url;
		this->username = username;
		this->password = password;
		this->localUdpEndpoint = localUdpEndpoint;
	}

	ServerConfig()
	{
		serverUrl = "";
		username = "";
		password = "";
		this->localUdpEndpoint = localUdpEndpoint;
	}
};