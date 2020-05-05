#include "mainwindow.h"

#include "QCheckBox"
#include "QComboBox"
#include "QGroupBox"
#include "QLabel"
#include "QPushButton"
#include "QSettings"
#include "QVBoxLayout"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QSettings settings;

    settings.beginGroup ("MainWindow");
    resize (settings.value ("size", QSize (640, 480)).toSize());
    move (settings.value ("pos", QPoint (200, 200)).toPoint());
    settings.endGroup();

    QWidget *central_widget = new QWidget (this);

    QGroupBox* settings_box = new QGroupBox (tr("Settings"), this);

    QLabel* label1 = new QLabel (tr("Select GPU vendor:"), this);
    QComboBox* gpu_type = new QComboBox (this);
    gpu_type->insertItem (0, "NVidia");
    gpu_type->insertItem (1, "AMD");

    settings.beginGroup ("GPU");
    QString vendor = settings.value ("Vendor", "NVidia").toString();
    gpu_type->setCurrentText (vendor);
    settings.endGroup();
    miner_command = (vendor == "NVidia") ? "ccminer" : "wildrig";

    QLabel* label2 = new QLabel (tr("Your Animecoin wallet address:"));
    input_address = new QLineEdit (this);

    QLabel* label3 = new QLabel (tr("Intensity (NVidia only):"));
    input_intensity = new QSpinBox (this);
    QLabel* label4 = new QLabel (tr("Lower intensity means less GPU load for the sake of other applications. Miner may fail to start with overly high (>25) intensity values."));

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

    input_intensity->setRange (9, 31);
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

    QPushButton* big_button = new QPushButton (tr("Start"), this);
    logger = new QPlainTextEdit (this);
    logger->setTextInteractionFlags (Qt::TextBrowserInteraction);

    QVBoxLayout* layout = new QVBoxLayout (central_widget);
    layout->addWidget (settings_box);
    layout->addStretch (1);
    layout->addWidget (big_button);
    layout->addWidget (logger);
    central_widget->setLayout (layout);
    setCentralWidget (central_widget);

    miner = new QProcess (this);
    miner->setProcessChannelMode (QProcess::MergedChannels);


    connect (miner, &QProcess::stateChanged, [this, big_button](QProcess::ProcessState new_state)
    {
        if (new_state == QProcess::Starting)
        {
            big_button->setText (tr("Starting..."));
            big_button->setEnabled (false);
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
                logger->appendPlainText (s);
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
            miner_command = "ccminer";
            input_intensity->setEnabled (true);
            settings.setValue ("Vendor", "NVidia");
        }
        else if (index == 1)
        {
            miner_command = "wildrig";
            input_intensity->setEnabled (false);
            settings.setValue ("Vendor", "AMD");
        }
        settings.endGroup ();
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

    connect (big_button, &QPushButton::clicked, [this, gpu_type]()
    {
        if (miner->state() == QProcess::NotRunning)
        {
            QStringList args;
            args << "-a" << "anime" << "-o" << input_url->text() << "-u" << input_address->text() << "-p" << "c=ANI";
            if ((gpu_type->currentIndex() == 0)&&(input_intensity->value() > 9))
                args << "-i" << input_intensity->cleanText();
            miner->start (miner_command, args);
        }
        else
        {
            miner->kill();
            logger->appendPlainText (tr("Mining stopped by user."));
        }
    });

    logger->appendPlainText (tr("Animiner ready to start..."));
}

MainWindow::~MainWindow()
{
    save_settings();
    miner->kill();
}

void MainWindow::save_settings()
{
    QSettings settings;

    settings.beginGroup ("MainWindow");
    settings.setValue ("size", size());
    settings.setValue ("pos", pos());
    settings.endGroup();
}