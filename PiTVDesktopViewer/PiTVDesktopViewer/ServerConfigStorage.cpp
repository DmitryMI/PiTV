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
    return false;
}

bool ServerConfigStorage::saveAllConfigs(const QList<ServerConfig>& configList)
{
    QJsonDocument doc;
    QJsonObject root = doc.object();

    QJsonArray usersArray;
    for (const ServerConfig& config : configList)
    {
        QJsonObject configObject;
        configObject.insert("URL: ", QJsonValue::fromVariant(config.serverUrl));
        configObject.insert("Username: ", QJsonValue::fromVariant(config.username));

        QByteArray plainPasswordBytes;
        QByteArray encryptedPasswordBytes;
        config.password.fromUtf8(plainPasswordBytes);
        if (!PlatformEncryption::encrypt(plainPasswordBytes, encryptedPasswordBytes, "password"))
        {
            return false;
        }

        QString encPassBase64 = encryptedPasswordBytes.toBase64();

        configObject.insert("Password: ", QJsonValue::fromVariant(encPassBase64));
        usersArray.push_back(configObject);
    }
    root.insert("servers", usersArray);
    doc.setObject(root);
    qDebug() << doc.toJson();

    QFile file(databaseFilePath);
    if (file.open(QIODevice::ReadWrite))
    {
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

