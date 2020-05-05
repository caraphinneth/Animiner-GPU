#pragma once

#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSpinBox>

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

    QLineEdit* input_address;
    QLineEdit* input_url;
    QSpinBox* input_intensity;
};