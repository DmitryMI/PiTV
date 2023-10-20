#pragma once

#include "PlatformEncryption.h"

bool PlatformEncryption::encrypt(const QByteArray& input, QByteArray& output, const QString& description)
{
	DATA_BLOB decryptedData;
	DATA_BLOB encryptedData;

	decryptedData.pbData = (BYTE*)input.data();
	decryptedData.cbData = input.length();

	if (CryptProtectData(&decryptedData,
		description.toStdWString().c_str(),
		NULL,
		NULL,
		NULL,
		0,
		&encryptedData))
	{
		output = QByteArray::fromRawData((char*)encryptedData.pbData, encryptedData.cbData);
		return true;
	}

	return false;
}

bool PlatformEncryption::decrypt(const QByteArray& input, QByteArray& output, QString& description)
{
	DATA_BLOB encryptedData;
	DATA_BLOB decryptedData;
	LPWSTR descStr;

	encryptedData.pbData = (BYTE*)input.data();
	encryptedData.cbData = input.length();

	if (CryptUnprotectData(&encryptedData,
		&descStr,
		NULL,
		NULL,
		NULL,
		0,
		&decryptedData))
	{
		output = QByteArray::fromRawData((char*)decryptedData.pbData, decryptedData.cbData);
		description = QString::fromWCharArray(descStr);
		return true;
	}

	return false;
}