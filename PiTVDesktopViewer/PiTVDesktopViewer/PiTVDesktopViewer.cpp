#pragma once

#include "PiTVDesktopViewer.h"
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QSslKey>
#include "EditServerDialog.h"
#include "ServerConfig.h"
#include "ServerConfigStorage.h"
#include "QMessageBox"

void PipelineAsyncConstructor::run()
{
	qDebug() << "Constructing pipeline in thread " << QThread::currentThreadId();
	
	Pipeline* pipeline = new Pipeline(request.udpPort, windowHandle);
	if (!pipeline->constructPipeline())
	{
		delete pipeline;
		pipeline = nullptr;
	}
	

	emit pipelineConstructed(pipeline, request);
}


PipelineAsyncConstructor::PipelineAsyncConstructor(WId handle, const CameraLeaseRequest& request)
{
	windowHandle = handle;
	this->request = request;
}


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
	connect(ui.refreshServersButton, &QPushButton::clicked, this, &PiTVDesktopViewer::onServersRefreshClicked);

	loadServerConfigs();
}

PiTVDesktopViewer::~PiTVDesktopViewer()
{

}

void PiTVDesktopViewer::onDisconnectClicked()
{
	if (activeLeaseRequest.serverConfig.serverUrl.isEmpty())
	{
		return;
	}

	disconnectFromCamera(activeLeaseRequest);
}

bool PiTVDesktopViewer::requestServerStatus(const ServerStatusRequest& request)
{
	QUrl url(request.serverConfig.serverUrl + "/status");

	QNetworkRequest netRequest = QNetworkRequest(url);
	QSslConfiguration sslConfig;

	if (!QFileInfo(request.serverConfig.tlsCaPath).exists())
	{
		return false;
	}

	QSslCertificate ca = loadCertificate(request.serverConfig.tlsCaPath);
	sslConfig.addCaCertificate(ca);

	if (QFileInfo(request.serverConfig.tlsClientCertPath).exists())
	{
		QSslCertificate cert = loadCertificate(request.serverConfig.tlsClientCertPath);
		sslConfig.setLocalCertificate(cert);
	}

	if (QFileInfo(request.serverConfig.tlsClientKeyPath).exists())
	{
		QSslKey key = loadKey(request.serverConfig.tlsClientKeyPath);
		sslConfig.setPrivateKey(key);
	}

	netRequest.setSslConfiguration(sslConfig);

	QNetworkReply* reply = netAccessManager.get(netRequest);
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { serverStatusHttpRequestFinished(reply); });
#if QT_CONFIG(ssl)
	connect(reply, &QNetworkReply::sslErrors, this, [this, reply](QList<QSslError> errors) {this->serverStatusSslErrors(reply, errors); });
#endif

	serverStatusReplyMap[reply] = request;

	return true;
}

bool PiTVDesktopViewer::requestCameraLease(const CameraLeaseRequest& request)
{
	QUrl url(request.serverConfig.serverUrl + "/camera");
	QNetworkRequest netRequest = QNetworkRequest(url);
	QSslConfiguration sslConfig;

	if (!QFileInfo(request.serverConfig.tlsCaPath).exists())
	{
		return false;
	}

	QSslCertificate ca = loadCertificate(request.serverConfig.tlsCaPath);
	sslConfig.addCaCertificate(ca);

	if (QFileInfo(request.serverConfig.tlsClientCertPath).exists())
	{
		QSslCertificate cert = loadCertificate(request.serverConfig.tlsClientCertPath);
		sslConfig.setLocalCertificate(cert);
	}

	if (QFileInfo(request.serverConfig.tlsClientKeyPath).exists())
	{
		QSslKey key = loadKey(request.serverConfig.tlsClientKeyPath);
		sslConfig.setPrivateKey(key);
	}

	netRequest.setSslConfiguration(sslConfig);

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

	netRequest.setRawHeader("Authorization", headerData.toLocal8Bit());
	netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	QNetworkReply* reply = netAccessManager.post(netRequest, postPayload);
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { cameraLeaseHttpRequestFinished(reply); });

	cameraLeaseReplyMap[reply] = request;
	return true;
}

void PiTVDesktopViewer::disconnectFromCamera(const CameraLeaseRequest& request)
{
	QJsonDocument requestJsonDoc;
	QJsonObject jsonObj = requestJsonDoc.object();
	jsonObj["lease_guid"] = request.leaseGuid;

	// lease_time == 0 will end camera lease
	jsonObj["lease_time"] = 0;

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

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { cameraEndLeaseRequestFinished(reply); });

	leaseUpdateTimer->stop();
	activeLeaseRequest = CameraLeaseRequest();
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

void PiTVDesktopViewer::serverStatusSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
	QString errorString;
	int errorsNum = 0;
	for (const QSslError& error : errors)
	{
		if (error.error() == QSslError::HostNameMismatch)
		{
			continue;
		}
		errorsNum++;

		if (!errorString.isEmpty())
			errorString += '\n';
		errorString += error.errorString();
	}

	if (errorsNum == 0 || QMessageBox::warning(this, tr("TLS Errors"),
		tr("One or more TLS errors has occurred:\n%1").arg(errorString),
		QMessageBox::Ignore | QMessageBox::Abort)
		== QMessageBox::Ignore)
	{
		reply->ignoreSslErrors();
	}
}

void PiTVDesktopViewer::cameraLeaseHttpRequestFinished(QNetworkReply* reply)
{
	Q_ASSERT(cameraLeaseReplyMap.count(reply) == 1);
	CameraLeaseRequest& requestData = cameraLeaseReplyMap[reply];
	cameraLeaseReplyMap.remove(reply);

	if (reply->error() == QNetworkReply::NoError)
	{
		QJsonDocument d = QJsonDocument::fromJson(reply->readAll());
		QJsonObject root = d.object();
		activeLeaseRequest.leaseGuid = root["guid"].toString();
	}
	else
	{
		leaseUpdateTimer->stop();
		activeLeaseRequest = CameraLeaseRequest();
		QMessageBox::critical(this, "Failed to lease camera", reply->errorString());
	}

	reply->deleteLater();
}

void PiTVDesktopViewer::cameraEndLeaseRequestFinished(QNetworkReply* reply)
{
	reply->deleteLater();
}

void PiTVDesktopViewer::updateServerListItemText(QListWidgetItem* item, bool isInitialized, QString errorStr) const
{
	ServerConfig serverConfig = qvariant_cast<ServerConfig>(item->data(Qt::UserRole));

	QString text = (serverConfig.serverUrl + " " + "(" + serverConfig.username + ")");

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
		QListWidgetItem* item = new QListWidgetItem(ui.serverListWidget);
		QVariant itemVariant;
		itemVariant.setValue(config);
		item->setData(Qt::UserRole, itemVariant);
		item->setIcon(QIcon(":/icons/unknown.png"));
		updateServerListItemText(item, false, "");

		ServerStatusRequest request;
		request.serverConfig = config;
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
		ServerConfig serverConfig = qvariant_cast<ServerConfig>(serverConfigItem->data(Qt::UserRole));
		serverConfigs.append(serverConfig);
	}
	ServerConfigStorage storage("servers.json");
	if (!storage.saveAllConfigs(serverConfigs))
	{
		QMessageBox::critical(this, "Config storage error", "Failed to save server configurations!", QMessageBox::StandardButton::Ok);
	}
}

void PiTVDesktopViewer::connectToCamera(const CameraLeaseRequest& request)
{
	if (!pipeline)
	{
		if (!pipelineContructorThread)
		{
			qDebug() << "Creating PipelineAsyncConstructor in thread " << QThread::currentThreadId();
			pipelineContructorThread = new PipelineAsyncConstructor(ui.videoViewer->winId(), request);
			connect(pipelineContructorThread, &PipelineAsyncConstructor::pipelineConstructed, this, &PiTVDesktopViewer::onPipelineConstructed);
			connect(pipelineContructorThread, &PipelineAsyncConstructor::finished, pipelineContructorThread, &QObject::deleteLater);
			pipelineContructorThread->start();
		}
		return;
	}

	if (!pipeline->isPipelinePlaying() && !pipeline->startPipeline())
	{
		QMessageBox::critical(this, "Pipeline error", "Failed to start the pipeline!", QMessageBox::StandardButton::Ok);
		pipeline.reset(nullptr);
		return;
	}

	activeLeaseRequest = request;
	requestCameraLease(activeLeaseRequest);
	leaseUpdateTimer->start(5000);
}

QSslKey PiTVDesktopViewer::loadKey(const QString& path) const
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		return QSslKey();
	}

	QByteArray data = file.readAll();
	file.close();

	return QSslKey(data, QSsl::KeyAlgorithm::Rsa);
}

QSslCertificate PiTVDesktopViewer::loadCertificate(const QString& path) const
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		return QSslCertificate();
	}

	QByteArray data = file.readAll();
	file.close();

	return QSslCertificate(data);
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

	QVariant itemDataVariant;
	ServerConfig config = qvariant_cast<ServerConfig>(itemDataVariant);
	editServerDialog->writeToServerConfig(config);
	itemDataVariant.setValue(config);

	QListWidgetItem* item = new QListWidgetItem(ui.serverListWidget);
	item->setData(Qt::UserRole, itemDataVariant);
	item->setIcon(QIcon(":/icons/unknown.png"));
	updateServerListItemText(item, false, "");

	ServerStatusRequest request;
	request.serverConfig = ServerConfig(editServerDialog->getServerAddress(), editServerDialog->getUsername(), editServerDialog->getPassword(), editServerDialog->getLocalUdpEndpoint());
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

	QVariant itemDataVariant = item->data(Qt::UserRole);
	ServerConfig config = qvariant_cast<ServerConfig>(itemDataVariant);

	EditServerDialog* editServerDialog = new EditServerDialog();
	editServerDialog->setFromServerConfig(config);

	editServerDialog->setWindowModality(Qt::ApplicationModal);
	editServerDialog->exec();

	if (!editServerDialog->result())
	{
		return;
	}

	editServerDialog->writeToServerConfig(config);
	
	itemDataVariant.setValue(config);
	item->setData(Qt::UserRole, itemDataVariant);
	item->setIcon(QIcon(":/icons/unknown.png"));
	updateServerListItemText(item, false, "");

	ServerStatusRequest request;
	request.serverConfig = ServerConfig(editServerDialog->getServerAddress(), editServerDialog->getUsername(), editServerDialog->getPassword(), editServerDialog->getLocalUdpEndpoint());
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

	saveServerConfigs();
}

void PiTVDesktopViewer::onServerDoubleClicked(QListWidgetItem* item)
{
	CameraLeaseRequest request;
	ServerConfig activeServerConfig = qvariant_cast<ServerConfig>(item->data(Qt::UserRole));

	QString udpEndpoint = activeServerConfig.localUdpEndpoint;
	auto items = udpEndpoint.split(":");
	request.udpAddress = items[0];
	if (items.size() == 2)
	{
		request.udpPort = items[1].toInt();
	}
	else
	{
		QMessageBox::critical(this, "Bad input", "Failed to parse Local UDP Endpoint because of bad format!", QMessageBox::StandardButton::Ok);
		return;
	}

	request.serverConfig = activeServerConfig;
	request.leaseTimeMsec = 10000;
	
	request.leaseGuid = "";

	connectToCamera(request);
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
	if (activeLeaseRequest.serverConfig.serverUrl.isEmpty())
	{
		return;
	}

	ServerStatusRequest statusUpdateRequest;
	statusUpdateRequest.doUpdateStatusBar = true;
	statusUpdateRequest.serverConfig = activeLeaseRequest.serverConfig;
	statusUpdateRequest.serverListItem = nullptr;
	requestServerStatus(statusUpdateRequest);

	requestCameraLease(activeLeaseRequest);
}

void PiTVDesktopViewer::onPipelineConstructed(Pipeline* pipeline, const CameraLeaseRequest& request)
{
	qDebug() << "onPipelineConstructed invoked in thread " << QThread::currentThreadId();

	pipelineContructorThread = nullptr;
	this->pipeline.reset(pipeline);
	if (pipeline)
	{
		connectToCamera(request);
	}
	else
	{
		QMessageBox::critical(this, "Pipeline construction error", "Failed to construct the pipeline!", QMessageBox::StandardButton::Ok);
	}
}

void PiTVDesktopViewer::onServersRefreshClicked()
{
	for (int i = 0; i < ui.serverListWidget->count(); i++)
	{
		QListWidgetItem* item = ui.serverListWidget->item(i);
		Q_ASSERT(item);
		QVariant data = item->data(Qt::UserRole);
		ServerConfig config = qvariant_cast<ServerConfig>(data);
		ServerStatusRequest statusRequest;
		statusRequest.serverConfig = config;
		statusRequest.serverListItem = item;
		statusRequest.doUpdateStatusBar = false;
		if (!requestServerStatus(statusRequest))
		{
			QMessageBox::critical(this, "Status request error", "Failed to send sever status request! Check server configuration!",
				QMessageBox::StandardButton::Ok);
		}
	}
}
