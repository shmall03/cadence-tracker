#include "cli.h"

#include "backend.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QTime>

namespace {

QTextStream out(stdout);
QTextStream err(stderr);

QString skillText()
{
    return QStringLiteral(R"(---
name: cadence
description: Log composition work and read the idea inbox for the cadence app. Use when a composition session has just finished, when the user asks what they have worked on, or when they want to triage composition ideas. Reads and appends to the Obsidian vault.
---
# cadence

cadence tracks time spent composing and the composition ideas waiting in the
Obsidian vault. It is append-only: sessions go into
`Dashboards/Composition Log.md` and idea stage changes into
`Dashboards/Idea Triage.md`. Never rewrite those notes.

## Commands

- `cadence sessions --days 14 --json` — recent logged sessions.
- `cadence projects --json` — projects and their total minutes.
- `cadence ideas --json` — idea notes with stage (heuristic, provisional, active, parked).
- `cadence log --minutes 25 --project "Cartography of Loss" --mode notation --note "Re-voiced bars 12-18"` — record a finished session.
- `cadence open` — the window.

Modes are `Composing`, `Notation`, `Electronics`, `Listening`, `Commentary`.
Only log a session the user has confirmed; never invent one. Every command
exits 0 on success and 1 on failure, with the error on stderr.
)");
}

QString jsonOf(const QJsonArray &array)
{
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)).trimmed();
}

QJsonObject sessionJson(const QVariantMap &map)
{
    QJsonObject object;
    object["date"] = map.value("date").toString();
    object["start"] = map.value("start").toString();
    object["minutes"] = map.value("minutes").toInt();
    object["project"] = map.value("project").toString();
    object["mode"] = map.value("mode").toString();
    object["note"] = map.value("note").toString();
    return object;
}

int fail(const QString &message)
{
    err << "cadence: " << message << "\n";
    return 1;
}

QCommandLineOption jsonOption()
{
    return QCommandLineOption("json", "Print JSON.");
}

// The parser owns its help text; each command keeps only its own options.
void addCommonOptions(QCommandLineParser &parser)
{
    parser.addHelpOption();
    parser.addOption(jsonOption());
}

} // namespace

int Cli::run(const QStringList &arguments)
{
    if (arguments.isEmpty())
        return fail("missing command");

    const QString command = arguments.first();
    const QStringList rest = arguments.mid(1);

    if (command == "log")
        return log(rest);
    if (command == "sessions")
        return sessions(rest);
    if (command == "ideas")
        return ideas(rest);
    if (command == "projects")
        return projects(rest);
    if (command == "skill")
        return skill(rest);
    if (command == "--version" || command == "-v") {
        out << QCoreApplication::applicationName() << " " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    if (command == "--help" || command == "-h") {
        out << "cadence — track composition sessions and the ideas between them.\n\n"
               "Usage:\n"
               "  cadence open\n"
               "  cadence log --minutes N [--project NAME] [--mode MODE] [--note TEXT]\n"
               "              [--date YYYY-MM-DD] [--time HH:MM] [--json]\n"
               "  cadence sessions [--days N] [--json]\n"
               "  cadence ideas [--json]\n"
               "  cadence projects [--json]\n"
               "  cadence skill install [--json]\n";
        return 0;
    }

    return fail(QString("unknown command “%1”; run cadence --help").arg(command));
}

int Cli::log(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Log a finished composition session.");
    addCommonOptions(parser);
    QCommandLineOption minutesOption({"m", "minutes"}, "Length in minutes.", "minutes");
    QCommandLineOption projectOption("project", "Project name.", "project");
    QCommandLineOption modeOption("mode", "Composing, Notation, Electronics, Listening, or Commentary.", "mode");
    QCommandLineOption noteOption("note", "What you did.", "note");
    QCommandLineOption dateOption("date", "Date, YYYY-MM-DD (default today).", "date");
    QCommandLineOption timeOption("time", "Start time, HH:MM (default now).", "time");
    parser.addOptions({minutesOption, projectOption, modeOption, noteOption, dateOption, timeOption});
    parser.process(QStringList{QCoreApplication::applicationName()} << arguments);

    bool ok = false;
    const int minutes = parser.value(minutesOption).toInt(&ok);
    if (!ok || minutes < 1 || minutes > 1440)
        return fail("--minutes must be a whole number between 1 and 1440");

    const QString mode = parser.value(modeOption).isEmpty() ? "Composing" : parser.value(modeOption);
    QString canonicalMode;
    for (const auto &name : Backend::modes())
        if (name.compare(mode, Qt::CaseInsensitive) == 0)
            canonicalMode = name;
    if (canonicalMode.isEmpty())
        return fail(QString("unknown mode “%1”; use %2").arg(mode, Backend::modes().join(", ")));

    QString project = parser.value(projectOption).trimmed();
    if (project.isEmpty())
        project = m_backend.projectNames().value(0, "Unfiled");

    Session session;
    session.date = parser.value(dateOption).isEmpty()
                       ? QDate::currentDate()
                       : QDate::fromString(parser.value(dateOption), Qt::ISODate);
    session.start = parser.value(timeOption).isEmpty()
                        ? QTime::currentTime()
                        : QTime::fromString(parser.value(timeOption), "HH:mm");
    if (!session.date.isValid())
        return fail("--date must be YYYY-MM-DD");
    if (!session.start.isValid())
        return fail("--time must be HH:MM");
    session.minutes = minutes;
    session.project = project;
    session.mode = canonicalMode;
    session.note = parser.value(noteOption);

    QString failure;
    if (!m_backend.logSession(session, &failure))
        return fail(failure);

    if (parser.isSet("json")) {
        out << jsonOf(QJsonArray{sessionJson(QVariantMap{{"date", session.date.toString(Qt::ISODate)},
                                                        {"start", session.start.toString("HH:mm")},
                                                        {"minutes", session.minutes},
                                                        {"project", session.project},
                                                        {"mode", session.mode},
                                                        {"note", session.note}})}) << "\n";
    } else {
        out << QString("Logged %1 min on %2 (%3).\n").arg(minutes).arg(project, canonicalMode);
    }
    return 0;
}

int Cli::sessions(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("List logged composition sessions.");
    addCommonOptions(parser);
    QCommandLineOption daysOption("days", "How many days back to list (default 14).", "days", "14");
    parser.addOption(daysOption);
    parser.process(QStringList{QCoreApplication::applicationName()} << arguments);

    bool ok = false;
    const int days = parser.value(daysOption).toInt(&ok);
    if (!ok || days < 1)
        return fail("--days must be a positive whole number");

    const QDate since = QDate::currentDate().addDays(-(days - 1));
    QJsonArray array;
    for (const auto &session : m_backend.vault().sessions()) {
        if (session.date < since)
            continue;
        array.append(sessionJson(QVariantMap{{"date", session.date.toString(Qt::ISODate)},
                                             {"start", session.start.toString("HH:mm")},
                                             {"minutes", session.minutes},
                                             {"project", session.project},
                                             {"mode", session.mode},
                                             {"note", session.note}}));
    }

    if (parser.isSet("json")) {
        out << jsonOf(array) << "\n";
        return 0;
    }
    if (array.isEmpty()) {
        out << "No sessions in the last " << days << " days.\n";
        return 0;
    }
    for (const auto &value : array) {
        const auto object = value.toObject();
        out << object.value("date").toString() << " " << object.value("start").toString() << "  "
            << QString("%1 min").arg(object.value("minutes").toInt()) << "  "
            << object.value("project").toString() << "  " << object.value("mode").toString();
        if (!object.value("note").toString().isEmpty())
            out << "  — " << object.value("note").toString();
        out << "\n";
    }
    return 0;
}

int Cli::ideas(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("List composition ideas and their stages.");
    addCommonOptions(parser);
    parser.process(QStringList{QCoreApplication::applicationName()} << arguments);

    QJsonArray array;
    for (const auto &idea : m_backend.vault().ideas()) {
        QJsonObject object;
        object["title"] = idea.title;
        object["stage"] = idea.stage;
        object["path"] = idea.path;
        object["excerpt"] = idea.excerpt;
        array.append(object);
    }

    if (parser.isSet("json")) {
        out << jsonOf(array) << "\n";
        return 0;
    }
    if (array.isEmpty()) {
        out << "No ideas yet.\n";
        return 0;
    }
    for (const auto &value : array) {
        const auto object = value.toObject();
        out << QString("%1  %2  %3\n")
                   .arg(object.value("stage").toString(), -12)
                   .arg(object.value("title").toString());
        const QString excerpt = object.value("excerpt").toString();
        if (!excerpt.isEmpty())
            out << "  " << excerpt << "\n";
    }
    return 0;
}

int Cli::projects(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("List projects and their logged time.");
    addCommonOptions(parser);
    parser.process(QStringList{QCoreApplication::applicationName()} << arguments);

    QJsonArray array;
    for (const auto &value : m_backend.projects()) {
        const auto map = value.toMap();
        QJsonObject object;
        object["name"] = map.value("name").toString();
        object["note"] = map.value("note").toString();
        object["minutes"] = map.value("minutes").toInt();
        array.append(object);
    }

    if (parser.isSet("json")) {
        out << jsonOf(array) << "\n";
        return 0;
    }
    if (array.isEmpty()) {
        out << "No projects yet.\n";
        return 0;
    }
    for (const auto &value : array) {
        const auto object = value.toObject();
        out << QString("%1  %2 min\n")
                   .arg(object.value("name").toString())
                   .arg(object.value("minutes").toInt());
    }
    return 0;
}

int Cli::skill(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Install the cadence agent skill.");
    addCommonOptions(parser);
    parser.addPositionalArgument("action", "install", "[install]");
    parser.process(QStringList{QCoreApplication::applicationName()} << arguments);

    const auto positional = parser.positionalArguments();
    if (positional.isEmpty() || positional.first() != "install")
        return fail("use: cadence skill install");

    const QString directory = QDir::homePath() + "/.agents/skills/cadence";
    if (!QDir().mkpath(directory))
        return fail(QString("could not create %1").arg(directory));
    const QString path = directory + "/SKILL.md";

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(QString("could not write %1: %2").arg(path, file.errorString()));
    file.write(skillText().toUtf8());
    if (!file.commit())
        return fail(QString("could not write %1: %2").arg(path, file.errorString()));

    if (parser.isSet("json"))
        out << QString("{\"ok\":true,\"path\":\"%1\"}\n").arg(path);
    else
        out << "Installed " << path << "\n";
    return 0;
}
