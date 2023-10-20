#pragma once

#include "PiTVDesktopViewer.h"
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include "EditServerDialog.h"
#include "ServerConfig.h"
#include "ServerConfigStorage.h"
#include "QMessageBox"

PiTVDesktopViewer::PiTVDesktopViewer(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

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
	QUrl url(request.serverHostname + "/status");
	QNetworkReply* reply = netAccessManager.get(QNetworkRequest(url));
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, this, [this, reply]() { serverStatusHttpRequestFinished(reply); });
#if QT_CONFIG(ssl)
	connect(reply, &QNetworkReply::sslErrors, this, [this, reply]() { serverStatusSslErrors(reply); });
#endif

	serverStatusReplyMap[reply] = request;
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
	if (requestData.isActiveConnection)
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
		request.serverHostname = config.serverUrl;
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
	request.serverHostname = editServerDialog->getServerAddress();
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
	request.serverHostname = editServerDialog->getServerAddress();
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
