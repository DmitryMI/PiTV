#pragma once

#include <QByteArray>
#include <QString>
#include "ServerConfig.h"

class ServerConfigStorage
{
private:
	QString databaseFilePath;
public:

	ServerConfigStorage(QString dbFilePath);

	bool loadAllConfigs(QList<ServerConfig>& configList);

	bool saveAllConfigs(const QList<ServerConfig>& configList);
};