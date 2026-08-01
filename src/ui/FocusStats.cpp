#include "ui/FocusStats.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

void FocusStats::load() {
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // 坏档不静默清零:改名留现场(.bak),专注历史可手工恢复
        f.close();
        QFile::rename(path(), path() + ".bak");
        return;
    }
    root_ = doc.object();
}

void FocusStats::save() const {
    if (!dirty_) return;
    const QString p = path();
    QDir().mkpath(QFileInfo(p).path());
    QSaveFile f(p);                       // 原子写:临时文件 + commit 才 rename,中途失败不碰原档
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(root_).toJson(QJsonDocument::Indented));
    if (!f.commit()) return;              // 写/落盘失败 → 不覆盖原档,也不清 dirty
    dirty_ = false;
}

void FocusStats::addMs(int tier, qint64 ms) {
    const QString date = QDate::currentDate().toString("yyyy-MM-dd");
    QJsonObject day = root_.value(date).toObject();
    const qint64 prev = static_cast<qint64>(day.value(QString::number(tier)).toDouble());
    day.insert(QString::number(tier), static_cast<double>(prev + ms));
    root_.insert(date, day);
    dirty_ = true;
}

qint64 FocusStats::Day::total() const {
    qint64 s = 0;
    for (auto it = tiers.begin(); it != tiers.end(); ++it) s += it.value();
    return s;
}

QVector<FocusStats::Day> FocusStats::lastDays(int n) const {
    QVector<Day> out;
    out.reserve(static_cast<size_t>(n));
    const QDate today = QDate::currentDate();
    for (int i = n - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        Day day;
        day.date = d.toString("MM-dd") + (d == today ? " 今" : "");
        const QJsonObject dj = root_.value(d.toString("yyyy-MM-dd")).toObject();
        for (auto it = dj.begin(); it != dj.end(); ++it)
            day.tiers.insert(it.key().toInt(), static_cast<qint64>(it.value().toDouble()));
        out.push_back(day);
    }
    return out;
}

QString FocusStats::fmtHours(qint64 ms) {
    return QString::number(ms / 3600000.0, 'f', 1) + "h";
}

QString FocusStats::path() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/focus-stats.json";
}
