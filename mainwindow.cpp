#include "arc.h"
#include "mainwindow.h"

#include "QCheckBox"
#include "QDir"
#include "QGroupBox"
#include "QJsonDocument"
#include "QJsonObject"
#include "QJsonArray"
#include "QLabel"
#include "QNetworkReply"
#include "QSettings"
#include "QVBoxLayout"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QSettings settings;

    network = new QNetworkAccessManager (this);

    settings.beginGroup ("MainWindow");
    resize (settings.value ("size", QSize (640, 480)).toSize());
    move (settings.value ("pos", QPoint (200, 200)).toPoint());
    settings.endGroup();

    QWidget *central_widget = new QWidget (this);

    QGroupBox* settings_box = new QGroupBox (tr("Settings"), this);

    QLabel* label1 = new QLabel (tr("Select GPU vendor:"), this);
    gpu_type = new QComboBox (this);
    gpu_type->insertItem (0, "NVidia");
    gpu_type->insertItem (1, "AMD");

    settings.beginGroup ("GPU");
    QString vendor = settings.value ("Vendor", "NVidia").toString();
    gpu_type->setCurrentText (vendor);
    settings.endGroup();
    set_miner_command (vendor);

    QLabel* label2 = new QLabel (tr("Your Animecoin wallet address:"));
    input_address = new QLineEdit (this);

    QLabel* label3 = new QLabel (tr("Intensity:"));
    input_intensity = new QSpinBox (this);
    QLabel* label4 = new QLabel (tr("Lower intensity means less GPU load for the sake of other applications.<br/>Miner may fail to start with overly high intensity values."));

    QLabel* label5 = new QLabel (tr("Mining pool URL:"));
    input_url = new QLineEdit (this);

    settings.beginGroup ("Arguments");
    QString address = settings.value ("Address", "").toString();
    if (!address.isEmpty())
        input_address->setText (address);

    QString url = settings.value ("URL", "stratum+tcp://miningbase.tk:3033").toString();
    if (!url.isEmpty())
        input_url->setText (url);

    int intensity = settings.value ("Intensity", 9).toInt();
    input_intensity->setValue (intensity);
    settings.endGroup();

    if (vendor == "NVidia")
        input_intensity->setRange (9, 31);
    else
        input_intensity->setRange (0, 4000);
    input_intensity->setSpecialValueText (tr("Automatic"));

    QVBoxLayout* settings_layout = new QVBoxLayout (central_widget);
    settings_layout->addWidget (label1);
    settings_layout->addWidget (gpu_type);
    settings_layout->addWidget (label2);
    settings_layout->addWidget (input_address);
    settings_layout->addWidget (label3);
    settings_layout->addWidget (input_intensity);
    settings_layout->addWidget (label4);
    settings_layout->addWidget (label5);
    settings_layout->addWidget (input_url);

    settings_box->setLayout (settings_layout);

    big_button = new QPushButton (tr("Start"), this);
    logger = new QPlainTextEdit (this);
    logger->setTextInteractionFlags (Qt::TextBrowserInteraction);

    QVBoxLayout* layout = new QVBoxLayout (central_widget);
    layout->addWidget (settings_box);
    layout->addStretch (1);
    layout->addWidget (big_button);
    layout->addWidget (logger);
    central_widget->setLayout (layout);
    setCentralWidget (central_widget);

    status_bar = new QStatusBar (this);
    setStatusBar (status_bar);

    miner = new QProcess (this);
    miner->setProcessChannelMode (QProcess::MergedChannels);


    connect (miner, &QProcess::stateChanged, [this](QProcess::ProcessState new_state)
    {
        if (new_state == QProcess::Starting)
        {
            big_button->setText (tr("Starting..."));
            big_button->setEnabled (false);
            status_bar->showMessage (tr("Starting..."));
        }
        else if (new_state == QProcess::Running)
        {
            big_button->setText (tr("Stop"));
            big_button->setEnabled (true);
        }
        else
        {
            big_button->setText (tr("Start"));
            big_button->setEnabled (true);
            logger->appendPlainText (tr("Mining stopped with code %1.").arg(miner->exitCode()));
            status_bar->showMessage (tr("Mining stopped."));
        }
    });

    connect (miner, &QProcess::readyReadStandardOutput, [this]()
    {
        while (miner->canReadLine())
        {
            QString s = tr (miner->readLine());

            // Filter out ANSI and linebreaks.
            s.remove (QRegExp ("\x1b\[[0-9;]*[a-zA-Z]"));
            s.remove ("\n", Qt::CaseInsensitive);
            if (!s.isEmpty())
            {
                logger->appendPlainText (s);
                status_bar->showMessage (tr("Mining..."));
            }
        }
    });

    connect (miner, &QProcess::errorOccurred, [this](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart)
            logger->appendPlainText ("Miner failed to start. Likely the executable is missing.");
    });

    connect (gpu_type, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index)
    {
        QSettings settings;
        settings.beginGroup ("GPU");
        if (index == 0)
        {
            input_intensity->setRange (9, 31);
            input_intensity->setValue (9);

            settings.setValue ("Vendor", "NVidia");
        }
        else if (index == 1)
        {
            input_intensity->setRange (0, 4000);
            input_intensity->setValue (0);

            settings.setValue ("Vendor", "AMD");
        }
        set_miner_command (gpu_type->currentText());
        settings.endGroup ();
        check_for_updates();
    });

    connect (input_address, &QLineEdit::textChanged, [this](const QString& text)
    {
        QSettings settings;
        settings.beginGroup ("Arguments");
        settings.setValue ("Address", text);
        settings.endGroup();
    });

    connect (input_url, &QLineEdit::textChanged, [this](const QString& text)
    {
        QSettings settings;
        settings.beginGroup ("Arguments");
        settings.setValue ("URL", text);
        settings.endGroup();
    });

    connect (input_intensity, QOverload<int>::of(&QSpinBox::valueChanged), [this](int i)
    {
        QSettings settings;
        settings.beginGroup ("Arguments");
        settings.setValue ("Intensity", i);
        settings.endGroup();
    });

    check_for_updates();
}

MainWindow::~MainWindow()
{
    save_settings();
    miner->kill();
    miner->waitForFinished();
}

// Stage 1: address GitHub api for the latest releases.
// Since network request is non-blocking, wait for the finished() signal.
void MainWindow::check_for_updates()
{
    disconnect (big_button, &QPushButton::clicked, this, &MainWindow::start_mining);
    disconnect (big_button, &QPushButton::clicked, this, &MainWindow::check_for_updates);

    gpu_type->setEnabled (false);
    logger->appendPlainText (tr("Checking for updates for %1 miner...").arg(gpu_type->currentText()));
    status_bar->showMessage (tr("Checking for miner updates..."));
    big_button->setText (tr("Abort updating"));

    if (gpu_type->currentIndex() == 0)
        get_github_release_info = network->get (QNetworkRequest (QUrl ("https://api.github.com/repos/testzcrypto/ccminer/releases")));
    else if (gpu_type->currentIndex() == 1)
        get_github_release_info = network->get (QNetworkRequest (QUrl ("https://api.github.com/repos/andru-kun/wildrig-multi/releases")));

    connect (big_button, &QPushButton::clicked, get_github_release_info, &QNetworkReply::abort);
    connect (get_github_release_info, &QNetworkReply::finished, this, &MainWindow::process_github_release_info);
}

// Stage 2: download and unpack the miner into cache folder.
void MainWindow::download_updates()
{
    QUrl best_download_link;
    if (download_links.size() > 1)
        foreach (QString link, download_links)
        {
#ifdef Q_OS_WIN
            if (link.contains ("windows"))
#else
            if (link.contains ("linux"))
#endif
            {
                best_download_link = QUrl::fromUserInput(link);
                break;
            }
        }
    else if (download_links.isEmpty())
    {
        get_ready_to_work();
        return;
    }
    else
        best_download_link = QUrl::fromUserInput(download_links.first());

    QString filename = QDir::toNativeSeparators (cache_dir + "/" + best_download_link.fileName());
    if (!QFile::exists (filename))
    {
        logger->appendPlainText (tr("Update found, downloading..."));
        QNetworkRequest req (best_download_link);
        req.setAttribute (QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        download_miner = network->get (req);

        connect (big_button, &QPushButton::clicked, download_miner, &QNetworkReply::abort);
        connect (download_miner, &QNetworkReply::downloadProgress, [this](qint64 received, qint64 total)
        {
            status_bar->showMessage (tr("Downloading %1%").arg(100*received/total));
        });
        connect (download_miner, &QNetworkReply::finished, [this, filename]()
        {
            if (download_miner->error() == QNetworkReply::NoError)
            {
                QFile* file = new QFile (filename);
                if (!file->open (QIODevice::WriteOnly))
                {
                    logger->appendPlainText (tr("Saving failed, could not write to temp directory!"));
                }
                else
                {
                    file->write (download_miner->readAll());
                    file->close();
                    logger->appendPlainText (tr("Unpacking..."));
                    extract (filename.toUtf8().data());
                    logger->appendPlainText (tr("Unpacking done."));
                }
            }
            else if (download_miner->error() == QNetworkReply::OperationCanceledError)
            {
                logger->appendPlainText (tr ("Download cancelled by user."));
            }
            else
            {
                qDebug() << download_miner->error();
                status_bar->clearMessage();
                logger->appendPlainText (tr ("Download failed! Error code: %1").arg (download_miner->error()));
            }
            disconnect (big_button, &QPushButton::clicked, download_miner, &QNetworkReply::abort);
            download_miner->deleteLater();

            get_ready_to_work();
        });
    }
    else
    {
        if (QFile::exists (miner_command))
        {
            logger->appendPlainText (tr("Up to date."));
            get_ready_to_work();
        }
        else
        {
            logger->appendPlainText (tr("Up to date, unzipping..."));
            extract (filename.toUtf8().data());
            logger->appendPlainText (tr("Done."));
            get_ready_to_work();
        }
    }
}

void MainWindow::get_ready_to_work()
{
    if (!QFile::exists (miner_command))
    {
        status_bar->showMessage (tr("Animiner not ready yet."));
        logger->appendPlainText (tr("No miner executable found. Kindly allow the upate process to complete at least once."));
        big_button->setText (tr("Update"));
        connect (big_button, &QPushButton::clicked, this, &MainWindow::check_for_updates);
    }
    else
    {
        connect (big_button, &QPushButton::clicked, this, &MainWindow::start_mining);
        status_bar->showMessage (tr("Animiner ready to start..."));
        big_button->setText (tr("Start"));
    }
    gpu_type->setEnabled (true);
}

void MainWindow::start_mining()
{
    if (miner->state() == QProcess::NotRunning)
    {

        QStringList args;
        args << "-a" << "anime" << "-o" << input_url->text() << "-u" << input_address->text() << "-p" << "c=ANI";
        if ((gpu_type->currentIndex() == 0)&&(input_intensity->value() > 9))
            args << "-i" << input_intensity->cleanText();
        else if (gpu_type->currentIndex() == 1)
        {
            if (input_intensity->value() > 0)
                args << "--opencl-launch" << input_intensity->cleanText()+"x64";
            else
                args << "--opencl-launch" << "20x64"; // workaround for new Wildrig issue.
        }
        miner->start (miner_command, args);
    }
    else
    {
        logger->appendPlainText (tr("Stopping by user request..."));
        miner->kill();
    }
}

void MainWindow::process_github_release_info()
{
    if (get_github_release_info->error() == QNetworkReply::NoError)
    {
        download_links.clear();
        QString strReply = (QString)get_github_release_info->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson (strReply.toUtf8());
        QJsonArray rootArray = jsonResponse.array();

        foreach (QJsonValue it, rootArray)
        {
            QJsonObject obj = it.toObject();
            QJsonArray jsonArray = obj["assets"].toArray();
            foreach (QJsonValue it2, jsonArray)
            {
                QJsonObject obj2 = it2.toObject();
                if (obj2.contains ("browser_download_url"))
                {
                    download_links.append (obj2["browser_download_url"].toString());
                }
            }
        }
    }
    else
    {
        logger->appendPlainText ("Failed to access "+get_github_release_info->url().toString());
    }

    disconnect (big_button, &QPushButton::clicked, get_github_release_info, &QNetworkReply::abort);
    get_github_release_info->deleteLater();

    download_updates();
}

void MainWindow::set_miner_command (QString vendor)
{
#ifdef Q_OS_WIN
    miner_command = (vendor == "NVidia") ? cache_dir+"\\win64\\ccminer.exe" : cache_dir+"\\wildrig.exe";
#else
    miner_command = (vendor == "NVidia") ? cache_dir+"/ccminer" : cache_dir+"/wildrig-multi";
#endif
}

void MainWindow::save_settings()
{
    QSettings settings;

    settings.beginGroup ("MainWindow");
    settings.setValue ("size", size());
    settings.setValue ("pos", pos());
    settings.endGroup();
}