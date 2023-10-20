#pragma once

#include <QtWidgets/QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include "ui_PiTVDesktopViewer.h"
#include "ServerStatusMessage.h"

struct ServerStatusRequest
{
    QString serverHostname;
    QListWidgetItem* serverListItem = nullptr;
    bool isActiveConnection = false;
};

class PiTVDesktopViewer : public QMainWindow
{
    Q_OBJECT

public:
    PiTVDesktopViewer(QWidget *parent = nullptr);
    ~PiTVDesktopViewer();

private:
    QLabel* serverCpuProcessLoadValue = new QLabel(tr("N/A"));
    QLabel* serverCpuTotalLoadValue = new QLabel(tr("N/A"));
    QLabel* serverTempCpuValue = new QLabel(tr("N/A"));

    Ui::PiTVDesktopViewerClass ui;
    QNetworkAccessManager netAccessManager;
    QMap<QNetworkReply*, ServerStatusRequest> serverStatusReplyMap;

    void requestServerStatus(const ServerStatusRequest& request);

    void serverStatusHttpRequestFinished(QNetworkReply* reply);
    void serverStatusSslErrors(QNetworkReply* reply);

    void updateServerListItemText(QListWidgetItem* item, bool isInitialized, QString errorStr) const;
    void updateStatusBarServerStatus(QString loadCpuProcess, QString loadCpuTotal, QString tempCpu);

    void loadServerConfigs();
    void saveServerConfigs();

public slots:
    void onDisconnectClicked();
    void onExitClicked();
    void onAddServerClicked();
    void onEditServerClicked();
    void onRemoveServerClicked();
};
