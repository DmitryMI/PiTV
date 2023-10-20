#pragma once

#include <QtWidgets/QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include "ui_PiTVDesktopViewer.h"
#include "ServerStatusMessage.h"
#include "ServerConfig.h"
#include "Pipeline.h"

struct ServerStatusRequest
{
    ServerConfig serverConfig;
    QListWidgetItem* serverListItem = nullptr;
    bool doUpdateStatusBar = false;
};

struct CameraLeaseRequest
{
    ServerConfig serverConfig;
    int leaseTimeMsec = 30000;
    QString udpAddress;
    int udpPort;
    QString leaseGuid;
};

class PiTVDesktopViewer : public QMainWindow
{
    Q_OBJECT

public:
    PiTVDesktopViewer(QWidget *parent = nullptr);
    ~PiTVDesktopViewer();

private:
    // UI
    Ui::PiTVDesktopViewerClass ui;
    QLabel* serverCpuProcessLoadValue = new QLabel(tr("N/A"));
    QLabel* serverCpuTotalLoadValue = new QLabel(tr("N/A"));
    QLabel* serverTempCpuValue = new QLabel(tr("N/A"));

    //QScopedPointer<QTimer, QScopedPointerDeleteLater> timer;
    QTimer* pipelinePollTimer;
    QTimer* leaseUpdateTimer;

    QSharedPointer<Pipeline> pipeline;

    QNetworkAccessManager netAccessManager;
    QMap<QNetworkReply*, ServerStatusRequest> serverStatusReplyMap;
    QMap<QNetworkReply*, CameraLeaseRequest> cameraLeaseReplyMap;

    ServerConfig activeServerConfig;
    CameraLeaseRequest activeLeaseRequest;

    void requestServerStatus(const ServerStatusRequest& request);
    void requestCameraLease(const CameraLeaseRequest& request);

    void serverStatusHttpRequestFinished(QNetworkReply* reply);
    void serverStatusSslErrors(QNetworkReply* reply);

    void cameraLeaseHttpRequestFinished(QNetworkReply* reply);

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
    void onServerDoubleClicked(QListWidgetItem* item);
    void onPipelinePollTimerElapsed();
    void onLeaseUpdateTimerElapsed();
};
