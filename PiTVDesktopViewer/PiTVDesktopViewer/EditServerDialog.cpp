#include "EditServerDialog.h"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QFileDialog>

EditServerDialog::EditServerDialog(QWidget* parent)
{
	ui.setupUi(this);

	QList<QHostAddress> list = QNetworkInterface::allAddresses();

	for (QHostAddress addr : list)
	{
		ui.udpAddressCombo->addItem(addr.toString());
	}

	connect(ui.caOpenButton, &QPushButton::clicked,  this, &EditServerDialog::onOpenClicked);
	connect(ui.clientKeyOpenButton, &QPushButton::clicked, this, &EditServerDialog::onOpenClicked);
	connect(ui.clientCertOpenButton, &QPushButton::clicked, this, &EditServerDialog::onOpenClicked);
}

EditServerDialog::~EditServerDialog()
{
}

void EditServerDialog::setFromServerConfig(const ServerConfig& config)
{
	setServerAddress(config.serverUrl);
	setUsername(config.username);
	setPassword(config.password);
	setLocalUdpEndpoint(config.localUdpEndpoint);
	ui.caPath->setText(config.tlsCaPath);
	ui.clientCertPath->setText(config.tlsClientCertPath);
	ui.clientKeyPath->setText(config.tlsClientKeyPath);
}

void EditServerDialog::writeToServerConfig(ServerConfig& config) const
{
	config.serverUrl = getServerAddress();
	config.username = getUsername();
	config.password = getPassword();
	config.localUdpEndpoint = getLocalUdpEndpoint();
	config.tlsCaPath = ui.caPath->text();
	config.tlsClientCertPath = ui.clientCertPath->text();
	config.tlsClientKeyPath = ui.clientKeyPath->text();
}

QString EditServerDialog::getServerAddress() const
{
	return ui.serverAddressLine->text();
}

QString EditServerDialog::getUsername() const
{
	return ui.usernameLine->text();
}

QString EditServerDialog::getPassword() const
{
	return ui.passwordLine->text();
}

QString EditServerDialog::getLocalUdpEndpoint() const
{
	QString host = ui.udpAddressCombo->currentText();
	QString port = ui.udpPortLine->text();
	return host + ":" + port;
}

void EditServerDialog::setServerAddress(const QString& str)
{
	ui.serverAddressLine->setText(str);
}

void EditServerDialog::setUsername(const QString& str)
{
	ui.usernameLine->setText(str);
}

void EditServerDialog::setPassword(const QString& str)
{
	ui.passwordLine->setText(str);
}

void EditServerDialog::setLocalUdpEndpoint(const QString& str)
{
	auto items = str.split(":");
	ui.udpAddressCombo->setCurrentText(items[0]);
	if (items.size() == 2)
	{
		ui.udpPortLine->setText(items[1]);
	}
}

void EditServerDialog::onOpenClicked()
{
	QString currentPath;
	if (sender() == ui.caOpenButton)
	{
		currentPath = ui.caPath->text();
	}
	else if (sender() == ui.clientCertOpenButton)
	{
		currentPath = ui.clientCertPath->text();
	}
	else
	{
		currentPath = ui.clientKeyPath->text();
	}

	QString dirPath = "";
	QFileInfo currentFileInfo(currentPath);
	if (currentFileInfo.exists())
	{
		currentPath = currentFileInfo.absoluteFilePath();
		dirPath = currentFileInfo.absoluteDir().absolutePath();
	}

	QString selectedPath = QFileDialog::getOpenFileName(this, "Select TLS file", dirPath);
	if (selectedPath.isEmpty())
	{
		return;
	}

	if (sender() == ui.caOpenButton)
	{
		ui.caPath->setText(selectedPath);
	}
	else if (sender() == ui.clientCertOpenButton)
	{
		ui.clientCertPath->setText(selectedPath);
	}
	else
	{
		ui.clientKeyPath->setText(selectedPath);
	}
}