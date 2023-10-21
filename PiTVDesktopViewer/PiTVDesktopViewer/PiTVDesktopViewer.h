#pragma once

#include <QtWidgets/QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QThread>
#include "ui_PiTVDesktopViewer.h"
#include "ServerStatusMessage.h"
#include "ServerConfig.h"
#include "Pipeline.h"

struct CameraLeaseRequest
{
    ServerConfig serverConfig;
    int leaseTimeMsec = 30000;
    QString udpAddress;
    int udpPort = 5000;
    QString leaseGuid;
};

class PipelineAsyncConstructor : public QThread
{
    Q_OBJECT

private:
    WId windowHandle;
    CameraLeaseRequest request;
public:
    PipelineAsyncConstructor(WId handle, const CameraLeaseRequest& request);
    virtual void run() override;

signals:
    void pipelineConstructed(Pipeline* pipeline, CameraLeaseRequest request);
};


struct ServerStatusRequest
{
    ServerConfig serverConfig;
    QListWidgetItem* serverListItem = nullptr;
    bool doUpdateStatusBar = false;
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

    PipelineAsyncConstructor* pipelineContructorThread = nullptr;

    //QScopedPointer<QTimer, QScopedPointerDeleteLater> timer;
    QTimer* pipelinePollTimer;
    QTimer* leaseUpdateTimer;

    QSharedPointer<Pipeline> pipeline;

    QNetworkAccessManager netAccessManager;
    QMap<QNetworkReply*, ServerStatusRequest> serverStatusReplyMap;
    QMap<QNetworkReply*, CameraLeaseRequest> cameraLeaseReplyMap;

    CameraLeaseRequest activeLeaseRequest;

    void requestServerStatus(const ServerStatusRequest& request);
    void requestCameraLease(const CameraLeaseRequest& request);
    void disconnectFromCamera(const CameraLeaseRequest& request);

    void serverStatusHttpRequestFinished(QNetworkReply* reply);
    void serverStatusSslErrors(QNetworkReply* reply);

    void cameraLeaseHttpRequestFinished(QNetworkReply* reply);
    void cameraEndLeaseRequestFinished(QNetworkReply* reply);

    void updateServerListItemText(QListWidgetItem* item, bool isInitialized, QString errorStr) const;
    void updateStatusBarServerStatus(QString loadCpuProcess, QString loadCpuTotal, QString tempCpu);

    void loadServerConfigs();
    void saveServerConfigs();

    void connectToCamera(const CameraLeaseRequest& request);

public slots:
    void onDisconnectClicked();
    void onExitClicked();
    void onAddServerClicked();
    void onEditServerClicked();
    void onRemoveServerClicked();
    void onServerDoubleClicked(QListWidgetItem* item);
    void onPipelinePollTimerElapsed();
    void onLeaseUpdateTimerElapsed();
    void onPipelineConstructed(Pipeline* pipeline, const CameraLeaseRequest& request);
};
