#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QTime>

struct Session {
    QDate date;
    QTime start;
    int minutes = 0;
    QString project;
    QString mode;
    QString note;
};

struct Idea {
    QString title;
    QString key;   // the note's file name without .md, used by triage lines
    QString path;
    QString stage;
    QString excerpt;
};

// The vault is the record. Sessions go into a table in
// Dashboards/Composition Log.md and idea stage changes into
// Dashboards/Idea Triage.md, both appended to. Existing notes are only ever
// read, so nothing the user wrote is ever rewritten.
class Vault {
public:
    explicit Vault(const QString &root);

    QString root() const { return m_root; }
    bool exists() const;
    QString sessionsPath() const;
    QString triagePath() const;
    QString ideasPath() const;

    QList<Session> sessions() const;
    bool logSession(const Session &session, QString *error = nullptr);

    QList<Idea> ideas() const;
    bool addIdea(const QString &title, const QString &body, QString *error = nullptr);
    bool setIdeaStage(const Idea &idea, const QString &stage, QString *error = nullptr);

    static QStringList stages();
    static bool isStage(const QString &stage);

private:
    bool ensureDirectory(const QString &path, QString *error);
    bool appendToSessions(const QString &row, QString *error);

    QString m_root;
};
