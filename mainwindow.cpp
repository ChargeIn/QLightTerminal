/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include"terminalwidget.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    terminal = new TerminalWidget();

    this->setCentralWidget(terminal);
}

MainWindow::~MainWindow()
{
    delete ui;
}

