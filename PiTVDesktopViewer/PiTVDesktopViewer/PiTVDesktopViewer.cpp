#pragma once

#include "PiTVDesktopViewer.h"
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QTimer>
#include "EditServerDialog.h"
#include "ServerConfig.h"
#include "ServerConfigStorage.h"
#include "QMessageBox"

PiTVDesktopViewer::PiTVDesktopViewer(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	//timer.reset(new QTimer(this));
	pipelinePollTimer = new QTimer(this);
	connect(pipelinePollTimer, &QTimer::timeout, this, QOverload<>::of(&PiTVDesktopViewer::onPipelinePollTimerElapsed));
	pipelinePollTimer->start(100);

	leaseUpdateTimer = new QTimer(this);
	connect(leaseUpdateTimer, &QTimer::timeout, this, QOverload<>::of(&PiTVDesktopViewer::onLeaseUpdateTimerElapsed));

	QStatusBar* statusBar = new QStatusBar();

	serverCpuProcessLoadValue = new QLabel();
	serverCpuTotalLoadValue = new QLabel();
	serverTempCpuValue = new QLabel();

	updateStatusBarServerStatus("N/A", "N/A", "N/A");

	statusBar->addWidget(serverCpuProcessLoadValue);
	statusBar->addWidget(serverCpuTotalLoadValue);
	statusBar->addWidget(serverTempCpuValue);
	setStatusBar(statusBar);

	connect(ui.actionDisconnect, &QAction::triggered, this, &PiTVDesktopViewer::onDisconnectClicked);
	connect(ui.addServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onAddServerClicked);
	connect(ui.editServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onEditServerClicked);
	connect(ui.removeServerButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onRemoveServerClicked);
	connect(ui.serverListWidget, &QListWidget::itemDoubleClicked, this, &PiTVDesktopViewer::onServerDoubleClicked);
	connect(ui.actionExit, &QAction::triggered, this, &PiTVDesktopViewer::onExitClicked);

	loadServerConfigs();
}

PiTVDesktopViewer::~PiTVDesktopViewer()
{
}

void PiTVDesktopViewer::onDisconnectClicked()
{
}

void PiTVDesktopViewer::requestServerStatus(const ServerStatusRequest& request)
{
	QUrl url(request.serverConfig.serverUrl + "/status");
	QNetworkReply* reply = netAccessManager.get(QNetworkRequest(url));
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { serverStatusHttpRequestFinished(reply); });
#if QT_CONFIG(ssl)
	connect(reply, &QNetworkReply::sslErrors, this, [this, reply]() { serverStatusSslErrors(reply); });
#endif

	serverStatusReplyMap[reply] = request;
}

void PiTVDesktopViewer::requestCameraLease(const CameraLeaseRequest& request)
{
	QJsonDocument requestJsonDoc;
	QJsonObject jsonObj = requestJsonDoc.object();
	jsonObj["lease_guid"] = request.leaseGuid;
	jsonObj["udp_address"] = request.udpAddress;
	jsonObj["udp_port"] = request.udpPort;
	jsonObj["lease_time"] = request.leaseTimeMsec;
	requestJsonDoc.setObject(jsonObj);
	QByteArray postPayload = requestJsonDoc.toJson();

	QString creds = QString("%1:%2").arg(request.serverConfig.username, request.serverConfig.password);
	QByteArray data = creds.toLocal8Bit().toBase64();
	QString headerData = "Basic " + data;

	QUrl url(request.serverConfig.serverUrl + "/camera");
	QNetworkRequest netRequest(url);
	netRequest.setRawHeader("Authorization", headerData.toLocal8Bit());
	netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	QNetworkReply* reply = netAccessManager.post(netRequest, postPayload);
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { cameraLeaseHttpRequestFinished(reply); });

	cameraLeaseReplyMap[reply] = request;
}

void PiTVDesktopViewer::serverStatusHttpRequestFinished(QNetworkReply* reply)
{
	Q_ASSERT(serverStatusReplyMap.count(reply) == 1);
	ServerStatusRequest requestData = serverStatusReplyMap[reply];
	serverStatusReplyMap.remove(reply);

	if (requestData.serverListItem)
	{
		QIcon statusIcon;
		QString errorMsg;
		if (reply->error() == QNetworkReply::NoError)
		{
			statusIcon = QIcon(":/icons/success.png");
			errorMsg = "OK";
		}
		else
		{
			statusIcon = QIcon(":/icons/failure.png");
			errorMsg = reply->errorString();
		}

		requestData.serverListItem->setIcon(statusIcon);
		updateServerListItemText(requestData.serverListItem, true, errorMsg);
	}
	if (requestData.doUpdateStatusBar)
	{
		QString jsonResponse = QString(reply->readAll());
		ServerStatusMessage serverStatus;
		bool jsonOk = ServerStatusMessage::fromJson(jsonResponse, serverStatus);
		if (reply->error() == QNetworkReply::NoError && jsonOk)
		{
			QString loadCpuProcess;
			QString loadCpuTotal;
			QString tempCpu;
			if (serverStatus.isLoadCpuProcessOk)
			{
				loadCpuProcess = QString("%1%").arg(serverStatus.loadCpuProcess);
			}
			else
			{
				loadCpuProcess = "N/A";
			}

			if (serverStatus.isLoadCpuTotalOk)
			{

				loadCpuTotal = QString("%1%").arg(serverStatus.loadCpuTotal);
			}
			else
			{
				loadCpuTotal = "N/A";
			}

			if (serverStatus.isTemperatureCpuOk)
			{
				tempCpu = QString("%1 C").arg(serverStatus.temperatureCpu);
			}
			else
			{
				tempCpu = "N/A";
			}

			updateStatusBarServerStatus(loadCpuProcess, loadCpuTotal, tempCpu);
		}
		else
		{

		}
	}

	reply->deleteLater();
}

void PiTVDesktopViewer::serverStatusSslErrors(QNetworkReply* reply)
{
}

void PiTVDesktopViewer::cameraLeaseHttpRequestFinished(QNetworkReply* reply)
{
	Q_ASSERT(cameraLeaseReplyMap.count(reply) == 1);
	CameraLeaseRequest& requestData = cameraLeaseReplyMap[reply];
	cameraLeaseReplyMap.remove(reply);

	bool disconnect = true;
	if (reply->error() == QNetworkReply::NoError)
	{
		disconnect = false;

		QJsonDocument d = QJsonDocument::fromJson(reply->readAll());
		QJsonObject root = d.object();
		activeLeaseRequest.leaseGuid = root["guid"].toString();
	}
	else
	{
		QMessageBox::critical(this, "Failed to lease camera", reply->errorString());
	}

	if (disconnect)
	{
		serverStatusReplyMap.remove(reply);
		leaseUpdateTimer->stop();
	}

	reply->deleteLater();
}

void PiTVDesktopViewer::updateServerListItemText(QListWidgetItem* item, bool isInitialized, QString errorStr) const
{
	QMap<QString, QVariant> serverDataMap = item->data(Qt::UserRole).toMap();

	QString text = (serverDataMap["serverAddress"].toString() + " " +
		"(" + serverDataMap["username"].toString() + ")"
		);

	if (isInitialized)
	{
		text += " [" + errorStr + "]";
	}

	item->setText(text);
}

void PiTVDesktopViewer::updateStatusBarServerStatus(QString loadCpuProcess, QString loadCpuTotal, QString tempCpu)
{
	serverCpuProcessLoadValue->setText(QString("CPU (process): %1").arg(loadCpuProcess));
	serverCpuTotalLoadValue->setText(QString("CPU (total): %1").arg(loadCpuTotal));
	serverTempCpuValue->setText(QString("Temperature (CPU): %1").arg(tempCpu));
}

void PiTVDesktopViewer::loadServerConfigs()
{
	QFile file;
	file.setFileName("servers.json");
	if (!file.exists())
	{
		return;
	}

	QList<ServerConfig> serverConfigs;
	ServerConfigStorage storage("servers.json");
	if (!storage.loadAllConfigs(serverConfigs))
	{
		QMessageBox::critical(this, "Config storage error", "Failed to load server configurations!", QMessageBox::StandardButton::Ok);
	}

	for (const ServerConfig& config : serverConfigs)
	{
		QMap<QString, QVariant> serverDataMap;
		serverDataMap["serverAddress"] = config.serverUrl;
		serverDataMap["username"] = config.username;
		serverDataMap["password"] = config.password;

		QListWidgetItem* item = new QListWidgetItem(ui.serverListWidget);
		item->setData(Qt::UserRole, serverDataMap);
		item->setIcon(QIcon(":/icons/unknown.png"));
		updateServerListItemText(item, false, "");

		ServerStatusRequest request;
		request.serverConfig = ServerConfig(serverDataMap["serverAddress"].toString(), serverDataMap["username"].toString(), serverDataMap["password"].toString());
		request.serverListItem = item;
		requestServerStatus(request);
	}
}

void PiTVDesktopViewer::saveServerConfigs()
{
	QList<ServerConfig> serverConfigs;
	int itemNum = ui.serverListWidget->count();
	for (int i = 0; i < itemNum; i++)
	{
		QListWidgetItem* serverConfigItem = ui.serverListWidget->item(i);
		QMap<QString, QVariant> serverDataMap = serverConfigItem->data(Qt::UserRole).toMap();
		ServerConfig serverConfig;
		serverConfig.serverUrl = serverDataMap["serverAddress"].toString();
		serverConfig.username = serverDataMap["username"].toString();
		serverConfig.password = serverDataMap["password"].toString();
		serverConfigs.append(serverConfig);
	}

	ServerConfigStorage storage("servers.json");
	if (!storage.saveAllConfigs(serverConfigs))
	{
		QMessageBox::critical(this, "Config storage error", "Failed to save server configurations!", QMessageBox::StandardButton::Ok);
	}
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
		delete editServerDialog;
		return;
	}

	QMap<QString, QVariant> serverDataMap;
	serverDataMap["serverAddress"] = editServerDialog->getServerAddress();
	serverDataMap["username"] = editServerDialog->getUsername();
	serverDataMap["password"] = editServerDialog->getPassword();

	QListWidgetItem* item = new QListWidgetItem(ui.serverListWidget);
	item->setData(Qt::UserRole, serverDataMap);
	item->setIcon(QIcon(":/icons/unknown.png"));
	updateServerListItemText(item, false, "");

	ServerStatusRequest request;
	request.serverConfig = ServerConfig(editServerDialog->getServerAddress(), editServerDialog->getUsername(), editServerDialog->getPassword());
	request.serverListItem = item;
	requestServerStatus(request);

	delete editServerDialog;

	saveServerConfigs();
}

void PiTVDesktopViewer::onEditServerClicked()
{
	auto selectedItems = ui.serverListWidget->selectedItems();
	if (selectedItems.size() != 1)
	{
		return;
	}

	QListWidgetItem* item = selectedItems[0];
	QMap<QString, QVariant> serverDataMap = item->data(Qt::UserRole).toMap();

	EditServerDialog* editServerDialog = new EditServerDialog();
	editServerDialog->setServerAddress(serverDataMap["serverAddress"].toString());
	editServerDialog->setUsername(serverDataMap["username"].toString());
	editServerDialog->setPassword(serverDataMap["password"].toString());

	editServerDialog->setWindowModality(Qt::ApplicationModal);
	editServerDialog->exec();

	if (!editServerDialog->result())
	{
		return;
	}

	serverDataMap["serverAddress"] = editServerDialog->getServerAddress();
	serverDataMap["username"] = editServerDialog->getUsername();
	serverDataMap["password"] = editServerDialog->getPassword();

	item->setData(Qt::UserRole, serverDataMap);
	item->setIcon(QIcon(":/icons/unknown.png"));
	updateServerListItemText(item, false, "");

	ServerStatusRequest request;
	request.serverConfig = ServerConfig(editServerDialog->getServerAddress(), editServerDialog->getUsername(), editServerDialog->getPassword());
	request.serverListItem = item;
	requestServerStatus(request);

	delete editServerDialog;

	saveServerConfigs();
}

void PiTVDesktopViewer::onRemoveServerClicked()
{
	auto selectedItems = ui.serverListWidget->selectedItems();
	for (auto item : selectedItems)
	{
		ui.serverListWidget->removeItemWidget(item);
		delete item;
	}
}

void PiTVDesktopViewer::onServerDoubleClicked(QListWidgetItem* item)
{
	QMap<QString, QVariant> serverDataMap = item->data(Qt::UserRole).toMap();
	activeServerConfig.serverUrl = serverDataMap["serverAddress"].toString();
	activeServerConfig.username = serverDataMap["username"].toString();
	activeServerConfig.password = serverDataMap["password"].toString();

	int port = 5000;

	if (!pipeline)
	{
		pipeline.reset(new Pipeline(port, ui.videoViewer->winId()));
	}

	if (!pipeline->isPipelinePlaying() && !pipeline->startPipeline())
	{
		QMessageBox::critical(this, "Pipeline error", "Failed to start the pipeline!", QMessageBox::StandardButton::Ok);
		pipeline.reset(nullptr);
		return;
	}

	activeLeaseRequest.serverConfig = activeServerConfig;
	activeLeaseRequest.leaseTimeMsec = 10000;
	activeLeaseRequest.udpAddress = "192.168.0.2";
	activeLeaseRequest.udpPort = port;
	activeLeaseRequest.leaseGuid = "";

	requestCameraLease(activeLeaseRequest);
	leaseUpdateTimer->start(5000);
}

void PiTVDesktopViewer::onPipelinePollTimerElapsed()
{
	if (!pipeline)
	{
		return;
	}

	pipeline->busPoll();
}

void PiTVDesktopViewer::onLeaseUpdateTimerElapsed()
{
	if (activeServerConfig.serverUrl.isEmpty())
	{
		return;
	}

	ServerStatusRequest statusUpdateRequest;
	statusUpdateRequest.doUpdateStatusBar = true;
	statusUpdateRequest.serverConfig = activeServerConfig;
	statusUpdateRequest.serverListItem = nullptr;
	requestServerStatus(statusUpdateRequest);

	requestCameraLease(activeLeaseRequest);
}
