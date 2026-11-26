#pragma once
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QJsonObject>
#include "ProfileStore.h"
#include "WindowsDisplay.h"

class QLabel; class QComboBox; class QTabWidget; class QCloseEvent;
class QCheckBox; class QLineEdit; class QSlider; class QTableWidget; class QPushButton; class QListWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void refreshStatus();
    void saveCurrentProfile();
    void loadSelectedProfile();
    void showFromTray();
    void refreshAudioSessions();
    void applyDisplayMode();
    void applyAudioDevices();
    void applyOutputVolume();
    void addCustomAudioApp();
    void removeCustomAudioApp();
    void launchCustomAudioApp();
    void applyCustomAudioVolume();
    void launchCS2();
    void applyCS2Profile();
    void restorePreviousSettings(bool gameExited=true);
    void applyCustomAudioRules();
private:
    void buildUi(); QWidget* buildOverviewPage(); QWidget* buildDisplayPage();
    QWidget* buildAudioPage(); QWidget* buildProfilesPage(); QWidget* buildStatusPage();
    void setupTray(); void refreshProfiles(); void populateDisplayModes(); void populateAudioDevices();
    QJsonObject captureSettings() const; void applySettings(const QJsonObject& data);
    void triggerNvidiaHotkey(bool recording);
    QTabWidget* tabs_{}; QLabel* cs2Status_{}; QLabel* shadowPlayStatus_{};
    QLabel* microphoneStatus_{}; QLabel* processAudioStatus_{}; QLabel* trayHint_{};
    QLabel* currentDisplayLabel_{}; QComboBox* displayModeCombo_{}; QComboBox* gameDisplayCombo_{}; QComboBox* exitDisplayCombo_{};
    QComboBox* outputCombo_{}; QComboBox* inputCombo_{}; QSlider* outputVolumeSlider_{}; QSlider* inputVolumeSlider_{};
    QTableWidget* sessionTable_{}; QListWidget* customAudioApps_{}; QLineEdit* customAudioPath_{}; QSlider* customAudioVolume_{}; QCheckBox* micMuteCheck_{}; QComboBox* profileCombo_{}; QLineEdit* profileNameEdit_{};
    QCheckBox* autoProfileCheck_{}; QSystemTrayIcon* trayIcon_{}; QTimer* statusTimer_{}; QTimer* launchWatchdog_{};
    ProfileStore profiles_; bool cs2WasRunning_=false; bool restorePending_=false;
    QJsonObject previousSettings_;
};
