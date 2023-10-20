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
    QListWidgetItem* serverListItem;
};

class PiTVDesktopViewer : public QMainWindow
{
    Q_OBJECT

public:
    PiTVDesktopViewer(QWidget *parent = nullptr);
    ~PiTVDesktopViewer();

private:
    Ui::PiTVDesktopViewerClass ui;
    QNetworkAccessManager netAccessManager;
    QMap<QNetworkReply*, ServerStatusRequest> serverStatusReplyMap;

    void requestServerStatus(const ServerStatusRequest& request);

    void serverStatusHttpRequestFinished(QNetworkReply* reply);
    void serverStatusSslErrors(QNetworkReply* reply);

public slots:
    void onDisconnectClicked();
    void onExitClicked();
    void onAddServerClicked();
    void onEditServerClicked();
    void onRemoveServerClicked();
};
