#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include "ServerConfigStorage.h"
#include "PlatformEncryption.h"

ServerConfigStorage::ServerConfigStorage(QString dbFilePath)
{
	databaseFilePath = dbFilePath;
}

bool ServerConfigStorage::loadAllConfigs(QList<ServerConfig>& configList)
{
	QString jsonText;
	QFile file;
	file.setFileName(databaseFilePath);

	if (!file.exists())
	{
		return false;
	}

	file.open(QIODevice::ReadOnly | QIODevice::Text);
	jsonText = file.readAll();
	file.close();

	qDebug() << jsonText;
	QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
	QJsonObject root = doc.object();

	QJsonArray serversArray = root["servers"].toArray();
	for (auto objRef : serversArray)
	{
		ServerConfig serverConfig;
		QJsonObject jsonObject = objRef.toObject();
		serverConfig.serverUrl = jsonObject["URL"].toString();
		serverConfig.username = jsonObject["Username"].toString();
		serverConfig.localUdpEndpoint = jsonObject["LocalUdpEndpoint"].toString();
		serverConfig.tlsCaPath = jsonObject["TlsCaPath"].toString();
		serverConfig.tlsClientCertPath = jsonObject["TlsClientCertPath"].toString();
		serverConfig.tlsClientKeyPath = jsonObject["TlsClientKeyPathPath"].toString();

		QString encPassBase64 = jsonObject["Password"].toString();
		QByteArray encPassBase64Bytes = encPassBase64.toUtf8();

		QByteArray encryptedPasswordBytes = QByteArray::fromBase64(encPassBase64Bytes);

		QByteArray plainPasswordBytes;
		QString desc;
		if (!PlatformEncryption::decrypt(encryptedPasswordBytes, plainPasswordBytes, desc))
		{
			return false;
		}

		if (desc != "password")
		{
			return false;
		}

		serverConfig.password = QString::fromUtf8(plainPasswordBytes);

		configList.append(serverConfig);
	}

	return true;
}

bool ServerConfigStorage::saveAllConfigs(const QList<ServerConfig>& configList)
{
	QJsonDocument doc;
	QJsonObject root = doc.object();

	QJsonArray serversArray;
	for (const ServerConfig& config : configList)
	{
		QJsonObject configObject;
		configObject.insert("URL", QJsonValue::fromVariant(config.serverUrl));
		configObject.insert("Username", QJsonValue::fromVariant(config.username));
		configObject.insert("LocalUdpEndpoint", QJsonValue::fromVariant(config.localUdpEndpoint));
		configObject.insert("TlsCaPath", QJsonValue::fromVariant(config.tlsCaPath));
		configObject.insert("TlsClientCertPath", QJsonValue::fromVariant(config.tlsClientCertPath));
		configObject.insert("TlsClientKeyPathPath", QJsonValue::fromVariant(config.tlsClientKeyPath));

		QByteArray encryptedPasswordBytes;
		QByteArray plainPasswordBytes = config.password.toUtf8();
		if (!PlatformEncryption::encrypt(plainPasswordBytes, encryptedPasswordBytes, "password"))
		{
			return false;
		}

		QString encPassBase64 = encryptedPasswordBytes.toBase64();

		configObject.insert("Password", QJsonValue::fromVariant(encPassBase64));
		serversArray.push_back(configObject);
	}
	root.insert("servers", serversArray);
	doc.setObject(root);
	qDebug() << doc.toJson();

	QFile file(databaseFilePath);
	if (file.open(QIODevice::ReadWrite))
	{
		file.resize(0);
		QTextStream stream(&file);
		stream << doc.toJson();
		file.close();
	}
	else
	{
		return false;
	}

	return true;
}

