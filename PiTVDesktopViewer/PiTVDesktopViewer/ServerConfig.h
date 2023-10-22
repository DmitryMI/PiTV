#pragma once
#include <QString>

class ServerConfig
{
public:
	QString serverUrl;
	QString username;
	QString password;
	QString localUdpEndpoint;
	QString tlsClientKeyPath;
	QString tlsClientCertPath;
	QString tlsCaPath;

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

Q_DECLARE_METATYPE(ServerConfig);