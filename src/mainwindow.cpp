#include "mainwindow.h"
#include "airplayworker.h"
#include "mdns_responder.hpp"
#include <windows.h>
#include <winsvc.h>

#include <QProcess>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QComboBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QPixmap>
#include <QScrollArea>
#include <QTextStream>

struct RenameData {
    DWORD pid;
    QString newTitle;
};

// Callback method that Windows calls for every opened window
BOOL CALLBACK EnumWindowsProcRename(HWND hwnd, LPARAM lParam) {
    RenameData *data = reinterpret_cast<RenameData*>(lParam);
    DWORD windowPid;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == data->pid) {
        char windowTitle[512];
        if (GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle))) {
            QString title = QString::fromLocal8Bit(windowTitle);
            
            if (title.contains("Direct") && title.contains("enderer")) {
                printf("found window to rename, setting new name...\n");
                SetWindowTextW(hwnd, reinterpret_cast<const wchar_t*>(data->newTitle.utf16()));
                return FALSE;
            }
        }
    }
    return TRUE;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    ensureSettingsFileExists();
    setupTray();
    setupUI();

    // If Bonjour Service is missing, we must install it; otherwise we exit.
    if (ensureBonjourServiceInstalled()) {
        startServer();
    } else {
        m_quitting = true;
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
}

MainWindow::~MainWindow() {
    m_quitting = true;
    stopServer();
}

QString MainWindow::userArgumentsPath() const {
    QString appDataPath = QProcessEnvironment::systemEnvironment().value("APPDATA");
    if (appDataPath.isEmpty()) {
        appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return QDir(appDataPath).filePath("Swave/swave-imirror/arguments.txt");
}

QString MainWindow::machineArgumentsPath() const {
    QString programDataPath =
        QProcessEnvironment::systemEnvironment().value("ProgramData");
    if (programDataPath.isEmpty()) {
        programDataPath = "C:/ProgramData";
    }
    return programDataPath + "/swave-imirror/arguments.txt";
}

QString MainWindow::activeArgumentsPath() const {
    QString machinePath = machineArgumentsPath();
    if (QFile::exists(machinePath)) {
        return machinePath;
    }

    return userArgumentsPath();
}

QString MainWindow::expandEnvironmentVariables(const QString &content) const {
    QString expanded = content;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    for (const QString &key : env.keys()) {
        expanded.replace("%" + key + "%", env.value(key), Qt::CaseInsensitive);
    }

    return expanded;
}

void MainWindow::ensureSettingsFileExists() {
    if (QFile::exists(userArgumentsPath()) || QFile::exists(machineArgumentsPath())) {
        return;
    }

    QFileInfo info(userArgumentsPath());
    QDir().mkpath(info.absolutePath());

    QFile file(userArgumentsPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "-n swave-imirror -nh";
        file.close();
    }
}

QStringList MainWindow::getArgumentsFromFile() {
    const QString userPath = userArgumentsPath();
    const QString machinePath = machineArgumentsPath();
    const QString selectedPath = activeArgumentsPath();

    qInfo().noquote() << "[arguments] Machine file:"
                      << QDir::toNativeSeparators(machinePath)
                      << (QFile::exists(machinePath) ? "(found)" : "(not found)");
    qInfo().noquote() << "[arguments] User file:"
                      << QDir::toNativeSeparators(userPath)
                      << (QFile::exists(userPath) ? "(found)" : "(not found)");
    qInfo().noquote() << "[arguments] Reading:"
                      << QDir::toNativeSeparators(selectedPath);

    QFile file(selectedPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = expandEnvironmentVariables(QTextStream(&file).readAll().trimmed());
        file.close();
        return QProcess::splitCommand(content);
    }

    qWarning().noquote()
        << "[arguments] Unable to read the selected file; using built-in defaults.";
    return QStringList() << "-n" << "swave-imirror" << "-nh";
}

void MainWindow::setupUI() {
    setWindowTitle("swave-imirror - Modern AirPlay Receiver");
    setWindowIcon(QApplication::windowIcon());
    setMinimumSize(540, 680);
    resize(580, 720);

    setStyleSheet(R"(
        QMainWindow {
            background-color: #0F172A;
            color: #F8FAFC;
            font-family: "Segoe UI", system-ui, sans-serif;
        }
        QWidget#centralWidget {
            background-color: #0F172A;
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollBar:vertical {
            border: none;
            background: #1E293B;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #38BDF8;
            border-radius: 4px;
            min-height: 20px;
        }
        QGroupBox {
            background-color: #1E293B;
            border: 1px solid #334155;
            border-radius: 12px;
            margin-top: 24px;
            padding: 20px 16px 16px 16px;
            font-weight: 600;
            color: #38BDF8;
            font-size: 11pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 0 8px;
            background-color: #0F172A;
            border-radius: 4px;
        }
        QLabel {
            color: #94A3B8;
            font-size: 9.5pt;
        }
        QLabel#statusLabel {
            background-color: #1E293B;
            border: 1px solid #38BDF8;
            border-radius: 8px;
            color: #38BDF8;
            font-weight: 700;
            font-size: 10.5pt;
            padding: 10px;
        }
        QLineEdit, QComboBox {
            background-color: #0F172A;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 6px 12px;
            color: #F8FAFC;
            font-size: 9.5pt;
            min-height: 28px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #38BDF8;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QCheckBox {
            color: #CBD5E1;
            font-size: 9.5pt;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #334155;
            background-color: #0F172A;
        }
        QCheckBox::indicator:checked {
            background-color: #38BDF8;
            border-color: #38BDF8;
        }
        QPushButton {
            background-color: #334155;
            color: #F8FAFC;
            border: 1px solid #475569;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
            font-size: 9.5pt;
            min-height: 32px;
        }
        QPushButton:hover {
            background-color: #475569;
            border-color: #38BDF8;
        }
        QPushButton#primaryBtn {
            background-color: #2563EB;
            border: 1px solid #3B82F6;
            color: #FFFFFF;
            font-size: 10pt;
            font-weight: 700;
            min-height: 36px;
        }
        QPushButton#primaryBtn:hover {
            background-color: #1D4ED8;
            border-color: #60A5FA;
        }
    )");

    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    auto *scrollContent = new QWidget(this);
    scrollArea->setWidget(scrollContent);

    auto *layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    // Hero Header
    auto *heroLayout = new QHBoxLayout();
    heroLayout->setSpacing(16);

    auto *logoLabel = new QLabel(this);
    logoLabel->setFixedSize(72, 72);
    QPixmap iconMap(QApplication::applicationDirPath() + "/resources/logo-screen-mirror-ios.png");
    if (!iconMap.isNull()) {
        logoLabel->setPixmap(iconMap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    heroLayout->addWidget(logoLabel);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);

    auto *titleLabel = new QLabel("swave-imirror", this);
    titleLabel->setStyleSheet("color: #F8FAFC; font-size: 20pt; font-weight: 800;");
    headerTextLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("High-Performance iOS Screen Mirroring Receiver", this);
    subtitleLabel->setStyleSheet("color: #38BDF8; font-size: 9.5pt; font-weight: 600;");
    headerTextLayout->addWidget(subtitleLabel);

    heroLayout->addLayout(headerTextLayout);
    heroLayout->addStretch();
    layout->addLayout(heroLayout);

    // Status Indicator
    m_statusLabel = new QLabel("Initializing Receiver...", this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    QSettings settings;

    // Card 1: Receiver Configuration
    auto *configBox = new QGroupBox("Receiver Configuration", this);
    auto *configForm = new QFormLayout(configBox);
    configForm->setSpacing(12);
    configForm->setContentsMargins(16, 20, 16, 16);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(settings.value("receiver_name", "swave-imirror").toString());
    m_nameEdit->setMaxLength(63);
    m_nameEdit->setPlaceholderText("Name visible on iPhone");
    auto *nameLabel = new QLabel("iOS Discovery Name:", this);
    nameLabel->setStyleSheet("color: #E2E8F0; font-weight: 600;");
    configForm->addRow(nameLabel, m_nameEdit);

    m_fpsCombo = new QComboBox(this);
    m_fpsCombo->addItem("30 FPS (Standard)", 30);
    m_fpsCombo->addItem("60 FPS (High Smoothness)", 60);
    int savedFps = settings.value("target_fps", 60).toInt();
    int fpsIdx = m_fpsCombo->findData(savedFps);
    m_fpsCombo->setCurrentIndex(fpsIdx >= 0 ? fpsIdx : 1);
    auto *fpsLabel = new QLabel("Target Frame Rate:", this);
    fpsLabel->setStyleSheet("color: #E2E8F0; font-weight: 600;");
    configForm->addRow(fpsLabel, m_fpsCombo);

    m_latencyCombo = new QComboBox(this);
    m_latencyCombo->addItem("Stable (Higher Buffer)", "stable");
    m_latencyCombo->addItem("Balanced (Recommended)", "balanced");
    m_latencyCombo->addItem("Low Latency (Ultra Fast)", "low");
    QString savedLatency = settings.value("latency_mode", "balanced").toString();
    int latencyIdx = m_latencyCombo->findData(savedLatency);
    m_latencyCombo->setCurrentIndex(latencyIdx >= 0 ? latencyIdx : 1);
    auto *latLabel = new QLabel("Latency Profile:", this);
    latLabel->setStyleSheet("color: #E2E8F0; font-weight: 600;");
    configForm->addRow(latLabel, m_latencyCombo);

    m_fpsDataCheckbox = new QCheckBox("Log FPS Performance Diagnostics", this);
    m_fpsDataCheckbox->setChecked(settings.value("fps_data", false).toBool());
    configForm->addRow(QString(), m_fpsDataCheckbox);

    layout->addWidget(configBox);

    // Apply Button
    auto *restartBtn = new QPushButton("Apply Changes & Restart Receiver", this);
    restartBtn->setObjectName("primaryBtn");
    connect(restartBtn, &QPushButton::clicked, this, &MainWindow::restartApplication);
    layout->addWidget(restartBtn);

    // Card 2: System Options
    auto *optionsBox = new QGroupBox("System Options", this);
    auto *optionsLayout = new QVBoxLayout(optionsBox);
    optionsLayout->setSpacing(10);
    optionsLayout->setContentsMargins(16, 20, 16, 16);

    m_bleCheckbox = new QCheckBox("Enable Bluetooth LE Discovery Beacon", this);
    m_bleCheckbox->setChecked(settings.value("ble_enabled", true).toBool());
    connect(m_bleCheckbox, &QCheckBox::toggled, this, &MainWindow::toggleBle);
    optionsLayout->addWidget(m_bleCheckbox);

    m_autostartCheckbox = new QCheckBox("Start swave-imirror automatically at Windows Login", this);
    m_autostartCheckbox->setChecked(isAutostartEnabled());
    connect(m_autostartCheckbox, &QCheckBox::toggled, this, &MainWindow::toggleAutostart);
    optionsLayout->addWidget(m_autostartCheckbox);

    m_fullscreenCheckbox = new QCheckBox("Force Fullscreen Mode on Stream Start", this);
    m_fullscreenCheckbox->setChecked(settings.value("force_fs_enabled", false).toBool());
    connect(m_fullscreenCheckbox, &QCheckBox::toggled, this, &MainWindow::toggleForceFullscreen);
    optionsLayout->addWidget(m_fullscreenCheckbox);

    auto *rendererLayout = new QHBoxLayout();
    auto *rendLabel = new QLabel("Graphics Renderer:", this);
    rendLabel->setStyleSheet("color: #E2E8F0; font-weight: 600;");
    rendererLayout->addWidget(rendLabel);

    m_rendererCombo = new QComboBox(this);
    m_rendererCombo->addItem("Automatic (Recommended)", "auto");
    m_rendererCombo->addItem("Direct3D 11 (D3D11)", "d3d11");
    m_rendererCombo->addItem("Direct3D 12 (D3D12)", "d3d12");
    QString savedRend = settings.value("renderer_mode", "auto").toString();
    int rendIdx = m_rendererCombo->findData(savedRend);
    if (rendIdx >= 0) m_rendererCombo->setCurrentIndex(rendIdx);
    connect(m_rendererCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRendererChanged);

    rendererLayout->addWidget(m_rendererCombo, 1);
    optionsLayout->addLayout(rendererLayout);

    layout->addWidget(optionsBox);

    // Card 3: Advanced Tools
    auto *advancedBox = new QGroupBox("Tools & Support", this);
    auto *advancedLayout = new QHBoxLayout(advancedBox);
    advancedLayout->setSpacing(10);
    advancedLayout->setContentsMargins(16, 20, 16, 16);

    m_settingsBtn = new QPushButton("Open Arguments File", this);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettingsFile);
    advancedLayout->addWidget(m_settingsBtn);

    m_listargsBtn = new QPushButton("View Arg Reference", this);
    connect(m_listargsBtn, &QPushButton::clicked, this, &MainWindow::openListArgsFile);
    advancedLayout->addWidget(m_listargsBtn);

    m_licenseBtn = new QPushButton("License", this);
    connect(m_licenseBtn, &QPushButton::clicked, this, &MainWindow::showLicense);
    advancedLayout->addWidget(m_licenseBtn);

    layout->addWidget(advancedBox);

    connect(m_nameEdit, &QLineEdit::editingFinished, this, &MainWindow::persistUserSettings);
    connect(m_fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::persistUserSettings);
    connect(m_latencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::persistUserSettings);
    connect(m_fpsDataCheckbox, &QCheckBox::toggled, this, &MainWindow::persistUserSettings);

    layout->addStretch();
    updateStatus();
}

void MainWindow::persistUserSettings() {
    QSettings settings;
    QString name = m_nameEdit ? m_nameEdit->text().trimmed() : QString("swave-imirror");
    if (name.isEmpty()) name = "swave-imirror";
    settings.setValue("receiver_name", name);
    if (m_fpsCombo) settings.setValue("target_fps", m_fpsCombo->currentData().toInt());
    if (m_latencyCombo) settings.setValue("latency_mode", m_latencyCombo->currentData().toString());
    if (m_fpsDataCheckbox) settings.setValue("fps_data", m_fpsDataCheckbox->isChecked());
    m_tray->showMessage("swave-imirror", "Settings saved. Click Apply to restart receiver.",
                        QSystemTrayIcon::Information, 2500);
}

void MainWindow::openSettingsFile() {
    QString filePath = activeArgumentsPath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    
    m_tray->showMessage("Settings", "Restart the app to apply new arguments.", 
                        QSystemTrayIcon::Information, 3000);
}

void MainWindow::openListArgsFile() {
    QString filePath = QApplication::applicationDirPath() + "/resources/uxplay_arguments_list.txt";
    QFile::setPermissions(filePath, QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    QIcon trayIcon;
    QString icoPath = QApplication::applicationDirPath() + "/resources/icon.ico";
    trayIcon = QIcon(icoPath);
    if (trayIcon.isNull()) {
        trayIcon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
    }
    m_tray->setIcon(trayIcon);
    m_tray->setToolTip("swave-imirror");

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Quit", this, &MainWindow::quit);
    m_trayMenu->addAction("Restart", this, &MainWindow::restartApplication);

    m_tray->setContextMenu(m_trayMenu);
    
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        isVisible() ? hide() : showNormal();
    }
}

void MainWindow::toggleBle(bool checked) {
    QSettings settings;
    
    if (settings.value("ble_enabled").toBool() == checked) {
        return;
    }

    settings.setValue("ble_enabled", checked);
    
    m_tray->showMessage("swave-imirror", "Please restart swave-imirror to apply changes.\n(Right-click the Tray Icon)",
                        QSystemTrayIcon::Information, 3000);
}

void MainWindow::toggleForceFullscreen(bool checked) {
    QSettings settings;
    if (settings.value("force_fs_enabled").toBool() == checked) {
        return;
    }

    settings.setValue("force_fs_enabled", checked);
    m_tray->showMessage("swave-imirror", "Please restart swave-imirror to apply changes.\n(Right-click the Tray Icon)",
                        QSystemTrayIcon::Information, 3000);
}

void MainWindow::onRendererChanged(int /*index*/) {
    if (!m_rendererCombo) return;

    QString mode = m_rendererCombo->currentData().toString();

    QSettings settings;
    QString saved = settings.value("renderer_mode", "auto").toString();
    if (saved == mode) return;

    settings.setValue("renderer_mode", mode);
    m_tray->showMessage("swave-imirror", "Please restart swave-imirror to apply changes.\n(Right-click the Tray Icon)",
                        QSystemTrayIcon::Information, 3000);
}

void MainWindow::applyRendererAndFullscreenArgs(QStringList &args) {
    while (true) {
        int idx = args.indexOf("-fs");
        if (idx < 0) break;
        args.removeAt(idx);
    }
    if (m_fullscreenCheckbox && m_fullscreenCheckbox->isChecked()) {
        args << "-fs";
    }

    // Remove existing "-vs <sink>" pairs
    for (int i = 0; i < args.size();) {
        if (args[i] == "-vs") {
            args.removeAt(i); // -vs
            if (i < args.size()) {
                args.removeAt(i); // sink
            }
            continue;
        }
        ++i;
    }

    QString mode = "auto";
    if (m_rendererCombo) {
        mode = m_rendererCombo->currentData().toString();
    }

    if (mode == "d3d11") {
        args << "-vs" << "d3d11videosink";
    } else if (mode == "d3d12") {
        args << "-vs" << "d3d12videosink";
    }
}

void MainWindow::applyUserSettings(QStringList &args) const {
    auto removeOption = [&args](const QString &option, bool hasValue) {
        for (int i = 0; i < args.size();) {
            if (args[i] == option) {
                args.removeAt(i);
                if (hasValue && i < args.size() && !args[i].startsWith("-")) args.removeAt(i);
                continue;
            }
            ++i;
        }
    };

    removeOption("-n", true);
    removeOption("-fps", true);
    removeOption("-s", true);
    removeOption("-al", true);
    removeOption("-vsync", true);
    removeOption("-FPSdata", false);
    removeOption("-nh", false);

    QString name = m_nameEdit ? m_nameEdit->text().trimmed() : QString("swave-imirror");
    name.replace(QRegularExpression("[^A-Za-z0-9 _.-]"), "");
    if (name.isEmpty()) name = "swave-imirror";

    int fps = m_fpsCombo ? m_fpsCombo->currentData().toInt() : 60;
    QString latency = m_latencyCombo ? m_latencyCombo->currentData().toString() : QString("balanced");

    args << "-n" << name << "-nh" << "-s" << "1920x1080@60" << "-fps" << QString::number(fps);
    if (latency == "low") {
        // Preserve smooth timestamp pacing; reduce only the audio target
        // latency. Disabling video sync produced the reported frame drops.
        args << "-vsync" << "0.00" << "-al" << "0.05";
    } else if (latency == "stable") {
        args << "-vsync" << "0.15" << "-al" << "0.25";
    } else {
        args << "-vsync" << "0.05" << "-al" << "0.10";
    }
    if (m_fpsDataCheckbox && m_fpsDataCheckbox->isChecked()) args << "-FPSdata";
}

void MainWindow::startServer() {
    if (m_worker && m_worker->isRunning()) return;

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    
    QStringList args = getArgumentsFromFile();
    
    int bleIdx = args.indexOf("-ble");
    if (bleIdx != -1) {
        args.removeAt(bleIdx);
        if (bleIdx < args.size() && !args[bleIdx].startsWith("-")) {
            args.removeAt(bleIdx);
        }
    }

    applyRendererAndFullscreenArgs(args);
    applyUserSettings(args);

    // Only add -ble and start beacon if checkbox is checked
    if (!m_bleCheckbox) return;
    if (m_bleCheckbox->isChecked()) {
        QString bleFilePath = QDir::toNativeSeparators(appData + "/uxplay_status.ble");
        args << "-ble" << bleFilePath;
        startBluetoothBeacon(bleFilePath);
    } else {
        stopBluetoothBeacon();
    }

    m_worker = new AirPlayWorker(this);
    qInfo().noquote() << "[runtime] swave-imirror arguments:" << args.join(' ');
    m_worker->setArgs(args);

    connect(m_worker, &AirPlayWorker::started, this, &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this, &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::finished, m_worker, &QObject::deleteLater);

    m_worker->start();
}

void MainWindow::stopServer() {
    stopBluetoothBeacon();
    if (m_worker) {
        m_worker->disconnect();
        if (m_worker->isRunning()) {
            QMetaObject::invokeMethod(m_worker, &AirPlayWorker::stopAirplay, Qt::QueuedConnection);
            if (!m_worker->wait(1000)) {
                m_worker->terminate();
            }
        }
        m_worker = nullptr;
    }
    m_running = false;
    updateStatus();
}

void MainWindow::onAirplayStarted() {
    m_running = true;
    updateStatus();

    printf("onAirplayStarted()!\n");

    // Only explicitly selected Direct3D sinks are known to support Alt+Enter.
    // Leave automatically selected GStreamer sink windows unchanged.
    if (!m_rendererCombo ||
        m_rendererCombo->currentData().toString() == "auto") {
        return;
    }

    // Rename the video window when it pops up.
    QTimer *monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this, monitorTimer]() {
        if (!m_running) {
            monitorTimer->stop();
            monitorTimer->deleteLater();
            return;
        }

        RenameData info;
        info.pid = GetCurrentProcessId();
        info.newTitle = "AirPlay Video Stream (ALT+ENTER for Fullscreen)"; 

        EnumWindows(EnumWindowsProcRename, reinterpret_cast<LPARAM>(&info));
    });
    monitorTimer->start(5000);

}

void MainWindow::onAirplayStopped() {
    m_running = false;
    updateStatus();
    
    if (!m_quitting) {
        qDebug() << "Session ended, restarting server to stay ready...";
        QTimer::singleShot(1000, this, &MainWindow::startServer);
    }
}

void MainWindow::onAirplayError(const QString &message) {
    m_tray->showMessage("swave-imirror", message, QSystemTrayIcon::Warning, 3000);
}

void MainWindow::toggleAutostart(bool checked) {
    setAutostart(checked);
    updateStatus();
}

bool MainWindow::isAutostartEnabled() const {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return reg.contains("swave-imirror");
}

void MainWindow::setAutostart(bool enabled) {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled) {
        QString path = QDir::toNativeSeparators(QApplication::applicationFilePath());
        reg.setValue("swave-imirror", "\"" + path + "\"");
    } else {
        reg.remove("swave-imirror");
    }
}

void MainWindow::startBluetoothBeacon(const QString &path) {
    if (m_beacon && m_beacon->state() != QProcess::NotRunning)
        return;

    QString exe = QApplication::applicationDirPath() + "/uxplay-bluetooth-beacon.exe";
    if (!QFile::exists(exe)) {
        qDebug() << "uxplay-bluetooth-beacon.exe not found";
        return;
    }

    m_beacon = new QProcess(this);
    m_beacon->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_beacon, &QProcess::readyRead, this, [this]() {
        // Forward beacon logs to our debug console
        qDebug() << "[beacon output]" << m_beacon->readAll().trimmed();
    });

    // Pass the explicit path to the beacon file
    m_beacon->start(exe, {"--path", path});
    qDebug() << "Beacon process started watching:" << path;
}

void MainWindow::stopBluetoothBeacon() {
    if (!m_beacon) return;
    
    qDebug() << "Stopping beacon process";
    if (m_beacon->state() != QProcess::NotRunning) {
        m_beacon->terminate();
        if (!m_beacon->waitForFinished(500)) {
            qDebug() << "Beacon didn't terminate, killing";
            m_beacon->kill();
            m_beacon->waitForFinished(100);
        }
    }
    
    delete m_beacon;
    m_beacon = nullptr;
    qDebug() << "Beacon stopped and cleaned up";
}

void MainWindow::showLicense() {
    QString path = QApplication::applicationDirPath() + "/LICENSE.rtf";
    if (QFile::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else qDebug() << "License file not found:" << path;
}

void MainWindow::quit() {
    m_quitting = true;
    stopServer();
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_quitting) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::updateStatus() {
    QString status = m_running ? "Receiver Status: Active & Ready" : "Receiver Status: Stopped";
    m_statusLabel->setText(status);
    if (m_running) {
        m_statusLabel->setStyleSheet("background-color: #064E3B; border: 1px solid #10B981; color: #6EE7B7; font-weight: 700; padding: 10px; border-radius: 8px;");
    } else {
        m_statusLabel->setStyleSheet("background-color: #451A03; border: 1px solid #F59E0B; color: #FCD34D; font-weight: 700; padding: 10px; border-radius: 8px;");
    }
    if (m_autostartCheckbox) m_autostartCheckbox->setChecked(isAutostartEnabled());
}

bool MainWindow::isWindowsServicePresent(const std::wstring& serviceName) const {
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hSvc = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!hSvc) {
        DWORD err = GetLastError();
        CloseServiceHandle(hSCM);
        return err != ERROR_SERVICE_DOES_NOT_EXIST;
    }

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return true;
}

bool MainWindow::ensureBonjourServiceInstalled() {
    const std::wstring serviceName = L"Bonjour Service";

    if (isWindowsServicePresent(serviceName)) {
        return true;
    }

    int choice = QMessageBox::question(
        this,
        "Bonjour Service Required",
        "Bonjour Service is required for discovery (mDNS). It is not "
        "installed.\n\nDo you want to install it now?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );

    if (choice != QMessageBox::Yes) {
        QMessageBox::critical(
            this,
            "Bonjour Service Missing",
            "Bonjour Service is required. The application will now exit."
        );
        return false;
    }

    QMessageBox::information(
        this,
        "Installing Bonjour Service",
        "Starting installation of 'Bonjour Service'. Follow any UAC prompt."
    );

    int rc = mdns::MdnsResponder::install();

    // Re-check after install to be safe.
    if (rc == 0 && isWindowsServicePresent(serviceName)) {
        QMessageBox::information(
            this,
            "Installation Complete",
            "Bonjour Service installed successfully.\nThe application will restart."
        );
        restartApplication();
        return false; // do not start server in this instance
    }

    QMessageBox::critical(
        this,
        "Installation Failed",
        "Failed to install 'Bonjour Service'.\n\n"
        "The application will now exit."
    );
    return false;
}

void MainWindow::restartApplication() {
    m_quitting = true;
    stopServer();

    QString exePath = QApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) args.removeFirst(); // remove exe path

    QProcess::startDetached(exePath, args);

    // close GUI of current process after a short delay
    QTimer::singleShot(200, qApp, &QCoreApplication::quit);
}
