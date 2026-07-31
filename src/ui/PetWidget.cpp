#include "ui/PetWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QPen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEnterEvent>
#include <QMenu>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QMimeData>
#include <QUrl>
#include <QAction>
#include <QActionGroup>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QEvent>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QCloseEvent>

#include "ui/PixelFaces.h"
#include "ui/MacPlatform.h"
#include "ui/Palette.h"
#include "ui/PetScene.h"
#include "ui/SettingsDialog.h"
#include "emotion/EmotionMapper.h"

using Palette::GenreTheme;
using PetScene::EqStyle;   // 音浪样式简名(菜单 + 实现)

// 菜单栏模板图标:单色像素笑脸剪影(setIsMask → macOS 自动适配明/暗菜单条)
static QIcon makeTrayIcon() {
    static const char* E[] = {
        "...........",
        "..XX...XX..",
        "..XX...XX..",
        "...........",
        ".X.......X.",
        ".XXXXXXXXX.",
        "...........",
    };
    constexpr int R = 7, C = 11, PX = 2;
    QImage img(C * PX, R * PX, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    for (int y = 0; y < R; ++y)
        for (int x = 0; x < C; ++x)
            if (E[y][x] == 'X')
                for (int dy = 0; dy < PX; ++dy)
                    for (int dx = 0; dx < PX; ++dx)
                        img.setPixelColor(x * PX + dx, y * PX + dy, QColor(255, 255, 255));
    QIcon ic(QPixmap::fromImage(img));
    ic.setIsMask(true);
    return ic;
}

PetWidget::PetWidget(QWidget* parent) : QWidget(parent) {
    // 不用 Qt::Tool:macOS 上 Tool 窗口随应用失活而隐藏(点别的 app 宠物就消失)。
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);   // 圆角+阴影要透明背景
    setAcceptDrops(true);
    setMouseTracking(true);          // hover 检测
    setCursor(Qt::OpenHandCursor);   // 默认可拖(锁定时切箭头)
    setMinimumSize(150, 150);        // 阴影边距由 PetScene 的 SH 处理;尺寸可缩到迷你宠物档
    setMaximumSize(520, 520);
    clock_.start();
    connect(&timer_, &QTimer::timeout, this, &PetWidget::onTick);
    timer_.start(16);

    analyzer_ = new QTimer(this);
    connect(analyzer_, &QTimer::timeout, this, &PetWidget::onAnalyze);

    // 网页音源:监听 ws://127.0.0.1:17632,浏览器插件推频谱进来
    webSource_ = new WebAudioSource(this);
    connect(webSource_, &WebAudioSource::audioFrame, this, &PetWidget::onWebAudio);
    connect(webSource_, &WebAudioSource::statusChanged, this, [this](WebStatus s) {
        webStatus_ = s;
        update();
    });

    // 系统音频(Windows WASAPI loopback):接桌面客户端等非浏览器音源。非 Windows 为 stub(start 返 false)。
    sysSource_ = new SystemAudioSource(this);
    connect(sysSource_, &SystemAudioSource::pcmFrame, this, &PetWidget::onSystemAudio);
    connect(sysSource_, &SystemAudioSource::activeChanged, this, [this](bool on) {
        systemActive_ = on;
        update();
    });

    // 托盘/菜单栏:最小化后从这里恢复(顺带修 macOS「点 Dock 没反应」)
    tray_ = new QSystemTrayIcon(makeTrayIcon(), this);
    QMenu* tm = new QMenu(this);
    tm->addAction(QStringLiteral("显示 / 隐藏"), this, [this]() {
        isVisible() ? minimizeToTray() : restoreFromTray();
    });
    tm->addAction(QStringLiteral("退出"), this, [this]() { tray_->hide(); close(); });
    tray_->setContextMenu(tm);
    tray_->show();
    qApp->installEventFilter(this);   // Dock 点击 → 激活时若隐藏则恢复
    loadSettings();                   // 恢复 尺寸/位置/手动模式
}

void PetWidget::setEmotion(const Emotion& e) { anim_.setEmotion(e); }

void PetWidget::setSpectrum(const float* spec, int n) {
    int m = std::min(n, kGW);
    for (int i = 0; i < m; ++i) spec_[i] = spec[i];
    haveRealAudio_ = true;
}

std::string PetWidget::gridAscii() const {
    std::string s;
    s.reserve(static_cast<size_t>(kGH * (kGW + 1)));
    for (int y = 0; y < kGH; ++y) {
        for (int x = 0; x < kGW; ++x) {
            const int i = y * kGW + x;
            char c = '.';
            if (faceGrid_.v[i]) c = '#';
            else if (specGrid_.v[i]) c = '+';
            s += c;
        }
        s += '\n';
    }
    return s;
}

void PetWidget::loadFile(const QString& path) {
    if (!engine_) {
        engine_ = new AudioEngine(this);
        connect(engine_, &AudioEngine::loaded, this, [this]() {
            engine_->play();
            analyzer_->start(33);
        });   // 不用 UniqueConnection:lambda 非成员函数指针,Qt 会静默失败 → loaded 连不上 → 不 play
        connect(engine_, &AudioEngine::ended, this, [this]() {
            analyzer_->stop();
        });
    }
    engine_->load(path);
}

void PetWidget::openFile() {
    const QString f = QFileDialog::getOpenFileName(
        this, "选择音乐文件", QString(),
        "音频 (*.mp3 *.wav *.m4a *.flac *.ogg *.aac);;所有文件 (*)");
    if (!f.isEmpty()) loadFile(f);
}

void PetWidget::setManualMode(Tier t) {
    manual_ = true;
    manualTier_ = t;
    anim_.setEmotion(EmotionMapper::canonical(t));     // 定动画节奏(呼吸/嘴动快慢)
    anim_.setGenre(EmotionMapper::def(t).genre);       // 定背景色系/节拍
    anim_.setManual(true, t);                          // 固定显示该档
    update();
    saveSettings();
}

void PetWidget::setAutoMode() {
    manual_ = false;
    anim_.setManual(false, Tier::Calm);
    update();
    saveSettings();
}

void PetWidget::setOpacityLevel(qreal o) {
    opacity_ = qBound(0.40, o, 1.0);
    setWindowOpacity(opacity_);
    saveSettings();
}

void PetWidget::setLocked(bool b) {
    locked_ = b;
    if (b) dragging_ = false;                       // 锁定终止进行中的拖拽
    setCursor(b ? Qt::ArrowCursor : Qt::OpenHandCursor);
    saveSettings();
}

void PetWidget::setEqStyle(EqStyle s) {
    eqStyle_ = s;
    update();
    saveSettings();
}

void PetWidget::setWebEnabled(bool b) {
    webEnabled_ = b;
    if (!b) webActive_ = false;   // 关闭即停止采纳网页数据
    update();
}
bool PetWidget::webConnected() const { return webStatus_ == WebStatus::Connected; }
bool PetWidget::systemOn() const { return sysSource_ && sysSource_->isActive(); }
void PetWidget::startSystemAudio() { if (sysSource_) sysSource_->start(); }
void PetWidget::stopSystemAudio()  { if (sysSource_) sysSource_->stop(); }
bool PetWidget::enginePlaying() const { return engine_ && engine_->isPlaying(); }
QString PetWidget::currentSourceName() const {
    if (webActive_ && webEnabled_)        return QStringLiteral("网页扩展");
    if (systemActive_)                    return QStringLiteral("系统音频");
    if (engine_ && engine_->isPlaying())  return QStringLiteral("本地文件");
    return QStringLiteral("无 — 静默中");
}

void PetWidget::minimizeToTray() { hide(); }

void PetWidget::restoreFromTray() {
    show();
    raise();
    activateWindow();
    MacPlatform::makeTopMost(this);   // 重断置顶,防隐藏后层级丢失
}

bool PetWidget::eventFilter(QObject* /*o*/, QEvent* e) {
    // 点 Dock 图标会让应用激活;若窗口已隐藏则恢复(否则 Dock 点击看似无反应)
    if (e->type() == QEvent::ApplicationActivate && !isVisible())
        restoreFromTray();
    return QWidget::eventFilter(nullptr, e);
}

void PetWidget::setSize(int s) {
    s = qBound(150, s, 520);
    if (s == width() && s == height()) return;
    const QPoint center = frameGeometry().center();
    resize(s, s);
    QPoint topLeft = center - QPoint(s / 2, s / 2);   // 中心锚定(别朝右下角长)
    if (QScreen* sc = QGuiApplication::screenAt(center)) {
        const QRect avail = sc->availableGeometry();   // 夹到屏幕内(防多屏飞出)
        topLeft.setX(qBound(avail.left(), topLeft.x(), avail.right()  - s));
        topLeft.setY(qBound(avail.top(),  topLeft.y(), avail.bottom() - s));
    }
    move(topLeft);
    side_ = s;
    saveSettings();
}

void PetWidget::wheelEvent(QWheelEvent* e) {
    pendingZoom_ += e->angleDelta().y();
    if (!zoomQueued_) {                                 // 合并触控板高频小 delta
        zoomQueued_ = true;
        QTimer::singleShot(0, this, &PetWidget::applyZoom);
    }
    e->accept();
}

void PetWidget::applyZoom() {
    zoomQueued_ = false;
    const int steps = pendingZoom_ / 120;               // 120 = 一格
    pendingZoom_ %= 120;
    if (steps != 0) setSize(width() + steps * 20);
}

void PetWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (inResizeGuard_) return;
    if (width() != height()) {                          // 强制方形
        inResizeGuard_ = true;
        resize(qMax(width(), height()), qMax(width(), height()));
        inResizeGuard_ = false;
    }
}

void PetWidget::closeEvent(QCloseEvent* e) {
    saveSettings();
    QWidget::closeEvent(e);
}

void PetWidget::loadSettings() {
    QSettings s;
    side_ = qBound(150, s.value("size", 320).toInt(), 520);
    resize(side_, side_);
    if (const QVariant p = s.value("pos"); p.isValid()) {
        QPoint pt = p.toPoint();
        if (QScreen* sc = QGuiApplication::screenAt(pt)) {
            const QRect avail = sc->availableGeometry();
            pt.setX(qBound(avail.left(), pt.x(), avail.right()  - side_));
            pt.setY(qBound(avail.top(),  pt.y(), avail.bottom() - side_));
            move(pt);
        }
    }
    const int mt = s.value("manualTier", -1).toInt();
    if (mt >= static_cast<int>(Tier::Focus) && mt < kTierCount)
        setManualMode(static_cast<Tier>(mt));
    // 透明度 / 锁定 / 音浪样式(定制项,构造恢复)
    opacity_ = qBound(0.40, s.value("opacity", 1.0).toDouble(), 1.0);
    setWindowOpacity(opacity_);
    locked_ = s.value("locked", false).toBool();
    setCursor(locked_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
    if (const int es = s.value("eqStyle", 0).toInt(); es >= 0 && es <= 2)
        eqStyle_ = static_cast<EqStyle>(es);
}

void PetWidget::saveSettings() {
    QSettings s;
    s.setValue("size", width());
    s.setValue("pos", pos());
    s.setValue("manualTier", manual_ ? static_cast<int>(manualTier_) : -1);
    s.setValue("opacity", opacity_);
    s.setValue("locked", locked_);
    s.setValue("eqStyle", static_cast<int>(eqStyle_));
}

void PetWidget::onAnalyze() {
    if (!engine_) return;
    float win[2048];
    const int n = engine_->readMonoWindow(win, 2048);
    if (n <= 0) return;
    Features f;
    fx_.compute(win, n, engine_->sampleRate(), 30.0f, f);
    applyFeatures(f);
}

void PetWidget::onWebAudio(const QVector<float>& pcm, int sr, const QString& title) {
    if (!webEnabled_) return;     // 设置页关了网页音源 → 忽略扩展数据
    webActive_ = true;
    lastTitle_ = title;
    Features f;
    fx_.compute(pcm.constData(), pcm.size(), sr, 30.0f, f);   // PCM 路径(同本地文件),chroma/调式才准
    applyFeatures(f);
}

void PetWidget::onSystemAudio(const QVector<float>& mono, int sr) {
    Features f;
    fx_.compute(mono.constData(), mono.size(), sr, 30.0f, f);   // PCM 路径(低频/BPM 质量好)
    applyFeatures(f);
}

void PetWidget::applyFeatures(const Features& f) {
    curRms_ = f.rms;
    const qint64 nowMs = clock_.elapsed();
    if (nowMs - lastAudioMs_ > 1500) anim_.resetSwitch();   // 中断>1.5s 后恢复:重新检测,不粘滞
    lastAudioMs_ = nowMs;
    setSpectrum(f.spectrum.data(), 48);   // 音浪/idle/超时 仍要喂,手动模式也跳
    if (!manual_) {                        // 手动模式:别让音频情绪/流派覆盖固定档
        setEmotion(EmotionMapper::map(f));
        setGenre(EmotionMapper::guessGenre(f));
    }

    // 调试打印(500ms 一次):看 SoundClass 判定准不准,据此手调阈值
    const qint64 now = clock_.elapsed();
    if (now - lastDebugMs_ > 500) {
        lastDebugMs_ = now;
        const char* sn = "Music";
        switch (f.sound) {
            case SoundClass::Silence: sn = "Silence"; break;
            case SoundClass::Voice:   sn = "Voice"; break;
            case SoundClass::Noise:   sn = "Noise"; break;
            default: break;
        }
        const Emotion e = EmotionMapper::map(f);
        const int raw = static_cast<int>(e.tier);              // 当前判据(基因)直判档
        const int shw = static_cast<int>(anim_.state().tier);  // 实际显示(平滑+粘滞)
        std::fprintf(stderr,
            "[dsp] %-7s rms=%.3f bpm=%.0f conf=%.2f mode=%+.2f kconf=%.2f mg=%+.2f ms=%.2f val=%+.2f aro=%.2f raw=%d show=%d | %s\n",
            sn, f.rms, f.bpm, f.bpmConfidence, f.mode, f.keyConfidence,
            f.keyMargin, f.minorShare, e.valence, e.arousal, raw, shw,
            lastTitle_.toUtf8().constData());
    }
}

void PetWidget::updateIdle(float rms, int dtMs) {
    // RMS 持续 <0.015 超 2s → idle;有声立即归零退出。dt 用 onTick 真实间隔。
    quietMs_ = (rms < 0.015f) ? quietMs_ + static_cast<float>(dtMs) : 0.0f;
    anim_.setIdle(quietMs_ > 2000.0f);
}

void PetWidget::togglePlay() {
    if (!engine_) return;
    if (engine_->isPlaying()) {
        engine_->pause();
        analyzer_->stop();
    } else {
        engine_->play();
        analyzer_->start(33);
    }
}

void PetWidget::onTick() {
    const qint64 now = clock_.elapsed();
    int dt = lastMs_ ? static_cast<int>(std::min<qint64>(now - lastMs_, 50)) : 16;
    lastMs_ = now;
    if (!haveRealAudio_) stepSim(dt);

    // idle 判定只在有真实音源时跑(模拟模式跳过)。统一在 onTick 用真实 dt,
    // 不依赖音频回调频率;网页断开>1.5s 或本地停止 → 视为静默。
    if (haveRealAudio_ && (engine_ || webActive_ || systemActive_)) {
        float r = curRms_;
        if (webActive_ && (now - lastAudioMs_ > 1500)) r = 0.0f;
        else if (systemActive_ && (now - lastAudioMs_ > 1500)) r = 0.0f;
        else if (engine_ && !engine_->isPlaying()) r = 0.0f;
        updateIdle(r, dt);
    }

    // 控件区:鼠标进入淡入,离开淡出
    const float cTgt = hasHover_ ? 1.0f : 0.0f;
    controlsAlpha_ += (cTgt - controlsAlpha_) * 0.2f;

    // 真实音频低频能量 → 踩拍:手动也随真曲跳;静默衰减→脸趋静;模拟(!haveRealAudio_)走假 bpm
    beatEnv_ *= 0.90f;
    if (haveRealAudio_) {
        const bool live = (engine_ && engine_->isPlaying()) ||
                          (webActive_ && (now - lastAudioMs_ <= 1500)) ||
                          (systemActive_ && (now - lastAudioMs_ <= 1500));
        float low = 0.0f;
        if (live) for (int i = 0; i < 8; ++i) low += spec_[i];
        low = live ? low / 8.0f : 0.0f;
        if (low > beatEnv_) beatEnv_ = low;   // 快攻
        anim_.setBeat(beatEnv_);
    } else {
        anim_.setBeat(-1.0f);                 // 清外部拍 → 假 bpm 路径
    }

    anim_.tick(dt);
    buildSpectrumGrid();
    update();
}

void PetWidget::stepSim(int dtMs) {
    simTMs_ += static_cast<float>(dtMs);
    const float s = simTMs_ * 0.001f;

    const int gi = static_cast<int>(simTMs_ / 8000.0f) % kGenreCount;
    const Genre g = static_cast<Genre>(gi);
    if (!manual_) anim_.setGenre(g);       // 手动模式:固定流派(下面假频谱照跑,音浪仍动)

    struct Rep { Tier t; float v, a; };
    static const Rep rep[6] = {
        {Tier::Hype,      0.5f, 0.95f},
        {Tier::Sad,      -0.7f, 0.20f},
        {Tier::Joyful,    0.8f, 0.80f},
        {Tier::Surprised, 0.0f, 0.90f},
        {Tier::Love,      0.7f, 0.55f},
        {Tier::Angry,    -0.6f, 0.80f},
    };
    const int idx = static_cast<int>(simTMs_ / 5500.0f) % 6;
    Emotion e;
    e.tier = rep[idx].t;
    e.valence = rep[idx].v;
    e.arousal = rep[idx].a;
    if (!manual_) anim_.setEmotion(e);     // 手动模式:固定情绪档

    const GenreTheme& th = Palette::theme(g);
    const float pulse = anim_.state().beatPulse;
    for (int i = 0; i < kGW; ++i) {
        const float f = i / static_cast<float>(kGW);
        const float env = 1.0f - f * 0.5f;
        const float w = (f < 0.3f) ? th.specLow : (f > 0.7f ? th.specHigh : 1.0f);
        float v = 0.5f + 0.5f * std::sin(s * (2.0f + f * 8.0f) + i * 0.5f);
        v *= env * w * th.specIntensity;
        v += 0.12f * std::sin(s * 7.0f + i);
        if (g == Genre::EDM && f < 0.18f) v += pulse * 0.6f;
        v = std::clamp(v, 0.0f, 1.0f);
        spec_[i] = spec_[i] * 0.55f + v * 0.45f;
    }
}

void PetWidget::buildSpectrumGrid() {
    specGrid_.clear();
    for (int x = 0; x < kGW; ++x) {
        const float v = std::clamp(spec_[x], 0.0f, 1.0f);
        const int h = static_cast<int>(v * kGH);
        for (int yy = 0; yy < h; ++yy) {
            const int row = kGH - 1 - yy;
            const uint8_t b = static_cast<uint8_t>(255.0f * (1.0f - static_cast<float>(yy) / (h + 1)));
            specGrid_.add(x, row, b);
        }
    }
}

void PetWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!raised_) {
        raised_ = true;
        MacPlatform::makeTopMost(this);   // macOS:首次 show 提到 NSStatusWindowLevel
    }
}

void PetWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const FrameState& st = anim_.state();
    // 脸网格仍在这里 build(供 gridAscii 调试 + 视图只读);绘制全交给分层 scene。
    faceGrid_.clear();
    PixelFaces::paintFace(st.tier, st.blink, faceGrid_);
    accentGrid_.clear();
    PixelFaces::paintAccent(st.tier, st.blink, accentGrid_);
    PetScene::SceneInput in{ st, faceGrid_, accentGrid_, Palette::tierSignature(st.tier),
                             spec_, kGW, webStatus_, controlsAlpha_, hoverBtn_, clock_.elapsed(),
                             eqStyle_ };
    PetScene::paint(p, rect(), in);
}

void PetWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int b = PetScene::buttonAt(PetScene::layout(rect()), e->pos());
    if (b == 0) { close(); return; }            // × 退出
    if (b == 1) { minimizeToTray(); return; }   // — 最小化到托盘
    if (locked_) return;                        // 锁定位置:控件照应,只挡拖拽
    dragOffset_ = e->globalPosition().toPoint() - frameGeometry().topLeft();
    dragging_ = true;
    setCursor(Qt::ClosedHandCursor);            // 拖拽中
}

void PetWidget::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_) { move(e->globalPosition().toPoint() - dragOffset_); return; }
    const int h = PetScene::buttonAt(PetScene::layout(rect()), e->pos());
    if (h != hoverBtn_) { hoverBtn_ = h; update(); }
}

void PetWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    setCursor(locked_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
}
void PetWidget::mouseDoubleClickEvent(QMouseEvent*) { togglePlay(); }

void PetWidget::enterEvent(QEnterEvent* e) { QWidget::enterEvent(e); hasHover_ = true; }
void PetWidget::leaveEvent(QEvent* e) { QWidget::leaveEvent(e); hasHover_ = false; hoverBtn_ = -1; }

void PetWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}
void PetWidget::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty()) loadFile(urls.first().toLocalFile());
}

void PetWidget::contextMenuEvent(QContextMenuEvent* e) {
    QMenu m(this);
    m.addAction("设置…", this, [this]() {
        SettingsDialog dlg(this, this);
        connect(&dlg, &SettingsDialog::requestOpenFile, this, [this]() { openFile(); });
        dlg.exec();
    });
    m.addAction("打开文件…", this, [this]() { openFile(); });
    m.addAction("暂停 / 继续", this, [this]() { togglePlay(); });
#ifdef Q_OS_WIN
    {   // 系统音频(WASAPI loopback):抓整个系统输出,接桌面客户端/任意 app
        QAction* a = m.addAction("系统音频");
        a->setCheckable(true);
        a->setChecked(sysSource_ && sysSource_->isActive());
        connect(a, &QAction::triggered, this, [this](bool on) {
            if (!sysSource_) return;
            if (on) sysSource_->start();
            else    sysSource_->stop();
        });
    }
#endif
    {   // 模式:自动(音频驱动)/ 活动档(手动固定脸+色,音浪仍随音频)
        QMenu* mode = m.addMenu("模式");
        QActionGroup* grp = new QActionGroup(&m);
        grp->setExclusive(true);
        auto add = [&](const QString& label, bool isAuto, Tier t) {
            QAction* a = mode->addAction(label);
            a->setCheckable(true);
            a->setChecked(isAuto ? !manual_ : (manual_ && manualTier_ == t));
            grp->addAction(a);
            connect(a, &QAction::triggered, this,
                    [this, isAuto, t]() { isAuto ? setAutoMode() : setManualMode(t); });
        };
        add(QStringLiteral("自动 · 跟随音乐"), true, Tier::Calm);
        const Tier acts[] = {Tier::Focus, Tier::Work, Tier::Game, Tier::Rest, Tier::Exercise, Tier::Edm};
        for (Tier t : acts) add(QString::fromUtf8(EmotionMapper::def(t).name), false, t);
    }
    {   // 大小:预设档(滚轮可细调,中心锚定 + 持久化)
        QMenu* sz = m.addMenu("大小");
        QActionGroup* sg = new QActionGroup(&m);
        sg->setExclusive(true);
        const int cur = width();
        auto addSz = [&](const QString& label, int v) {
            QAction* a = sz->addAction(label);
            a->setCheckable(true);
            a->setChecked(cur == v);
            sg->addAction(a);
            connect(a, &QAction::triggered, this, [this, v]() { setSize(v); });
        };
        addSz(QStringLiteral("迷你 · 180"), 180);
        addSz(QStringLiteral("小 · 240"), 240);
        addSz(QStringLiteral("中 · 320"), 320);
        addSz(QStringLiteral("大 · 440"), 440);
    }
    {   // 透明度:整窗不透明度档(挡视线时调淡;下限 40% 保可读)
        QMenu* op = m.addMenu("透明度");
        QActionGroup* og = new QActionGroup(&m);
        og->setExclusive(true);
        const struct { const char* label; qreal v; } OP[] = {
            {"100%", 1.00}, {"85%", 0.85}, {"70%", 0.70}, {"55%", 0.55}, {"40%", 0.40},
        };
        for (const auto& o : OP) {
            QAction* a = op->addAction(QString::fromUtf8(o.label));
            a->setCheckable(true);
            a->setChecked(qAbs(opacity_ - o.v) < 0.01);
            og->addAction(a);
            connect(a, &QAction::triggered, this, [this, v = o.v]() { setOpacityLevel(v); });
        }
    }
    {   // 音浪:底部地板样式(柱状/镜像/波形)
        QMenu* eq = m.addMenu("音浪");
        QActionGroup* eg = new QActionGroup(&m);
        eg->setExclusive(true);
        const struct { const char* label; EqStyle s; } ES[] = {
            {"柱状", EqStyle::Bars}, {"镜像", EqStyle::Mirror}, {"波形", EqStyle::Wave},
        };
        for (const auto& e : ES) {
            QAction* a = eq->addAction(QString::fromUtf8(e.label));
            a->setCheckable(true);
            a->setChecked(eqStyle_ == e.s);
            eg->addAction(a);
            connect(a, &QAction::triggered, this, [this, s = e.s]() { setEqStyle(s); });
        }
    }
    {   // 锁定位置:勾选后禁止拖拽(控件/右键/双击/托盘仍响应)
        QAction* a = m.addAction("锁定位置");
        a->setCheckable(true);
        a->setChecked(locked_);
        connect(a, &QAction::triggered, this, [this](bool on) { setLocked(on); });
    }
    m.addSeparator();
    m.addAction("退出", this, [this]() { close(); });
    m.exec(e->globalPos());
}

void PetWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) close();
}
