#include "PiTVDesktopViewer.h"
#include <QStatusBar>
#include <qlabel.h>
#include <qpushbutton.h>
#include "EditServerDialog.h"


PiTVDesktopViewer::PiTVDesktopViewer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    QStatusBar* statusBar = new QStatusBar();

    QLabel* serverCpuProcessLoadInfo = new QLabel(tr("CPU (process):"));
    QLabel* serverCpuProcessLoadValue = new QLabel(tr("N/A"));
    QLabel* serverCpuTotalLoadInfo = new QLabel(tr("CPU (total):"));
    QLabel* serverCpuTotalLoadValue = new QLabel(tr("N/A"));
    QLabel* serverTempCpuInfo = new QLabel(tr("CPU Temperature:"));
    QLabel* serverTempCpuValue = new QLabel(tr("N/A"));
    statusBar->addWidget(serverCpuProcessLoadInfo);
    statusBar->addWidget(serverCpuProcessLoadValue);
    statusBar->addWidget(serverCpuTotalLoadInfo);
    statusBar->addWidget(serverCpuTotalLoadValue);
    statusBar->addWidget(serverTempCpuInfo);
    statusBar->addWidget(serverTempCpuValue);
    setStatusBar(statusBar);

    // Connect signals
    connect(ui.actionDisconnect, &QAction::triggered, this, &PiTVDesktopViewer::onDisconnectClicked);
    connect(ui.addServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onAddServerClicked);
    connect(ui.editServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onEditServerClicked);
    connect(ui.removeServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onRemoveServerClicked);
}

PiTVDesktopViewer::~PiTVDesktopViewer()
{
}

void PiTVDesktopViewer::onDisconnectClicked()
{
}

void PiTVDesktopViewer::onExitClicked()
{
    QApplication::quit();
}

void PiTVDesktopViewer::onAddServerClicked()
{
    EditServerDialog* editServerDialog = new EditServerDialog();
    editServerDialog->setWindowModality(Qt::ApplicationModal);
    editServerDialog->exec();

    if (!editServerDialog->result())
    {
        return;
    }

    QMap<QString, QVariant> serverDataMap;
    serverDataMap["serverAddress"] = editServerDialog->getServerAddress();
    serverDataMap["username"] = editServerDialog->getUsername();
    serverDataMap["password"] = editServerDialog->getPassword();

    QListWidgetItem* item = new QListWidgetItem(ui.serverListWidget);
    item->setData(Qt::UserRole, serverDataMap);
    item->setText(editServerDialog->getServerAddress() + "(" + editServerDialog->getUsername() + ")");
    delete editServerDialog;
}

void PiTVDesktopViewer::onEditServerClicked()
{
    EditServerDialog* editServerDialog = new EditServerDialog();
    editServerDialog->setWindowModality(Qt::ApplicationModal);
    editServerDialog->exec();

    if (!editServerDialog->result())
    {
        return;
    }


}

void PiTVDesktopViewer::onRemoveServerClicked()
{
}
