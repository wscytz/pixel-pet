#include "ui/SettingsDialog.h"
#include "ui/PetWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>

SettingsDialog::SettingsDialog(PetWidget* pet, QWidget* parent)
    : QDialog(parent), pet_(pet) {
    setWindowTitle(QStringLiteral("音源设置 — PixelPet"));
    setMinimumWidth(440);

    auto* root = new QVBoxLayout(this);

    // 顶部:当前驱动(自动判定哪个源在喂宠物)
    drive_ = new QLabel(this);
    drive_->setStyleSheet("font-weight:600; padding:6px;");
    root->addWidget(drive_);

    // ── 本地文件 ──
    auto* fileBox = new QGroupBox(QStringLiteral("本地文件"), this);
    auto* fl = new QVBoxLayout(fileBox);
    auto* fileDesc = new QLabel(
        QStringLiteral("把音乐文件拖到宠物窗口,或点「打开文件」。宠物跟这首曲子动。"
                       "适合测某首歌、或没有浏览器时。"), this);
    fileDesc->setWordWrap(true);
    fl->addWidget(fileDesc);
    auto* fr = new QHBoxLayout();
    fileStatus_ = new QLabel(this);
    auto* openBtn = new QPushButton(QStringLiteral("打开文件…"), this);
    connect(openBtn, &QPushButton::clicked, this, &SettingsDialog::requestOpenFile);
    fr->addWidget(fileStatus_);
    fr->addStretch();
    fr->addWidget(openBtn);
    fl->addLayout(fr);
    root->addWidget(fileBox);

    // ── 网页扩展 ──
    auto* webBox = new QGroupBox(QStringLiteral("网页扩展"), this);
    auto* wl = new QVBoxLayout(webBox);
    auto* webDesc = new QLabel(
        QStringLiteral("装浏览器扩展,任意网页播放站点(音乐 / 视频)的"
                       "标签页都能抓。最省心、音质好。装好后点扩展图标或快捷键开始抓。"), this);
    webDesc->setWordWrap(true);
    wl->addWidget(webDesc);
    auto* wr = new QHBoxLayout();
    webStatus_ = new QLabel(this);
    webEnable_ = new QCheckBox(QStringLiteral("启用"), this);
    webEnable_->setChecked(pet_->webEnabled());
    connect(webEnable_, &QCheckBox::toggled, this, [this](bool on) {
        pet_->setWebEnabled(on);
        refresh();
    });
    wr->addWidget(webStatus_);
    wr->addStretch();
    wr->addWidget(webEnable_);
    wl->addLayout(wr);
    root->addWidget(webBox);

    // ── 系统音频(Windows WASAPI)──
    auto* sysBox = new QGroupBox(QStringLiteral("系统音频(Windows)"), this);
    auto* sl = new QVBoxLayout(sysBox);
    auto* sysDesc = new QLabel(
        QStringLiteral("抓整个系统声音输出,桌面音乐客户端 / 任何 app 都能驱动宠物。"
                       "仅 Windows 支持(mac 走网页扩展)。"), this);
    sysDesc->setWordWrap(true);
    sl->addWidget(sysDesc);
    auto* sr = new QHBoxLayout();
    sr->addStretch();
    sysEnable_ = new QCheckBox(QStringLiteral("开启捕获"), this);
#ifdef Q_OS_WIN
    sysEnable_->setChecked(pet_->systemOn());
    connect(sysEnable_, &QCheckBox::toggled, this, [this](bool on) {
        if (on) pet_->startSystemAudio();
        else    pet_->stopSystemAudio();
        refresh();
    });
#else
    sysEnable_->setEnabled(false);
    sysEnable_->setToolTip(QStringLiteral("仅 Windows 支持"));
    sysEnable_->setText(QStringLiteral("开启捕获(仅 Windows)"));
#endif
    sr->addWidget(sysEnable_);
    sl->addLayout(sr);
    root->addWidget(sysBox);

    root->addStretch();

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    timer_ = new QTimer(this);
    timer_->setInterval(500);
    connect(timer_, &QTimer::timeout, this, &SettingsDialog::refresh);
    timer_->start();
    refresh();
}

void SettingsDialog::refresh() {
    drive_->setText(QStringLiteral("🎧 当前驱动:%1").arg(pet_->currentSourceName()));
    const bool fp = pet_->enginePlaying();
    fileStatus_->setText(fp ? QStringLiteral("● 在播") : QStringLiteral("○ 未加载"));
    fileStatus_->setStyleSheet(fp ? "color:#3aaa55;" : "color:#999;");
    const bool wc = pet_->webConnected();
    webStatus_->setText(wc ? QStringLiteral("● 已连") : QStringLiteral("○ 未连"));
    webStatus_->setStyleSheet(wc ? "color:#3aaa55;" : "color:#999;");
}
