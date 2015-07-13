#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
//    ui->wCluster->addItem("Hopper");
//    ui->wCluster->addItem("Turing");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_wCluster_currentIndexChanged(const QString &arg1)
{
    ui->statusBar->showMessage(arg1);
}
