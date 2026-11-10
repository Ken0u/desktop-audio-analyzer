#pragma once
#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>

class Workspace : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList layoutNames READ layoutNames NOTIFY changed)
    Q_PROPERTY(QString startupLayout READ startupLayout WRITE setStartupLayout NOTIFY changed)
    Q_PROPERTY(bool restoreLast READ restoreLast WRITE setRestoreLast NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit Workspace(QString folder, QObject* parent = nullptr);
    QStringList layoutNames() const;
    QString startupLayout() const { return data_.value("startupLayout").toString(); }
    bool restoreLast() const { return data_.value("restoreLast").toBool(); }
    QString message() const { return message_; }
    void setStartupLayout(const QString& name);
    void setRestoreLast(bool value);
    Q_INVOKABLE QString createId() const;
    Q_INVOKABLE bool saveLayout(const QString& name, const QVariantList& windows);
    Q_INVOKABLE QVariantList loadLayout(const QString& name);
    Q_INVOKABLE bool deleteLayout(const QString& name);
    Q_INVOKABLE void remember(const QVariantList& windows);
    Q_INVOKABLE QVariantList initialWindows();
    Q_INVOKABLE QVariantMap fitWindow(const QVariantMap& window) const;
    QString filePath() const { return folder_ + "/workspace.json"; }
signals:
    void changed();
private:
    bool write();
    QVariantList validWindows(const QVariantList& windows) const;
    QString folder_, message_;
    QJsonObject data_;
    bool corrupt_ = false;
};
