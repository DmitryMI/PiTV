#pragma once

#include <QByteArray>
#include <QString>
#include "windows.h"
#include "dpapi.h"

class PlatformEncryption
{
public:
	static bool encrypt(const QByteArray& input, QByteArray& output, const QString& description);
	static bool decrypt(const QByteArray& input, QByteArray& output, QString& description);
};