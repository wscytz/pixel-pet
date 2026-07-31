#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

// 专注时间统计:手动活动档(专注/工作/游戏/休息/运动/电音)真实时长按「日期+档」持久化。
// 语义:手动模式 = 用户主动声明在做什么 → 这段真实时间计入;退回自动模式即停。
// 数据:QStandardPaths::AppDataLocation/focus-stats.json = {"2026-07-31": {"3": 1234567, ...}}
// 纯 QtCore/Json,零新依赖;周报对话框在 PetWidget::showFocusReport 组文本。
class FocusStats {
public:
    void load();
    void save() const;
    void addMs(int tier, qint64 ms);   // 累计到当天该档(tier 为 Tier 枚举 int)

    struct Day {
        QString date;                   // 展示标签(MM-dd,今天带"今")
        QHash<int, qint64> tiers;       // tier → ms
        qint64 total() const;
    };
    QVector<Day> lastDays(int n) const; // 今天往前 n 天(含今天,从旧到新)

    static QString fmtHours(qint64 ms); // 1234567 → "1.2h"

private:
    QString path() const;
    QJsonObject root_;
    mutable bool dirty_ = false;
};
