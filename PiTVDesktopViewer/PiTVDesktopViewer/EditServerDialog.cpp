#include "EditServerDialog.h"

EditServerDialog::EditServerDialog(QWidget* parent)
{
	ui.setupUi(this);
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
