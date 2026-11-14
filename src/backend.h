#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

#include "vault.h"

// The app's one job: time spent composing, and the ideas waiting for those
// gaps. The vault holds the record; this class only reads it and appends to it.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList projects READ projects NOTIFY changed)
    Q_PROPERTY(QVariantList ideas READ ideas NOTIFY changed)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList daily READ daily NOTIFY changed)
    Q_PROPERTY(int dailyMax READ dailyMax NOTIFY changed)
    Q_PROPERTY(QStringList modes READ modes CONSTANT)

    Q_PROPERTY(int currentProject READ currentProject NOTIFY changed)
    Q_PROPERTY(int currentMode READ currentMode NOTIFY changed)
    Q_PROPERTY(QString currentProjectName READ currentProjectName NOTIFY changed)

    Q_PROPERTY(bool running READ running NOTIFY clockChanged)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY clockChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QVariantMap pendingInfo READ pendingInfo NOTIFY pendingChanged)
    Q_PROPERTY(bool recovered READ recovered NOTIFY changed)

    Q_PROPERTY(int todayMinutes READ todayMinutes NOTIFY changed)
    Q_PROPERTY(int weekMinutes READ weekMinutes NOTIFY changed)
    Q_PROPERTY(int allMinutes READ allMinutes NOTIFY changed)
    Q_PROPERTY(int streak READ streak NOTIFY changed)

    Q_PROPERTY(QString vaultPath READ vaultPath CONSTANT)
    Q_PROPERTY(bool vaultExists READ vaultExists NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    using Now = std::function<QDateTime()>;

    explicit Backend(const QString &vaultRoot = {}, const QString &dataDirectory = {}, Now now = {},
                     QObject *parent = nullptr);

    QVariantList projects() const { return m_projects; }
    QVariantList ideas() const { return m_ideas; }
    QVariantList sessions() const { return m_sessions; }
    QVariantList daily() const { return m_daily; }
    int dailyMax() const { return m_dailyMax; }
    static QStringList modes();

    int currentProject() const { return m_currentProject; }
    int currentMode() const { return m_currentMode; }
    QString currentProjectName() const;

    bool running() const { return m_running; }
    int elapsed() const;
    bool pending() const { return m_pending; }
    QVariantMap pendingInfo() const;
    bool recovered() const { return m_recovered; }

    int todayMinutes() const { return m_todayMinutes; }
    int weekMinutes() const { return m_weekMinutes; }
    int allMinutes() const { return m_allMinutes; }
    int streak() const { return m_streak; }

    QString vaultPath() const;
    bool vaultExists() const;
    QString error() const { return m_error; }

    Vault vault() const { return Vault(vaultPath()); }
    QStringList projectNames() const;
    bool logSession(const Session &session, QString *error = nullptr);

    Q_INVOKABLE void startPause();
    Q_INVOKABLE void stopAndLog();
    Q_INVOKABLE void savePending(const QString &note);
    Q_INVOKABLE void discardPending();
    Q_INVOKABLE void discardSession();
    Q_INVOKABLE void dismissRecovered();

    Q_INVOKABLE void setProject(int index);
    Q_INVOKABLE void setMode(int index);
    Q_INVOKABLE void nextProject();
    Q_INVOKABLE void addProject(const QString &name);
    Q_INVOKABLE void openProjectNote(int index);

    Q_INVOKABLE void addIdea(const QString &title, const QString &body);
    Q_INVOKABLE void setIdeaStage(int index, const QString &stage);
    Q_INVOKABLE void cycleIdeaStage(int index);
    Q_INVOKABLE void openIdea(int index);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearError();

signals:
    void changed();
    void clockChanged();
    void pendingChanged();

private:
    struct Project {
        QString name;
        QString note;
    };

    void loadProjects();
    void saveProjects();
    void seedProjects();
    void reload();
    void watch();
    void setError(const QString &error);
    void resetTimer();
    void writeCheckpoint();
    void recoverCheckpoint();
    QString checkpointPath() const;

    QString m_vaultRoot;
    QString m_dataDirectory;
    Now m_now;

    QList<Project> m_projectData;
    QVariantList m_projects, m_ideas, m_sessions, m_daily;
    QList<Idea> m_ideaData;
    int m_dailyMax = 1;

    int m_todayMinutes = 0, m_weekMinutes = 0, m_allMinutes = 0, m_streak = 0;
    int m_currentProject = 0, m_currentMode = 0;

    bool m_running = false;
    int m_baseSeconds = 0;
    QDateTime m_startedAt;
    QDateTime m_sessionStart;
    QTimer m_tick, m_debounce;

    bool m_pending = false;
    Session m_pendingSession;

    bool m_recovered = false;
    QString m_error;

    QFileSystemWatcher m_watcher;
};
