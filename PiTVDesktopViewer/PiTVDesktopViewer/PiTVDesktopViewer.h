#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_PiTVDesktopViewer.h"

class PiTVDesktopViewer : public QMainWindow
{
    Q_OBJECT

public:
    PiTVDesktopViewer(QWidget *parent = nullptr);
    ~PiTVDesktopViewer();

private:
    Ui::PiTVDesktopViewerClass ui;

public slots:
    void onDisconnectClicked();
    void onExitClicked();
    void onAddServerClicked();
    void onEditServerClicked();
    void onRemoveServerClicked();
};
