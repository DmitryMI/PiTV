#include <QtWidgets/QDialog>
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
    QString getServerAddress() const;
    QString getUsername() const;
    QString getPassword() const;

    void setServerAddress(const QString& str);
    void setUsername(const QString& str);
    void setPassword(const QString& str);

public slots:
};
