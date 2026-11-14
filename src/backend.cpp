#include "backend.h"

#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace {

QString modeName(int index)
{
    const auto list = Backend::modes();
    return index >= 0 && index < list.size() ? list.at(index) : list.value(0);
}

// The vault note that belongs to a seeded project. Projects added by hand get
// no mapping; their notes are linked from the idea they came from.
QString seedNoteFile(const QString &project)
{
    if (project == "Cartography of Loss")
        return QStringLiteral("Cartography of Loss.md");
    if (project == "Holes, Scars, and Erosion")
        return QStringLiteral("Tangram Ideas - Holes, Scars, and Erosion.md");
    return {};
}

int modeIndex(const QString &name)
{
    const auto list = Backend::modes();
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i).compare(name, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

int stageRank(const QString &stage)
{
    if (stage == "active") return 0;
    if (stage == "provisional") return 1;
    if (stage == "parked") return 3;
    return 2;
}

bool openInObsidian(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return false;
    QUrl url(QStringLiteral("obsidian://open"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("path"), path);
    url.setQuery(query);
    return QDesktopServices::openUrl(url);
}

} // namespace

Backend::Backend(const QString &vaultRoot, const QString &dataDirectory, Now now, QObject *parent)
    : QObject(parent),
      m_vaultRoot(!vaultRoot.isEmpty() ? vaultRoot
                  : qEnvironmentVariableIsSet("CADENCE_VAULT") ? qEnvironmentVariable("CADENCE_VAULT")
                                                               : QDir::homePath() + "/Obsidian-Vault"),
      m_dataDirectory(!dataDirectory.isEmpty() ? dataDirectory
                                               : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
      m_now(now ? now : [] { return QDateTime::currentDateTime(); }) {
    QDir().mkpath(m_dataDirectory);

    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        emit clockChanged();
        if (m_running && elapsed() % 10 == 0)
            writeCheckpoint();
    });

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Backend::reload);

    loadProjects();
    reload();
    recoverCheckpoint();
}

QStringList Backend::modes()
{
    return {"Composing", "Notation", "Electronics", "Listening", "Commentary"};
}

QString Backend::vaultPath() const { return m_vaultRoot; }

bool Backend::vaultExists() const { return QFileInfo(m_vaultRoot).isDir(); }

QString Backend::currentProjectName() const
{
    if (m_currentProject >= 0 && m_currentProject < m_projectData.size())
        return m_projectData.at(m_currentProject).name;
    return {};
}

QStringList Backend::projectNames() const
{
    QStringList names;
    for (const auto &project : m_projectData)
        names << project.name;
    return names;
}

int Backend::elapsed() const
{
    qint64 seconds = m_baseSeconds;
    if (m_running)
        seconds += m_startedAt.secsTo(m_now());
    return int(qMax<qint64>(0, seconds));
}

QVariantMap Backend::pendingInfo() const
{
    QVariantMap map;
    map["minutes"] = m_pendingSession.minutes;
    map["project"] = m_pendingSession.project;
    map["mode"] = m_pendingSession.mode;
    map["date"] = m_pendingSession.date.toString(Qt::ISODate);
    map["start"] = m_pendingSession.start.toString("HH:mm");
    return map;
}

void Backend::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit changed();
}

void Backend::clearError()
{
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit changed();
}

QString Backend::checkpointPath() const { return m_dataDirectory + "/current.json"; }

void Backend::loadProjects()
{
    m_projectData.clear();
    QFile file(m_dataDirectory + "/projects.json");
    if (file.open(QIODevice::ReadOnly)) {
        const auto array = QJsonDocument::fromJson(file.readAll()).object().value("projects").toArray();
        for (const auto &value : array) {
            const auto object = value.toObject();
            const QString name = object.value("name").toString().trimmed();
            if (!name.isEmpty())
                m_projectData << Project{name, object.value("note").toString()};
        }
    }
    if (m_projectData.isEmpty()) {
        seedProjects();
        saveProjects();
    } else {
        // A note can appear after the project was first saved; link it now so
        // double-clicking the project opens the note rather than an error.
        bool linked = false;
        for (auto &project : m_projectData) {
            if (!project.note.isEmpty())
                continue;
            const QString file = seedNoteFile(project.name);
            const QString path = m_vaultRoot + "/Composition Ideas/" + file;
            if (!file.isEmpty() && QFileInfo::exists(path)) {
                project.note = path;
                linked = true;
            }
        }
        if (linked)
            saveProjects();
    }
    m_currentProject = m_projectData.isEmpty() ? -1 : 0;
}

void Backend::seedProjects()
{
    // The pieces named across the vault's composition notes, so the first run
    // already has somewhere to put time. Notes are linked when they exist.
    const auto note = [this](const QString &project) {
        const QString file = seedNoteFile(project);
        const QString path = m_vaultRoot + "/Composition Ideas/" + file;
        return !file.isEmpty() && QFileInfo::exists(path) ? path : QString();
    };
    m_projectData = {
        {"Cartography of Loss", note("Cartography of Loss")},
        {"Holes, Scars, and Erosion", note("Holes, Scars, and Erosion")},
        {"Belonging", {}},
        {"Filters", {}},
    };
}

void Backend::saveProjects()
{
    QJsonArray array;
    for (const auto &project : m_projectData) {
        QJsonObject object;
        object["name"] = project.name;
        object["note"] = project.note;
        array.append(object);
    }
    QJsonObject root;
    root["projects"] = array;

    QSaveFile file(m_dataDirectory + "/projects.json");
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void Backend::reload()
{
    const QString selected = currentProjectName();
    const QList<Session> all = vault().sessions();

    // A session logged from the command line for a project cadence has not
    // seen yet still deserves a place in the list.
    bool added = false;
    for (const auto &session : all) {
        if (session.project.isEmpty())
            continue;
        const bool known = std::any_of(m_projectData.cbegin(), m_projectData.cend(),
                                       [&](const Project &p) { return p.name == session.project; });
        if (!known) {
            m_projectData << Project{session.project, {}};
            added = true;
        }
    }
    if (added)
        saveProjects();

    QHash<QString, int> perProject;
    QHash<QDate, int> perDay;
    int todayMinutes = 0, weekMinutes = 0, allMinutes = 0;
    const QDate today = m_now().date();
    const QDate weekStart = today.addDays(1 - today.dayOfWeek());

    for (const auto &session : all) {
        perProject[session.project] += session.minutes;
        perDay[session.date] += session.minutes;
        allMinutes += session.minutes;
        if (session.date == today)
            todayMinutes += session.minutes;
        if (session.date >= weekStart && session.date <= today)
            weekMinutes += session.minutes;
    }
    m_todayMinutes = todayMinutes;
    m_weekMinutes = weekMinutes;
    m_allMinutes = allMinutes;

    // A streak through yesterday still counts until today is over.
    QDate day = perDay.contains(today) ? today : today.addDays(-1);
    int streak = 0;
    while (perDay.contains(day)) {
        ++streak;
        day = day.addDays(-1);
    }
    m_streak = streak;

    m_dailyMax = 1;
    m_daily.clear();
    for (int i = 13; i >= 0; --i) {
        const QDate date = today.addDays(-i);
        const int minutes = perDay.value(date);
        m_dailyMax = qMax(m_dailyMax, minutes);
        m_daily << QVariantMap{{"date", date.toString(Qt::ISODate)},
                               {"day", date.day()},
                               {"minutes", minutes},
                               {"today", date == today}};
    }

    m_projects.clear();
    for (const auto &project : m_projectData) {
        m_projects << QVariantMap{{"name", project.name},
                                  {"note", project.note},
                                  {"minutes", perProject.value(project.name)}};
    }

    // Newest first, and enough to see the shape of the week.
    QList<Session> recent = all;
    std::sort(recent.begin(), recent.end(), [](const Session &a, const Session &b) {
        if (a.date != b.date)
            return a.date > b.date;
        return a.start > b.start;
    });
    m_sessions.clear();
    for (int i = 0; i < recent.size() && i < 20; ++i) {
        const auto &session = recent.at(i);
        m_sessions << QVariantMap{{"date", session.date.toString(Qt::ISODate)},
                                  {"start", session.start.toString("HH:mm")},
                                  {"minutes", session.minutes},
                                  {"project", session.project},
                                  {"mode", session.mode},
                                  {"note", session.note}};
    }

    m_ideaData = vault().ideas();
    std::stable_sort(m_ideaData.begin(), m_ideaData.end(), [](const Idea &a, const Idea &b) {
        const int rank = stageRank(a.stage) - stageRank(b.stage);
        return rank != 0 ? rank < 0 : a.title.localeAwareCompare(b.title) < 0;
    });
    m_ideas.clear();
    for (const auto &idea : m_ideaData) {
        m_ideas << QVariantMap{{"title", idea.title},
                               {"key", idea.key},
                               {"path", idea.path},
                               {"stage", idea.stage},
                               {"excerpt", idea.excerpt}};
    }

    // Keep the selection on the same project when a reload rebuilds the list.
    if (!selected.isEmpty()) {
        for (int i = 0; i < m_projectData.size(); ++i)
            if (m_projectData.at(i).name == selected)
                m_currentProject = i;
    }
    if (m_currentProject >= m_projectData.size())
        m_currentProject = m_projectData.isEmpty() ? -1 : m_projectData.size() - 1;
    if (m_currentProject < 0 && !m_projectData.isEmpty())
        m_currentProject = 0;

    watch();
    emit changed();
}

void Backend::watch()
{
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    const QStringList candidates{m_vaultRoot,
                                 m_vaultRoot + "/Dashboards",
                                 m_vaultRoot + "/Composition Ideas",
                                 m_vaultRoot + "/Dashboards/Composition Log.md",
                                 m_vaultRoot + "/Dashboards/Idea Triage.md"};
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
}

void Backend::startPause()
{
    if (m_running) {
        m_baseSeconds = elapsed();
        m_running = false;
        m_tick.stop();
    } else {
        if (m_baseSeconds == 0)
            m_sessionStart = m_now();
        m_startedAt = m_now();
        m_running = true;
        m_tick.start();
    }
    writeCheckpoint();
    emit clockChanged();
}

void Backend::stopAndLog()
{
    if (m_pending || elapsed() <= 0)
        return;

    const int seconds = elapsed();
    if (m_running) {
        m_baseSeconds = seconds;
        m_running = false;
        m_tick.stop();
    }
    if (m_sessionStart.isValid() == false)
        m_sessionStart = m_now().addSecs(-seconds);

    m_pendingSession = Session{m_sessionStart.date(),
                               m_sessionStart.time(),
                               qMax(1, qRound(seconds / 60.0)),
                               currentProjectName(),
                               modeName(m_currentMode),
                               {}};
    m_pending = true;
    m_recovered = false;
    emit clockChanged();
    emit pendingChanged();
    emit changed();
}

void Backend::savePending(const QString &note)
{
    Session session = m_pendingSession;
    session.note = note.trimmed();
    QString failure;
    if (!vault().logSession(session, &failure)) {
        setError(failure);
        return;
    }
    resetTimer();
    reload();
    emit clockChanged();
    emit pendingChanged();
}

void Backend::discardPending()
{
    resetTimer();
    emit clockChanged();
    emit pendingChanged();
}

void Backend::discardSession()
{
    resetTimer();
    emit clockChanged();
    emit pendingChanged();
}

void Backend::resetTimer()
{
    m_pending = false;
    m_pendingSession = {};
    m_baseSeconds = 0;
    m_running = false;
    m_startedAt = {};
    m_sessionStart = {};
    m_recovered = false;
    m_tick.stop();
    QFile::remove(checkpointPath());
}

void Backend::writeCheckpoint()
{
    if (elapsed() < 30) {
        QFile::remove(checkpointPath());
        return;
    }
    QJsonObject object;
    object["project"] = currentProjectName();
    object["mode"] = modeName(m_currentMode);
    object["start"] = m_sessionStart.toString(Qt::ISODate);
    object["base"] = elapsed();
    object["running"] = m_running;

    QSaveFile file(checkpointPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

void Backend::recoverCheckpoint()
{
    QFile file(checkpointPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    const int base = object.value("base").toInt();
    if (base < 30) {
        QFile::remove(checkpointPath());
        return;
    }

    const QString project = object.value("project").toString();
    for (int i = 0; i < m_projectData.size(); ++i)
        if (m_projectData.at(i).name == project)
            m_currentProject = i;
    const int mode = modeIndex(object.value("mode").toString());
    if (mode >= 0)
        m_currentMode = mode;

    m_baseSeconds = base;
    m_sessionStart = QDateTime::fromString(object.value("start").toString(), Qt::ISODate);
    m_running = false;
    m_recovered = true;
    emit changed();
    emit clockChanged();
}

void Backend::dismissRecovered()
{
    m_recovered = false;
    emit changed();
}

void Backend::setProject(int index)
{
    if (index < 0 || index >= m_projectData.size() || index == m_currentProject)
        return;
    m_currentProject = index;
    writeCheckpoint();
    emit changed();
}

void Backend::setMode(int index)
{
    if (index < 0 || index >= modes().size() || index == m_currentMode)
        return;
    m_currentMode = index;
    writeCheckpoint();
    emit changed();
}

void Backend::nextProject()
{
    if (m_projectData.isEmpty())
        return;
    setProject((m_currentProject + 1) % m_projectData.size());
}

void Backend::addProject(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setError(tr("Enter a project name."));
        return;
    }
    for (const auto &project : m_projectData) {
        if (project.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setError(tr("A project called “%1” already exists.").arg(trimmed));
            return;
        }
    }
    m_projectData << Project{trimmed, {}};
    saveProjects();
    m_currentProject = m_projectData.size() - 1;
    m_error.clear();
    reload();
}

void Backend::openProjectNote(int index)
{
    if (index < 0 || index >= m_projectData.size())
        return;
    const QString note = m_projectData.at(index).note;
    if (note.isEmpty()) {
        setError(tr("This project has no note to open."));
        return;
    }
    if (!openInObsidian(note))
        setError(tr("Could not open %1 in Obsidian.").arg(QFileInfo(note).fileName()));
}

void Backend::addIdea(const QString &title, const QString &body)
{
    QString failure;
    if (!vault().addIdea(title, body, &failure)) {
        setError(failure);
        return;
    }
    m_error.clear();
    reload();
}

void Backend::setIdeaStage(int index, const QString &stage)
{
    if (index < 0 || index >= m_ideaData.size())
        return;
    QString failure;
    if (!vault().setIdeaStage(m_ideaData.at(index), stage, &failure)) {
        setError(failure);
        return;
    }
    m_error.clear();
    reload();
}

void Backend::cycleIdeaStage(int index)
{
    if (index < 0 || index >= m_ideaData.size())
        return;
    const auto stages = Vault::stages();
    const int current = stages.indexOf(m_ideaData.at(index).stage);
    setIdeaStage(index, stages.value((current + 1) % stages.size()));
}

void Backend::openIdea(int index)
{
    if (index < 0 || index >= m_ideaData.size())
        return;
    if (!openInObsidian(m_ideaData.at(index).path))
        setError(tr("Could not open %1 in Obsidian.").arg(m_ideaData.at(index).title));
}

bool Backend::logSession(const Session &session, QString *error)
{
    if (!vault().logSession(session, error))
        return false;
    reload();
    emit clockChanged();
    return true;
}

void Backend::refresh() { reload(); }
