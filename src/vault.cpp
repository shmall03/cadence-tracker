#include "vault.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace {

QString message(const char *text)
{
    return QCoreApplication::translate("Vault", text);
}

QString sanitizeCell(QString text)
{
    // The log is a Markdown table, so a pipe inside a project name or a note
    // would split the row. A broken bar keeps the text readable instead.
    text.replace('|', QChar(0x00A6));
    text.replace('\n', ' ');
    return text.trimmed();
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool writeAll(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = message("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = message("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QString sessionHeader(const QDate &created)
{
    return QStringLiteral(
               "---\n"
               "created: %1\n"
               "tags: [status/axiomatic, domain/composition, context/personal]\n"
               "---\n"
               "# Composition Log\n"
               "\n"
               "Time spent composing, appended by cadence. Each row is one logged session.\n"
               "\n"
               "| Date | Start | Minutes | Project | Mode | Note |\n"
               "| :--- | :---- | ------: | :------ | :--- | :--- |\n")
        .arg(created.toString(Qt::ISODate));
}

QString triageHeader(const QDate &created)
{
    return QStringLiteral(
               "---\n"
               "created: %1\n"
               "tags: [status/axiomatic, domain/composition, context/personal]\n"
               "---\n"
               "# Idea Triage\n"
               "\n"
               "Stage changes appended by cadence, newest last.\n"
               "\n")
        .arg(created.toString(Qt::ISODate));
}

// The first paragraph under "## Concept", or the first paragraph of the note,
// flattened to one readable line.
QString excerptOf(const QString &contents)
{
    const QStringList lines = contents.split('\n');
    int start = -1;
    for (int i = 0; i < lines.size(); ++i) {
        static const QRegularExpression heading("^##\\s+Concept\\s*$");
        if (heading.match(lines.at(i)).hasMatch()) {
            start = i + 1;
            break;
        }
    }
    if (start < 0) {
        bool inFrontmatter = lines.value(0).trimmed() == "---";
        for (int i = 0; i < lines.size(); ++i) {
            const QString line = lines.at(i).trimmed();
            if (inFrontmatter) {
                if (i > 0 && line == "---")
                    inFrontmatter = false;
                continue;
            }
            if (line.startsWith("# ") || line.isEmpty())
                continue;
            start = i;
            break;
        }
    }
    if (start < 0)
        return {};

    QStringList paragraph;
    for (int i = start; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.startsWith('#'))
            break;
        if (line.isEmpty() && !paragraph.isEmpty())
            break;
        if (!line.isEmpty())
            paragraph << line;
    }

    QString text = paragraph.join(' ');
    static const QRegularExpression image(R"(!\[[^\]]*\]\([^)]*\))");
    static const QRegularExpression link(R"(\[([^\]]+)\]\([^)]*\))");
    static const QRegularExpression wikiAlias(R"(\[\[[^\]|]+\|([^\]]+)\]\])");
    static const QRegularExpression wiki(R"(\[\[([^\]]+)\]\])");
    text.remove(image);
    text.replace(link, "\\1");
    text.replace(wikiAlias, "\\1");
    text.replace(wiki, "\\1");
    text.remove(QRegularExpression(R"([*_`>])"));
    text = text.simplified();
    if (text.size() > 220)
        text = text.left(217).trimmed() + QStringLiteral("…");
    return text;
}

QString frontmatter(const QString &contents)
{
    if (!contents.startsWith(QStringLiteral("---")))
        return {};
    const int end = contents.indexOf(QStringLiteral("\n---"), 3);
    return end < 0 ? QString() : contents.mid(3, end - 3);
}

} // namespace

Vault::Vault(const QString &root) : m_root(root) {}

bool Vault::exists() const { return QFileInfo(m_root).isDir(); }

QString Vault::sessionsPath() const { return m_root + "/Dashboards/Composition Log.md"; }
QString Vault::triagePath() const { return m_root + "/Dashboards/Idea Triage.md"; }
QString Vault::ideasPath() const { return m_root + "/Composition Ideas"; }

QStringList Vault::stages() { return {"heuristic", "provisional", "active", "finished", "parked"}; }

bool Vault::isStage(const QString &stage) { return stages().contains(stage); }

bool Vault::ensureDirectory(const QString &path, QString *error)
{
    if (QDir().mkpath(path))
        return true;
    if (error)
        *error = message("Could not create %1.").arg(path);
    return false;
}

QList<Session> Vault::sessions() const
{
    QList<Session> list;
    const QString contents = QString::fromUtf8(readAll(sessionsPath()));
    if (contents.isEmpty())
        return list;

    static const QRegularExpression row(
        R"(^\|\s*(\d{4})-(\d{2})-(\d{2})\s*\|\s*(\d{1,2}):(\d{2})\s*\|\s*(\d+)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|\s*$)",
        QRegularExpression::MultilineOption);
    auto it = row.globalMatch(contents);
    while (it.hasNext()) {
        const auto match = it.next();
        Session session;
        session.date = QDate(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt());
        session.start = QTime(match.captured(4).toInt(), match.captured(5).toInt());
        session.minutes = match.captured(6).toInt();
        session.project = match.captured(7);
        session.mode = match.captured(8);
        session.note = match.captured(9);
        if (session.date.isValid() && session.start.isValid() && session.minutes > 0)
            list << session;
    }
    return list;
}

bool Vault::appendToSessions(const QString &row, QString *error)
{
    if (!exists()) {
        if (error)
            *error = message("The vault is not available at %1.").arg(m_root);
        return false;
    }
    if (!ensureDirectory(m_root + "/Dashboards", error))
        return false;

    const QString path = sessionsPath();
    QString contents = QString::fromUtf8(readAll(path));
    if (contents.isEmpty()) {
        contents = sessionHeader(QDate::currentDate());
    } else if (!contents.endsWith('\n')) {
        contents += '\n';
    }

    // Keep the table at the end of the note and append the new row after the
    // last existing row, so hand-written prose above it is untouched.
    static const QRegularExpression tableRow(R"(^\s*\|.*\|\s*$)", QRegularExpression::MultilineOption);
    if (!tableRow.match(contents).hasMatch()) {
        contents += "\n| Date | Start | Minutes | Project | Mode | Note |\n"
                    "| :--- | :---- | ------: | :------ | :--- | :--- |\n";
    }

    QStringList lines = contents.split('\n');
    int insertAt = -1;
    for (int i = 0; i < lines.size(); ++i)
        if (tableRow.match(lines.at(i)).hasMatch())
            insertAt = i + 1;
    if (insertAt < 0)
        insertAt = lines.size();
    lines.insert(insertAt, row);

    return writeAll(path, lines.join('\n').toUtf8(), error);
}

bool Vault::logSession(const Session &session, QString *error)
{
    if (!session.date.isValid() || !session.start.isValid() || session.minutes <= 0) {
        if (error)
            *error = message("That session is not valid.");
        return false;
    }
    const QString row = QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 |")
                            .arg(session.date.toString(Qt::ISODate),
                                 session.start.toString("HH:mm"),
                                 QString::number(session.minutes),
                                 sanitizeCell(session.project),
                                 sanitizeCell(session.mode),
                                 sanitizeCell(session.note));
    return appendToSessions(row, error);
}

QList<Idea> Vault::ideas() const
{
    QList<Idea> list;
    QDir directory(ideasPath());
    if (!directory.exists())
        return list;

    for (const auto &name : directory.entryList({"*.md"}, QDir::Files, QDir::Name)) {
        const QString path = directory.filePath(name);
        const QString contents = QString::fromUtf8(readAll(path));
        if (contents.isEmpty())
            continue;

        Idea idea;
        idea.key = QFileInfo(path).completeBaseName();
        idea.path = path;
        idea.title = idea.key;
        static const QRegularExpression heading(R"(^#\s+(.+)$)", QRegularExpression::MultilineOption);
        const auto headingMatch = heading.match(contents);
        if (headingMatch.hasMatch())
            idea.title = headingMatch.captured(1).trimmed();

        idea.stage = "heuristic";
        static const QRegularExpression status(R"(status/(heuristic|provisional|active|finished|parked))");
        const auto statusMatch = status.match(frontmatter(contents));
        if (statusMatch.hasMatch())
            idea.stage = statusMatch.captured(1);

        idea.excerpt = excerptOf(contents);
        list << idea;
    }

    // A stage change is recorded in the triage note rather than by rewriting
    // the idea note, so the newest line for a note wins.
    static const QRegularExpression event(
        R"(^-\s*(\d{4}-\d{2}-\d{2})\s*·\s*\[\[([^\]|]+)(?:\|[^\]]*)?\]\]\s*·\s*(heuristic|provisional|active|finished|parked)\s*$)",
        QRegularExpression::MultilineOption);
    QHash<QString, QString> latest;
    auto it = event.globalMatch(QString::fromUtf8(readAll(triagePath())));
    while (it.hasNext()) {
        const auto match = it.next();
        latest.insert(match.captured(2).trimmed(), match.captured(3));
    }
    for (auto &idea : list)
        if (latest.contains(idea.key))
            idea.stage = latest.value(idea.key);

    return list;
}

bool Vault::addIdea(const QString &title, const QString &body, QString *error)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = message("Enter an idea first.");
        return false;
    }
    if (!exists()) {
        if (error)
            *error = message("The vault is not available at %1.").arg(m_root);
        return false;
    }
    if (!ensureDirectory(ideasPath(), error))
        return false;

    QString fileName = trimmed;
    fileName.replace(QRegularExpression(R"([/\\:*?"<>|])"), "-");
    fileName = fileName.trimmed();
    const QString path = ideasPath() + "/" + fileName + ".md";
    if (QFileInfo::exists(path)) {
        if (error)
            *error = message("An idea note named “%1” already exists.").arg(fileName);
        return false;
    }

    const QString text = body.trimmed();
    const QString contents = QStringLiteral(
                                 "---\n"
                                 "created: %1\n"
                                 "tags:\n"
                                 "  - status/heuristic\n"
                                 "  - domain/composition\n"
                                 "  - context/personal\n"
                                 "---\n"
                                 "# %2\n"
                                 "\n")
                                 .arg(QDate::currentDate().toString(Qt::ISODate), trimmed);
    const QString full = text.isEmpty() ? contents + "## Concept\n"
                                        : contents + "## Concept\n\n" + text + "\n";
    return writeAll(path, full.toUtf8(), error);
}

bool Vault::setIdeaStage(const Idea &idea, const QString &stage, QString *error)
{
    if (!isStage(stage)) {
        if (error)
            *error = message("Unknown idea stage: %1.").arg(stage);
        return false;
    }
    if (!exists()) {
        if (error)
            *error = message("The vault is not available at %1.").arg(m_root);
        return false;
    }
    if (!ensureDirectory(m_root + "/Dashboards", error))
        return false;

    const QString path = triagePath();
    QString contents = QString::fromUtf8(readAll(path));
    if (contents.isEmpty())
        contents = triageHeader(QDate::currentDate());
    if (!contents.endsWith('\n'))
        contents += '\n';
    contents += QStringLiteral("- %1 · [[%2]] · %3\n")
                    .arg(QDate::currentDate().toString(Qt::ISODate), idea.key, stage);
    return writeAll(path, contents.toUtf8(), error);
}
