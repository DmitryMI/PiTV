#include <QtWidgets/QDialog>
#include "ServerConfig.h"
#include "ui_EditServerDialog.h"

class EditServerDialog : public QDialog
{
    Q_OBJECT

public:
    EditServerDialog(QWidget* parent = nullptr);
    ~EditServerDialog();

private:
    Ui::EditServerDialogClass ui;

public:
    void setFromServerConfig(const ServerConfig& config);
    void writeToServerConfig(ServerConfig& config) const;

    QString getServerAddress() const;
    QString getUsername() const;
    QString getPassword() const;
    QString getLocalUdpEndpoint() const;

    void setServerAddress(const QString& str);
    void setUsername(const QString& str);
    void setPassword(const QString& str);
    void setLocalUdpEndpoint(const QString& str);

public slots:
    void onOpenClicked();
};
