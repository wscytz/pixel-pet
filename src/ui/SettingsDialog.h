#pragma once

#include <QDialog>

class PetWidget;
class QLabel;
class QCheckBox;
class QTimer;

// 音源设置页:三个音源(本地文件 / 网页扩展 / Windows 系统音频)各一张卡,
// 附介绍 + 当前状态 + 手动开关。顶部「当前驱动」自动判定哪个源正在喂宠物。
// 协同:默认多开自动(谁响谁驱动);网页/系统可手动开关强制禁用。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(PetWidget* pet, QWidget* parent = nullptr);

signals:
    void requestOpenFile();   // 本地文件卡「打开文件」→ PetWidget::openFile

private slots:
    void refresh();   // 周期刷新「当前驱动」+ 各源状态

private:
    PetWidget* pet_;
    QLabel*   drive_;       // "当前驱动:网页扩展 ●"
    QLabel*   fileStatus_;  // 本地文件 在播/未加载
    QLabel*   webStatus_;   // 网页扩展 已连/未连
    QCheckBox* webEnable_;
    QCheckBox* sysEnable_;
    QTimer*   timer_;
};
