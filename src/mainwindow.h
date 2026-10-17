#pragma once

#include <string>

#include <QLabel>
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QCheckBox>
#include <QMessageBox>
#include <QLineEdit>

class QMenu;
class QAction;
class AirPlayWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAutostart(bool checked);
    void showLicense();
    void openSettingsFile();
    void openListArgsFile();
    void quit();
    void onAirplayStarted();
    void onAirplayStopped();
    void onAirplayError(const QString &message);
    void toggleBle(bool checked); // bluetooth
    void toggleForceFullscreen(bool checked);
    void onRendererChanged(int index);
    void persistUserSettings();


private:
    void startServer();
    void stopServer();
    void setupTray();
    void setupUI();
    void updateStatus();
    void startBluetoothBeacon(const QString &path);
    void stopBluetoothBeacon();
    void applyRendererAndFullscreenArgs(QStringList &args);
    void applyUserSettings(QStringList &args) const;

    QProcess *m_beacon = nullptr;

    QStringList getArgumentsFromFile();
    void ensureSettingsFileExists();
    QString userArgumentsPath() const;
    QString machineArgumentsPath() const;
    QString activeArgumentsPath() const;
    QString expandEnvironmentVariables(const QString &content) const;
    bool ensureBonjourServiceInstalled();
    bool isWindowsServicePresent(const std::wstring &serviceName) const;
    void restartApplication();

    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

    QCheckBox *m_bleCheckbox = nullptr;
    QCheckBox *m_fullscreenCheckbox = nullptr;
    QComboBox *m_rendererCombo = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_statusAction = nullptr;

    QCheckBox *m_autostartCheckbox = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QComboBox *m_latencyCombo = nullptr;
    QCheckBox *m_fpsDataCheckbox = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_listargsBtn = nullptr;
    QPushButton *m_licenseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    AirPlayWorker *m_worker = nullptr;

    bool m_running = false;
    bool m_quitting = false;
};
