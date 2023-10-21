#include "EditServerDialog.h"
#include <QHostAddress>
#include <QNetworkInterface>

EditServerDialog::EditServerDialog(QWidget* parent)
{
	ui.setupUi(this);

	QList<QHostAddress> list = QNetworkInterface::allAddresses();

	for (QHostAddress addr : list)
	{
		ui.udpAddressCombo->addItem(addr.toString());
	}
}

EditServerDialog::~EditServerDialog()
{
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
