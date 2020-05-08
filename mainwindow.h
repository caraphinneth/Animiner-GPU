#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void save_settings();
    void check_for_updates();
    void download_updates();
    void get_ready_to_work();
    void process_github_release_info();
    void start_mining();

    void set_miner_command (QString vendor);

    QPlainTextEdit* logger;
    QProcess* miner;
    QString miner_command;
    QStringList download_links;

    QNetworkAccessManager* network;
    QNetworkReply* get_github_release_info;
    QNetworkReply* download_miner;

    QComboBox* gpu_type;
    QLineEdit* input_address;
    QLineEdit* input_url;
    QSpinBox* input_intensity;
    QPushButton* big_button;
    QStatusBar* status_bar;
};