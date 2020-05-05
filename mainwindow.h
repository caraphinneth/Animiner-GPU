#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void save_settings();

    QPlainTextEdit* logger;
    QProcess* miner;
    QString miner_command;
};