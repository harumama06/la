#include "MainWindow.h"
#include "WindowsMonitor.h"
#include "WindowsAudio.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStyle>
#include <QSettings>
#include <QSignalBlocker>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#ifdef Q_OS_WIN
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {
QLabel* makeValueLabel(const QString& text) { auto* l=new QLabel(text); l->setObjectName("valueLabel"); return l; }
QWidget* makeCard(const QString& title, QWidget* content) {
    auto* card=new QWidget; card->setObjectName("card"); auto* layout=new QVBoxLayout(card);
    auto* heading=new QLabel(title); heading->setObjectName("cardTitle"); layout->addWidget(heading); layout->addWidget(content); return card;
}
QString modeText(const DisplayMode& m) { return QString("%1 × %2 @ %3 Hz").arg(m.width).arg(m.height).arg(m.refresh); }
void sendAltKey(unsigned short key) {
#ifdef Q_OS_WIN
    INPUT input[4]{};
    input[0].type=INPUT_KEYBOARD; input[0].ki.wVk=VK_MENU;
    input[1].type=INPUT_KEYBOARD; input[1].ki.wVk=key;
    input[2].type=INPUT_KEYBOARD; input[2].ki.wVk=key; input[2].ki.dwFlags=KEYEVENTF_KEYUP;
    input[3].type=INPUT_KEYBOARD; input[3].ki.wVk=VK_MENU; input[3].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(4,input,sizeof(INPUT));
#else
    Q_UNUSED(key);
#endif
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Game Control Center"); resize(1100,760); setMinimumSize(940,650);
    buildUi(); setupTray();
    statusTimer_=new QTimer(this); connect(statusTimer_,&QTimer::timeout,this,&MainWindow::refreshStatus); statusTimer_->start(1500);
    launchWatchdog_=new QTimer(this); launchWatchdog_->setSingleShot(true);
    connect(launchWatchdog_,&QTimer::timeout,this,[this]{
        if(restorePending_&&!cs2WasRunning_&&!WindowsMonitor::isProcessRunning("cs2.exe")){
            restorePreviousSettings(false);
            if(trayIcon_)trayIcon_->showMessage("Game Control Center","CS2 did not start; previous settings restored.",QSystemTrayIcon::Warning,4000);
        }
    });
    refreshStatus(); refreshProfiles(); populateDisplayModes(); populateAudioDevices(); refreshAudioSessions();
}
void MainWindow::buildUi() {
    auto* central=new QWidget; auto* root=new QVBoxLayout(central); root->setContentsMargins(22,18,22,18); root->setSpacing(14);
    auto* header=new QHBoxLayout; auto* titleBox=new QVBoxLayout;
    auto* title=new QLabel("GAME CONTROL CENTER"); title->setObjectName("appTitle");
    auto* subtitle=new QLabel("Gaming Environment Manager  •  v0.3"); subtitle->setObjectName("muted"); titleBox->addWidget(title); titleBox->addWidget(subtitle); header->addLayout(titleBox); header->addStretch();
    auto* launch=new QPushButton("Launch CS2"); launch->setObjectName("primaryButton"); launch->setMinimumHeight(42); connect(launch,&QPushButton::clicked,this,&MainWindow::launchCS2); header->addWidget(launch); root->addLayout(header);
    tabs_=new QTabWidget; tabs_->addTab(buildOverviewPage(),"Overview"); tabs_->addTab(buildDisplayPage(),"Display"); tabs_->addTab(buildAudioPage(),"Audio"); tabs_->addTab(buildProfilesPage(),"Profiles"); tabs_->addTab(buildStatusPage(),"Status"); root->addWidget(tabs_,1); setCentralWidget(central);
}
QWidget* MainWindow::buildOverviewPage() {
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); layout->setSpacing(14);
    auto* grid=new QHBoxLayout; auto* cs=new QWidget; auto* csl=new QVBoxLayout(cs); csl->addWidget(new QLabel("GAME STATUS")); cs2Status_=makeValueLabel("Checking..."); csl->addWidget(cs2Status_); grid->addWidget(makeCard("CS2",cs));
    auto* sp=new QWidget; auto* spl=new QVBoxLayout(sp); spl->addWidget(new QLabel("NVIDIA SHARE PROCESS")); shadowPlayStatus_=makeValueLabel("Checking..."); spl->addWidget(shadowPlayStatus_); grid->addWidget(makeCard("ShadowPlay",sp)); layout->addLayout(grid);
    auto* row=new QHBoxLayout; auto* mic=new QWidget; auto* ml=new QVBoxLayout(mic); ml->addWidget(new QLabel("Default capture device")); microphoneStatus_=makeValueLabel("Checking..."); ml->addWidget(microphoneStatus_); row->addWidget(makeCard("Microphone",mic));
    auto* app=new QWidget; auto* al=new QVBoxLayout(app); al->addWidget(new QLabel("Audio sessions on default output")); processAudioStatus_=makeValueLabel("Not queried"); al->addWidget(processAudioStatus_); row->addWidget(makeCard("Application Audio",app)); layout->addLayout(row);
    auto* buttons=new QHBoxLayout; auto* overlay=new QPushButton("Open NVIDIA overlay (Alt+Z)"); auto* record=new QPushButton("Toggle recording (Alt+F9)"); buttons->addWidget(overlay); buttons->addWidget(record); buttons->addStretch(); layout->addLayout(buttons);
    connect(overlay,&QPushButton::clicked,this,[this]{triggerNvidiaHotkey(false);}); connect(record,&QPushButton::clicked,this,[this]{triggerNvidiaHotkey(true);});
    auto* note=new QLabel("CS2 profile can be applied automatically when the game starts and your previous display/audio settings restored when it exits. NVIDIA hotkeys require NVIDIA overlay enabled."); note->setObjectName("muted"); note->setWordWrap(true); layout->addWidget(note); layout->addStretch(); return page;
}
QWidget* MainWindow::buildDisplayPage() {
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); auto* content=new QWidget; auto* form=new QFormLayout(content);
    currentDisplayLabel_=new QLabel("Detecting..."); displayModeCombo_=new QComboBox; gameDisplayCombo_=new QComboBox; exitDisplayCombo_=new QComboBox; form->addRow("Current display mode",currentDisplayLabel_); form->addRow("Manual resolution / refresh",displayModeCombo_); form->addRow("In-game Windows resolution",gameDisplayCombo_); form->addRow("After CS2 closes",exitDisplayCombo_);
    auto saveModeChoice=[](QComboBox* combo,const QString& key){QObject::connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),combo,[combo,key](int){QVariantList v=combo->currentData().toList();if(v.size()==3){QSettings s;s.setValue(key,QString("%1x%2@%3").arg(v[0].toInt()).arg(v[1].toInt()).arg(v[2].toInt()));}});};saveModeChoice(gameDisplayCombo_,"gameDisplayMode");saveModeChoice(exitDisplayCombo_,"exitDisplayMode");
    auto* apply=new QPushButton("Apply display mode"); connect(apply,&QPushButton::clicked,this,&MainWindow::applyDisplayMode); form->addRow(apply);
    layout->addWidget(makeCard("Display configuration",content)); auto* warning=new QLabel("Changing display modes can briefly blank the screen. Choose a mode supported by your monitor."); warning->setObjectName("muted"); warning->setWordWrap(true); layout->addWidget(warning); layout->addStretch(); return page;
}
QWidget* MainWindow::buildAudioPage() {
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); auto* content=new QWidget; auto* form=new QFormLayout(content);
    outputCombo_=new QComboBox; inputCombo_=new QComboBox; form->addRow("Default output device",outputCombo_); form->addRow("Default microphone",inputCombo_);
    auto* setDevices=new QPushButton("Set selected default devices"); connect(setDevices,&QPushButton::clicked,this,&MainWindow::applyAudioDevices); form->addRow(setDevices);
    outputVolumeSlider_=new QSlider(Qt::Horizontal); outputVolumeSlider_->setRange(0,100); outputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(false)*100));
    auto* volRow=new QWidget; auto* vl=new QHBoxLayout(volRow); vl->setContentsMargins(0,0,0,0); vl->addWidget(outputVolumeSlider_); auto* applyVol=new QPushButton("Apply volume"); vl->addWidget(applyVol); connect(applyVol,&QPushButton::clicked,this,&MainWindow::applyOutputVolume); form->addRow("Output volume",volRow);
    inputVolumeSlider_=new QSlider(Qt::Horizontal); inputVolumeSlider_->setRange(0,100); inputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(true)*100)); auto* micVolRow=new QWidget; auto* mvl=new QHBoxLayout(micVolRow); mvl->setContentsMargins(0,0,0,0); mvl->addWidget(inputVolumeSlider_); auto* applyMicVol=new QPushButton("Apply mic volume"); mvl->addWidget(applyMicVol); connect(applyMicVol,&QPushButton::clicked,this,[this]{WindowsAudio::setDefaultVolume(inputVolumeSlider_->value()/100.0f,true);}); form->addRow("Microphone volume",micVolRow);
    micMuteCheck_=new QCheckBox("Mute microphone input"); micMuteCheck_->setChecked(WindowsAudio::defaultMuted(true));
    connect(micMuteCheck_,&QCheckBox::toggled,this,[](bool muted){WindowsAudio::setDefaultMute(muted,true);});
    form->addRow("Microphone mute",micMuteCheck_);
    layout->addWidget(makeCard("Audio devices",content));
    auto* openMixer=new QPushButton("Open Windows per-app volume / output routing"); connect(openMixer,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("ms-settings:apps-volume"));}); layout->addWidget(openMixer);
    auto* sessionContent=new QWidget; auto* sl=new QVBoxLayout(sessionContent); auto* refresh=new QPushButton("Refresh application sessions"); connect(refresh,&QPushButton::clicked,this,&MainWindow::refreshAudioSessions); sl->addWidget(refresh);
    sessionTable_=new QTableWidget(0,3); sessionTable_->setHorizontalHeaderLabels({"Application","PID","Volume"}); sessionTable_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch); sessionTable_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents); sessionTable_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch); sessionTable_->setMinimumHeight(260); sl->addWidget(sessionTable_); layout->addWidget(makeCard("Per-application volume",sessionContent));
    auto* customContent=new QWidget; auto* customLayout=new QVBoxLayout(customContent);
    auto* pathRow=new QHBoxLayout; customAudioPath_=new QLineEdit; customAudioPath_->setPlaceholderText("Select an application executable (.exe)");
    auto* browsePath=new QPushButton("Browse..."); auto* addPath=new QPushButton("Add path"); pathRow->addWidget(customAudioPath_,1); pathRow->addWidget(browsePath); pathRow->addWidget(addPath); customLayout->addLayout(pathRow);
    customAudioApps_=new QListWidget; customAudioApps_->setMinimumHeight(100); customLayout->addWidget(customAudioApps_);
    auto* customButtons=new QHBoxLayout; auto* removePath=new QPushButton("Remove selected"); auto* launchPath=new QPushButton("Launch selected app"); customButtons->addWidget(removePath); customButtons->addWidget(launchPath); customButtons->addStretch(); customLayout->addLayout(customButtons);
    auto* customVolRow=new QHBoxLayout; customAudioVolume_=new QSlider(Qt::Horizontal); customAudioVolume_->setRange(0,100); customAudioVolume_->setValue(100); auto* applyCustomVol=new QPushButton("Save app volume rule"); customVolRow->addWidget(customAudioVolume_,1); customVolRow->addWidget(applyCustomVol); customLayout->addLayout(customVolRow);
    layout->addWidget(makeCard("Custom application paths",customContent));
    connect(browsePath,&QPushButton::clicked,this,[this]{QString path=QFileDialog::getOpenFileName(this,"Select application","","Applications (*.exe)");if(!path.isEmpty())customAudioPath_->setText(QDir::toNativeSeparators(path));});
    connect(addPath,&QPushButton::clicked,this,&MainWindow::addCustomAudioApp); connect(removePath,&QPushButton::clicked,this,&MainWindow::removeCustomAudioApp); connect(launchPath,&QPushButton::clicked,this,&MainWindow::launchCustomAudioApp); connect(applyCustomVol,&QPushButton::clicked,this,&MainWindow::applyCustomAudioVolume);
    connect(customAudioApps_,&QListWidget::currentRowChanged,this,[this](int row){if(row<0)return;QSettings settings;QString path=customAudioApps_->item(row)->data(Qt::UserRole).toString();QJsonObject rules=settings.value("customAudioVolumeRules").value<QJsonObject>();customAudioVolume_->setValue(qBound(0,int(rules.value(path).toDouble(100)),100));});
    QSettings customSettings; for(const QString& path:customSettings.value("customAudioPaths").toStringList()){auto* item=new QListWidgetItem(QFileInfo(path).fileName()+" — "+path,customAudioApps_);item->setData(Qt::UserRole,path);}
    auto* hint=new QLabel("Windows per-app session volume is natively limited to 0–100%. Custom paths let GCC register an executable and apply its saved volume rule when its audio session appears. Per-app output routing remains controlled by Windows Volume Mixer."); hint->setObjectName("muted"); hint->setWordWrap(true); layout->addWidget(hint); return page;
}
QWidget* MainWindow::buildProfilesPage() {
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); auto* content=new QWidget; auto* form=new QFormLayout(content);
    profileCombo_=new QComboBox; profileNameEdit_=new QLineEdit("CS2"); form->addRow("Saved profile",profileCombo_); form->addRow("Profile name",profileNameEdit_);
    auto* buttons=new QHBoxLayout; auto* save=new QPushButton("Save current settings"); auto* load=new QPushButton("Apply selected profile"); buttons->addWidget(save); buttons->addWidget(load); form->addRow(buttons);
    autoProfileCheck_=new QCheckBox("Automatically apply CS2 profile on game start and restore previous settings on exit"); QSettings settings; autoProfileCheck_->setChecked(settings.value("autoCs2Profile",true).toBool()); form->addRow(autoProfileCheck_);
    connect(save,&QPushButton::clicked,this,&MainWindow::saveCurrentProfile); connect(load,&QPushButton::clicked,this,&MainWindow::loadSelectedProfile); connect(autoProfileCheck_,&QCheckBox::toggled,this,[](bool v){QSettings s;s.setValue("autoCs2Profile",v);});
    layout->addWidget(makeCard("Game profiles",content)); auto* hint=new QLabel("Profiles store display and audio settings, custom application paths and per-app volume rules. A snapshot of the current settings is held in memory for automatic restoration."); hint->setObjectName("muted"); hint->setWordWrap(true); layout->addWidget(hint); layout->addStretch(); return page;
}
QWidget* MainWindow::buildStatusPage() {
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); auto* content=new QWidget; auto* form=new QFormLayout(content);
    trayHint_=makeValueLabel("System tray ready"); form->addRow("System tray",trayHint_); form->addRow("ShadowPlay",new QLabel("Process detection + NVIDIA overlay hotkeys")); form->addRow("Audio",new QLabel("Windows Core Audio endpoint/session controls")); form->addRow("Game monitor",new QLabel("CS2 process polling every 1.5 seconds"));
    layout->addWidget(makeCard("Background services",content)); layout->addStretch(); return page;
}
void MainWindow::setupTray() {
    trayIcon_=new QSystemTrayIcon(this); trayIcon_->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon)); trayIcon_->setToolTip("Game Control Center");
    auto* menu=new QMenu(this); auto* open=menu->addAction("Open Game Control Center"); auto* quit=menu->addAction("Exit"); connect(open,&QAction::triggered,this,&MainWindow::showFromTray); connect(quit,&QAction::triggered,this,[this]{if(restorePending_)restorePreviousSettings(false);qApp->quit();}); trayIcon_->setContextMenu(menu);
    connect(trayIcon_,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason r){if(r==QSystemTrayIcon::Trigger)showFromTray();}); trayIcon_->show();
}
void MainWindow::refreshStatus() {
    bool running=WindowsMonitor::isProcessRunning("cs2.exe"); bool shadow=WindowsMonitor::isShadowPlayRunning();
    cs2Status_->setText(running?"Running":"Not running"); shadowPlayStatus_->setText(shadow?"NVIDIA Share detected":"Not detected");
    microphoneStatus_->setText(WindowsAudio::defaultEndpointName(true));
    const auto sessions=WindowsAudio::sessions();
    if (processAudioStatus_) processAudioStatus_->setText(QString("%1 active session(s)").arg(sessions.size()));
    applyCustomAudioRules();
    if (currentDisplayLabel_) currentDisplayLabel_->setText(modeText(WindowsDisplay::current()));
    if(running&&launchWatchdog_)launchWatchdog_->stop();
    if (running && !cs2WasRunning_ && autoProfileCheck_ && autoProfileCheck_->isChecked()) applyCS2Profile();
    if (!running && cs2WasRunning_ && restorePending_) restorePreviousSettings();
    cs2WasRunning_=running;
}
void MainWindow::refreshProfiles() {
    if(!profileCombo_)return; QString current=profileCombo_->currentText(); profileCombo_->clear(); profileCombo_->addItems(profiles_.profileNames());
    if(profileCombo_->findText(current)>=0)profileCombo_->setCurrentText(current); else if(profileCombo_->findText("CS2")>=0)profileCombo_->setCurrentText("CS2");
}
void MainWindow::populateDisplayModes() {
    if(!displayModeCombo_)return;const auto current=WindowsDisplay::current();QSettings settings;
    auto fill=[&](QComboBox* combo,const QString& key){if(!combo)return;combo->clear();int selected=-1;const QString saved=settings.value(key).toString();for(const auto& m:WindowsDisplay::modes()){QString text=modeText(m);combo->addItem(text,QVariantList{m.width,m.height,m.refresh});if((!saved.isEmpty()&&saved==QString("%1x%2@%3").arg(m.width).arg(m.height).arg(m.refresh))||(saved.isEmpty()&&m.width==current.width&&m.height==current.height&&m.refresh==current.refresh))selected=combo->count()-1;}if(selected>=0)combo->setCurrentIndex(selected);};
    fill(displayModeCombo_,QString());fill(gameDisplayCombo_,"gameDisplayMode");fill(exitDisplayCombo_,"exitDisplayMode");
}
void MainWindow::populateAudioDevices() {
    if(!outputCombo_||!inputCombo_)return; QString outId=WindowsAudio::defaultEndpointId(false), inId=WindowsAudio::defaultEndpointId(true);
    outputCombo_->clear(); for(const auto& e:WindowsAudio::endpoints(false)){outputCombo_->addItem(e.name,e.id);if(e.id==outId)outputCombo_->setCurrentIndex(outputCombo_->count()-1);}
    inputCombo_->clear(); for(const auto& e:WindowsAudio::endpoints(true)){inputCombo_->addItem(e.name,e.id);if(e.id==inId)inputCombo_->setCurrentIndex(inputCombo_->count()-1);}
}
void MainWindow::applyCustomAudioRules(){
    QSettings settings;
    const QStringList registered=settings.value("customAudioPaths").toStringList();
    const QJsonObject rules=settings.value("customAudioVolumeRules").value<QJsonObject>();
    if(registered.isEmpty()||rules.isEmpty())return;
    const auto sessions=WindowsAudio::sessions();
    for(const auto& session:sessions){
        for(const QString& registeredPath:registered){
            if(session.processPath.compare(registeredPath,Qt::CaseInsensitive)!=0)continue;
            QString ruleKey;
            for(const QString& key:rules.keys())if(key.compare(registeredPath,Qt::CaseInsensitive)==0){ruleKey=key;break;}
            if(!ruleKey.isEmpty())WindowsAudio::setProcessVolume(session.processId,qBound(0,rules.value(ruleKey).toInt(),100)/100.0f);
            break;
        }
    }
}
void MainWindow::refreshAudioSessions() {
    if(!sessionTable_)return; auto sessions=WindowsAudio::sessions(); sessionTable_->setRowCount(0);
    QSettings settings;const QStringList registered=settings.value("customAudioPaths").toStringList();const QJsonObject rules=settings.value("customAudioVolumeRules").value<QJsonObject>();
    for(const auto& s:sessions){
        int targetPercent=qBound(0,int(s.volume*100),100);
        for(const QString& path:registered)if(s.processPath.compare(path,Qt::CaseInsensitive)==0){
            QString ruleKey;for(const QString& key:rules.keys())if(key.compare(path,Qt::CaseInsensitive)==0){ruleKey=key;break;}
            if(!ruleKey.isEmpty()){targetPercent=qBound(0,rules.value(ruleKey).toInt(),100);WindowsAudio::setProcessVolume(s.processId,targetPercent/100.0f);}
            break;
        }
        int row=sessionTable_->rowCount();sessionTable_->insertRow(row);sessionTable_->setItem(row,0,new QTableWidgetItem(s.processName));sessionTable_->setItem(row,1,new QTableWidgetItem(QString::number(s.processId)));
        auto* slider=new QSlider(Qt::Horizontal);slider->setRange(0,100);slider->setValue(targetPercent);slider->setToolTip(QString("%1% (Windows per-app limit)").arg(slider->value()));
        connect(slider,&QSlider::valueChanged,this,[pid=s.processId,path=s.processPath,slider](int v){
            slider->setToolTip(QString("%1% (Windows per-app limit)").arg(v));
            WindowsAudio::setProcessVolume(pid,v/100.0f);
            QSettings settings;const QStringList registeredPaths=settings.value("customAudioPaths").toStringList();
            for(const QString& registeredPath:registeredPaths)if(registeredPath.compare(path,Qt::CaseInsensitive)==0){
                QJsonObject saved=settings.value("customAudioVolumeRules").value<QJsonObject>();saved[registeredPath]=v;settings.setValue("customAudioVolumeRules",saved);break;
            }
        });sessionTable_->setCellWidget(row,2,slider);
    }
}
void MainWindow::addCustomAudioApp(){
    QString path=QFileInfo(customAudioPath_->text().trimmed()).absoluteFilePath();
    if(path.isEmpty()||!QFileInfo::exists(path)||QFileInfo(path).suffix().compare("exe",Qt::CaseInsensitive)!=0){QMessageBox::warning(this,"Custom application","Select a valid .exe file first.");return;}
    QSettings settings;QStringList paths=settings.value("customAudioPaths").toStringList();if(!paths.contains(path,Qt::CaseInsensitive))paths.append(path);settings.setValue("customAudioPaths",paths);
    bool found=false;for(int i=0;i<customAudioApps_->count();++i)if(customAudioApps_->item(i)->data(Qt::UserRole).toString().compare(path,Qt::CaseInsensitive)==0)found=true;
    if(!found){auto* item=new QListWidgetItem(QFileInfo(path).fileName()+" — "+path,customAudioApps_);item->setData(Qt::UserRole,path);customAudioApps_->setCurrentItem(item);}
}
void MainWindow::removeCustomAudioApp(){
    int row=customAudioApps_->currentRow();if(row<0)return;QString path=customAudioApps_->currentItem()->data(Qt::UserRole).toString();QSettings settings;QStringList paths=settings.value("customAudioPaths").toStringList();paths.removeAll(path);settings.setValue("customAudioPaths",paths);QJsonObject rules=settings.value("customAudioVolumeRules").value<QJsonObject>();rules.remove(path);settings.setValue("customAudioVolumeRules",rules);delete customAudioApps_->takeItem(row);
}
void MainWindow::launchCustomAudioApp(){
    if(customAudioApps_->currentRow()<0)return;QString path=customAudioApps_->currentItem()->data(Qt::UserRole).toString();if(!QProcess::startDetached(path,QStringList{}))QMessageBox::warning(this,"Launch application","Windows could not start this executable. Check permissions or required working directory.");
}
void MainWindow::applyCustomAudioVolume(){
    if(customAudioApps_->currentRow()<0){QMessageBox::information(this,"Custom application","Select a registered executable first.");return;}
    QString path=customAudioApps_->currentItem()->data(Qt::UserRole).toString();QSettings settings;QJsonObject rules=settings.value("customAudioVolumeRules").value<QJsonObject>();rules[path]=customAudioVolume_->value();settings.setValue("customAudioVolumeRules",rules);
    const auto sessions=WindowsAudio::sessions();int matched=0;for(const auto& session:sessions)if(session.processPath.compare(path,Qt::CaseInsensitive)==0){WindowsAudio::setProcessVolume(session.processId,customAudioVolume_->value()/100.0f);++matched;}
    QMessageBox::information(this,"Application volume rule",matched?QString("Saved and applied to %1 active session(s). The rule will apply when this app creates an audio session.").arg(matched):"Saved. It will apply when this app is running and creates an audio session.");
}
void MainWindow::applyDisplayMode() {
    QVariantList v=displayModeCombo_->currentData().toList(); if(v.size()!=3)return; DisplayMode m{v[0].toInt(),v[1].toInt(),v[2].toInt()};
    if(!WindowsDisplay::apply(m))QMessageBox::warning(this,"Display mode","Windows could not apply this display mode."); else {currentDisplayLabel_->setText(modeText(WindowsDisplay::current()));QMessageBox::information(this,"Display mode","Display mode applied.");}
}
void MainWindow::applyAudioDevices() {
    bool out=outputCombo_->currentData().toString().isEmpty() || WindowsAudio::setDefaultEndpoint(outputCombo_->currentData().toString(),false); bool in=inputCombo_->currentData().toString().isEmpty() || WindowsAudio::setDefaultEndpoint(inputCombo_->currentData().toString(),true);
    populateAudioDevices(); microphoneStatus_->setText(WindowsAudio::defaultEndpointName(true)); if(micMuteCheck_){QSignalBlocker blocker(micMuteCheck_);micMuteCheck_->setChecked(WindowsAudio::defaultMuted(true));} if(outputVolumeSlider_)outputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(false)*100)); if(inputVolumeSlider_)inputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(true)*100));
    if(!out||!in)QMessageBox::warning(this,"Audio devices","One or more default devices could not be changed. Check Windows permissions/device state.");
}
void MainWindow::applyOutputVolume(){WindowsAudio::setDefaultVolume(outputVolumeSlider_->value()/100.0f,false);}
void MainWindow::launchCS2(){
    if(WindowsMonitor::isProcessRunning("cs2.exe")){QMessageBox::information(this,"CS2","CS2 is already running.");return;}
    if(!restorePending_){previousSettings_=captureSettings();restorePending_=true;}
    const QJsonObject profile=profiles_.loadProfile("CS2");if(!profile.isEmpty())applySettings(profile);
    auto applyChoice=[](QComboBox* combo){if(!combo)return false;QVariantList v=combo->currentData().toList();if(v.size()!=3)return false;return WindowsDisplay::apply(DisplayMode{v[0].toInt(),v[1].toInt(),v[2].toInt()});};
    if(gameDisplayCombo_&&gameDisplayCombo_->currentIndex()>=0&&!applyChoice(gameDisplayCombo_)){QMessageBox::warning(this,"Display mode","Could not switch to the selected in-game Windows display mode. CS2 launch cancelled.");restorePreviousSettings(false);return;}
#ifdef Q_OS_WIN
    HINSTANCE result=ShellExecuteW(nullptr,L"open",L"steam://rungameid/730",nullptr,nullptr,SW_SHOWNORMAL);
    if(reinterpret_cast<INT_PTR>(result)<=32){QMessageBox::warning(this,"Launch CS2","Could not open Steam CS2 URI. Make sure Steam is installed and associated with steam://.");restorePreviousSettings(false);}else if(launchWatchdog_)launchWatchdog_->start(120000);
#else
    if(!QProcess::startDetached("steam",QStringList{"steam://rungameid/730"})){QMessageBox::warning(this,"Launch CS2","Steam could not be launched.");restorePreviousSettings(false);}else if(launchWatchdog_)launchWatchdog_->start(120000);
#endif
}
QJsonObject MainWindow::captureSettings() const {
    QJsonObject data; data["profileName"]=profileNameEdit_?profileNameEdit_->text().trimmed():"CS2"; data["game"]="cs2"; data["version"]="0.3.0";
    auto m=WindowsDisplay::current();data["displayWidth"]=m.width;data["displayHeight"]=m.height;data["displayRefresh"]=m.refresh;
    data["outputEndpointId"]=WindowsAudio::defaultEndpointId(false);data["inputEndpointId"]=WindowsAudio::defaultEndpointId(true);data["outputVolume"]=WindowsAudio::defaultVolume(false);data["inputVolume"]=WindowsAudio::defaultVolume(true);data["inputMuted"]=WindowsAudio::defaultMuted(true);
    QJsonArray appVolumes; for(const auto& session:WindowsAudio::sessions()){QJsonObject item;item["app"]=session.processName;item["path"]=session.processPath;item["volume"]=session.volume;appVolumes.append(item);} data["appVolumes"]=appVolumes; QSettings settings; data["customAudioPaths"]=QJsonArray::fromStringList(settings.value("customAudioPaths").toStringList()); data["customAudioVolumeRules"]=settings.value("customAudioVolumeRules").value<QJsonObject>(); return data;
}
void MainWindow::applySettings(const QJsonObject& data) {
    if(data.contains("displayWidth")){DisplayMode m{data["displayWidth"].toInt(),data["displayHeight"].toInt(),data["displayRefresh"].toInt()};if(m.width>0&&m.height>0)WindowsDisplay::apply(m);}
    if(data.contains("outputEndpointId"))WindowsAudio::setDefaultEndpoint(data["outputEndpointId"].toString(),false);
    if(data.contains("inputEndpointId"))WindowsAudio::setDefaultEndpoint(data["inputEndpointId"].toString(),true);
    if(data.contains("outputVolume"))WindowsAudio::setDefaultVolume(float(data["outputVolume"].toDouble()),false);
    if(data.contains("inputVolume"))WindowsAudio::setDefaultVolume(float(data["inputVolume"].toDouble()),true);if(data.contains("inputMuted"))WindowsAudio::setDefaultMute(data["inputMuted"].toBool(),true);if(micMuteCheck_)micMuteCheck_->setChecked(WindowsAudio::defaultMuted(true));
    if(data.contains("appVolumes")){const auto active=WindowsAudio::sessions();for(const auto value:data["appVolumes"].toArray()){const auto item=value.toObject();for(const auto& session:active)if((!item["path"].toString().isEmpty()&&session.processPath.compare(item["path"].toString(),Qt::CaseInsensitive)==0)||(item["path"].toString().isEmpty()&&session.processName.compare(item["app"].toString(),Qt::CaseInsensitive)==0))WindowsAudio::setProcessVolume(session.processId,float(item["volume"].toDouble()));}}
    if(data.contains("customAudioPaths")){QStringList paths;for(const auto v:data["customAudioPaths"].toArray())paths.append(v.toString());QSettings settings;settings.setValue("customAudioPaths",paths);}
    if(data.contains("customAudioVolumeRules")){QSettings settings;settings.setValue("customAudioVolumeRules",data["customAudioVolumeRules"].toObject());}
    if(customAudioApps_){customAudioApps_->clear();QSettings settings;for(const QString& path:settings.value("customAudioPaths").toStringList()){auto* item=new QListWidgetItem(QFileInfo(path).fileName()+" — "+path,customAudioApps_);item->setData(Qt::UserRole,path);}}
    populateDisplayModes();populateAudioDevices();if(outputVolumeSlider_)outputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(false)*100));if(inputVolumeSlider_)inputVolumeSlider_->setValue(int(WindowsAudio::defaultVolume(true)*100));
}
void MainWindow::saveCurrentProfile(){
    QString name=profileNameEdit_->text().trimmed();if(name.isEmpty()){QMessageBox::warning(this,"Profile","Enter a profile name first.");return;}
    if(profiles_.saveProfile(name,captureSettings())){refreshProfiles();profileCombo_->setCurrentText(name);QMessageBox::information(this,"Profile saved","Saved profile: "+name);}else QMessageBox::warning(this,"Save failed","Could not save profile.");
}
void MainWindow::loadSelectedProfile(){QString name=profileCombo_->currentText();auto data=profiles_.loadProfile(name);if(data.isEmpty()){QMessageBox::information(this,"Profile","This profile does not exist yet.");return;}applySettings(data);profileNameEdit_->setText(name);QMessageBox::information(this,"Profile applied","Applied profile: "+name);}
void MainWindow::applyCS2Profile(){
    if(restorePending_)return;
    previousSettings_=captureSettings();restorePending_=true;
    auto data=profiles_.loadProfile("CS2");
    if(!data.isEmpty())applySettings(data);
    else { // Create a baseline profile on first automatic launch; no settings are changed.
        profiles_.saveProfile("CS2",previousSettings_);refreshProfiles();
    }
    if(gameDisplayCombo_&&gameDisplayCombo_->currentIndex()>=0){
        const QVariantList v=gameDisplayCombo_->currentData().toList();
        if(v.size()==3&&!WindowsDisplay::apply(DisplayMode{v[0].toInt(),v[1].toInt(),v[2].toInt()})&&trayIcon_)
            trayIcon_->showMessage("Game Control Center","Could not apply the selected in-game Windows display mode.",QSystemTrayIcon::Warning,4000);
    }
}
void MainWindow::restorePreviousSettings(bool gameExited){
    if(launchWatchdog_)launchWatchdog_->stop();
    applySettings(previousSettings_);
    if(gameExited&&exitDisplayCombo_){QVariantList v=exitDisplayCombo_->currentData().toList();if(v.size()==3)WindowsDisplay::apply(DisplayMode{v[0].toInt(),v[1].toInt(),v[2].toInt()});}
    restorePending_=false;previousSettings_={};
    if(trayIcon_)trayIcon_->showMessage("Game Control Center","Restored your previous display and audio settings.",QSystemTrayIcon::Information,2500);
}
void MainWindow::triggerNvidiaHotkey(bool recording){sendAltKey(recording?VK_F9:0x5A);}
void MainWindow::showFromTray(){showNormal();raise();activateWindow();}
void MainWindow::closeEvent(QCloseEvent* event){if(trayIcon_&&QSystemTrayIcon::isSystemTrayAvailable()&&trayIcon_->isVisible()){hide();if(trayHint_)trayHint_->setText("Running in system tray");event->ignore();return;}event->accept();}
