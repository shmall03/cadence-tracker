#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "vault.h"

class CadenceTests : public QObject {
    Q_OBJECT

private slots:
    void logSessionAppendsAndReadsBack()
    {
        QTemporaryDir dir;
        Vault vault(dir.path());

        Session session;
        session.date = QDate(2026, 11, 14);
        session.start = QTime(9, 15);
        session.minutes = 45;
        session.project = "Cartography of Loss";
        session.mode = "Notation";
        session.note = "Re-voiced bars 12-18 | clarinet low";

        QString error;
        QVERIFY2(vault.logSession(session, &error), qPrintable(error));
        QVERIFY(QFileInfo::exists(dir.path() + "/Dashboards/Composition Log.md"));

        const auto sessions = vault.sessions();
        QCOMPARE(sessions.size(), 1);
        QCOMPARE(sessions.first().date, session.date);
        QCOMPARE(sessions.first().start, session.start);
        QCOMPARE(sessions.first().minutes, 45);
        QCOMPARE(sessions.first().project, QString("Cartography of Loss"));
        QCOMPARE(sessions.first().mode, QString("Notation"));
        QVERIFY(sessions.first().note.contains("clarinet low"));

        // A second session appends rather than replacing the first.
        session.date = QDate(2026, 11, 13);
        session.start = QTime(20, 5);
        session.minutes = 20;
        QVERIFY(vault.logSession(session, &error));
        QCOMPARE(vault.sessions().size(), 2);
        QCOMPARE(vault.sessions().at(1).minutes, 20);

        // The table header is written once, and rows follow it in order.
        QFile log(dir.path() + "/Dashboards/Composition Log.md");
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(log.readAll()).split('\n');
        int headers = 0, headerLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).startsWith("| Date | Start | Minutes | Project | Mode | Note |")) {
                ++headers;
                headerLine = i;
            }
        }
        QCOMPARE(headers, 1);
        QVERIFY(lines.at(headerLine + 2).startsWith("| 2026-11-14 |"));
        QVERIFY(lines.at(headerLine + 3).startsWith("| 2026-11-13 |"));
    }

    void ideasReadFrontmatterAndConcept()
    {
        QTemporaryDir dir;
        Vault vault(dir.path());
        write(dir.path() + "/Composition Ideas/Spectral Shadows.md",
              "---\n"
              "created: 2026-02-04\n"
              "tags: [status/heuristic, domain/composition, context/personal]\n"
              "---\n"
              "# Spectral Shadows\n"
              "\n"
              "## Concept\n"
              "\n"
              "Using a personal **audiogram** as a musical score, with [[Hearing Data]].\n");

        const auto ideas = vault.ideas();
        QCOMPARE(ideas.size(), 1);
        QCOMPARE(ideas.first().title, QString("Spectral Shadows"));
        QCOMPARE(ideas.first().key, QString("Spectral Shadows"));
        QCOMPARE(ideas.first().stage, QString("heuristic"));
        QCOMPARE(ideas.first().excerpt,
                 QString("Using a personal audiogram as a musical score, with Hearing Data."));
    }

    void ideaStageEventsOverrideTheNote()
    {
        QTemporaryDir dir;
        Vault vault(dir.path());
        QDir(dir.path()).mkpath("Composition Ideas");
        write(dir.path() + "/Composition Ideas/Threshold Discomfort.md",
              "---\ntags:\n  - status/provisional\n---\n# Threshold Discomfort\n\n## Concept\n\nHyperacusis.\n");
        write(dir.path() + "/Dashboards/Idea Triage.md",
              "- 2026-11-01 · [[Threshold Discomfort]] · active\n"
              "- 2026-11-10 · [[Threshold Discomfort]] · parked\n");

        QCOMPARE(vault.ideas().first().stage, QString("parked"));
    }

    void addIdeaCreatesANoteAndRefusesDuplicates()
    {
        QTemporaryDir dir;
        Vault vault(dir.path());
        QString error;

        QVERIFY2(vault.addIdea("Bone Conduction Choir", "Sing into contact mics."), qPrintable(error));
        const QString path = dir.path() + "/Composition Ideas/Bone Conduction Choir.md";
        QVERIFY(QFileInfo::exists(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString contents = QString::fromUtf8(file.readAll());
        QVERIFY(contents.contains("status/heuristic"));
        QVERIFY(contents.contains("# Bone Conduction Choir"));
        QVERIFY(contents.contains("## Concept"));
        QVERIFY(contents.contains("Sing into contact mics."));

        const auto ideas = vault.ideas();
        QCOMPARE(ideas.size(), 1);
        QCOMPARE(ideas.first().title, QString("Bone Conduction Choir"));
        QCOMPARE(ideas.first().stage, QString("heuristic"));

        QVERIFY(!vault.addIdea("Bone Conduction Choir", "", &error));
        QVERIFY(!error.isEmpty());
    }

    void setIdeaStageAppendsWithoutRewritingTheNote()
    {
        QTemporaryDir dir;
        Vault vault(dir.path());
        QVERIFY(vault.addIdea("Spatial Blur", ""));
        const QString notePath = dir.path() + "/Composition Ideas/Spatial Blur.md";

        QFile before(notePath);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const QByteArray original = before.readAll();
        before.close();

        const auto idea = vault.ideas().first();
        QString error;
        QVERIFY2(vault.setIdeaStage(idea, "active", &error), qPrintable(error));

        QFile after(notePath);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), original);

        QVERIFY(QFileInfo::exists(dir.path() + "/Dashboards/Idea Triage.md"));
        QCOMPARE(vault.ideas().first().stage, QString("active"));
    }

    void missingVaultIsReported()
    {
        QTemporaryDir dir;
        Vault vault(dir.path() + "/nowhere");
        Session session;
        session.date = QDate::currentDate();
        session.start = QTime(9, 0);
        session.minutes = 30;
        session.project = "Anything";
        session.mode = "Composing";
        QString error;
        QVERIFY(!vault.logSession(session, &error));
        QVERIFY(error.contains("not available"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/nowhere"));
    }

    void backendSeedsTheVaultProjects()
    {
        QTemporaryDir vaultDir, dataDir;
        Backend backend(vaultDir.path(), dataDir.path());
        QCOMPARE(backend.projects().size(), 4);
        QCOMPARE(backend.projectNames().first(), QString("Cartography of Loss"));
        QVERIFY(backend.vaultExists());
    }

    void backendLinksNotesThatAppearLater()
    {
        QTemporaryDir vaultDir, dataDir;
        {
            Backend backend(vaultDir.path(), dataDir.path());
            QVERIFY(backend.projects().first().toMap().value("note").toString().isEmpty());
        }

        // The note is written after the project list was first saved.
        QDir(vaultDir.path()).mkpath("Composition Ideas");
        QFile note(vaultDir.path() + "/Composition Ideas/Cartography of Loss.md");
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("# Cartography of Loss\n");
        note.close();

        Backend backend(vaultDir.path(), dataDir.path());
        QCOMPARE(backend.projects().first().toMap().value("note").toString(), note.fileName());

        // And the link is remembered when the next backend starts.
        Backend reopened(vaultDir.path(), dataDir.path());
        QCOMPARE(reopened.projects().first().toMap().value("note").toString(), note.fileName());
    }

    void backendTotalsAndStreak()
    {
        QTemporaryDir vaultDir, dataDir;
        Vault vault(vaultDir.path());
        const QDate today(2026, 11, 14); // Saturday; the week starts Monday the 9th.
        log(vault, today, 30);
        log(vault, today.addDays(-1), 20);
        log(vault, today.addDays(-5), 10);
        log(vault, today.addDays(-6), 60);

        Backend backend(vaultDir.path(), dataDir.path(), [today] {
            return QDateTime(today, QTime(18, 0));
        });

        QCOMPARE(backend.todayMinutes(), 30);
        QCOMPARE(backend.weekMinutes(), 60);
        QCOMPARE(backend.allMinutes(), 120);
        QCOMPARE(backend.streak(), 2);
        QCOMPARE(backend.daily().size(), 14);
        QCOMPARE(backend.daily().last().toMap().value("minutes").toInt(), 30);
        QCOMPARE(backend.dailyMax(), 60);
        QCOMPARE(backend.projects().first().toMap().value("minutes").toInt(), 120);
    }

    void backendTimerLogsTheSession()
    {
        QTemporaryDir vaultDir, dataDir;
        QDateTime now(QDate(2026, 11, 14), QTime(9, 0));
        Backend backend(vaultDir.path(), dataDir.path(), [&now] { return now; });

        backend.startPause();
        QVERIFY(backend.running());
        now = now.addSecs(20 * 60 + 30);
        QCOMPARE(backend.elapsed(), 1230);

        backend.stopAndLog();
        QVERIFY(!backend.running());
        QVERIFY(!backend.running());
        QVERIFY(backend.pending());
        QCOMPARE(backend.pendingInfo().value("minutes").toInt(), 21);
        QCOMPARE(backend.pendingInfo().value("project").toString(), QString("Cartography of Loss"));
        QCOMPARE(backend.pendingInfo().value("mode").toString(), QString("Composing"));

        backend.savePending("Bars 1-24 sketched");
        QVERIFY(!backend.pending());
        QCOMPARE(backend.elapsed(), 0);
        QCOMPARE(backend.allMinutes(), 21);
        QCOMPARE(backend.sessions().size(), 1);
        QCOMPARE(backend.sessions().first().toMap().value("note").toString(), QString("Bars 1-24 sketched"));

        const auto sessions = Vault(vaultDir.path()).sessions();
        QCOMPARE(sessions.size(), 1);
        QCOMPARE(sessions.first().minutes, 21);
        QCOMPARE(sessions.first().start, QTime(9, 0));
    }

    void backendRecoversAnUnfinishedSession()
    {
        QTemporaryDir vaultDir, dataDir;
        QDateTime now(QDate(2026, 11, 14), QTime(9, 0));
        {
            Backend backend(vaultDir.path(), dataDir.path(), [&now] { return now; });
            backend.startPause();
            now = now.addSecs(120);
            backend.startPause(); // pause, which checkpoints the elapsed time
            QCOMPARE(backend.elapsed(), 120);
        }

        Backend restored(vaultDir.path(), dataDir.path(), [&now] { return now; });
        QVERIFY(restored.recovered());
        QCOMPARE(restored.elapsed(), 120);
        QVERIFY(!restored.running());

        restored.discardSession();
        QCOMPARE(restored.elapsed(), 0);
        QVERIFY(!restored.recovered());
    }

    void backendAddsProjectsAndRefusesDuplicates()
    {
        QTemporaryDir vaultDir, dataDir;
        Backend backend(vaultDir.path(), dataDir.path());
        backend.addProject("  Two Ears  ");
        QCOMPARE(backend.projects().size(), 5);
        QCOMPARE(backend.currentProjectName(), QString("Two Ears"));

        backend.addProject("two ears");
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(backend.projects().size(), 5);
    }

    void backendTracksIdeaStages()
    {
        QTemporaryDir vaultDir, dataDir;
        Vault vault(vaultDir.path());
        QVERIFY(vault.addIdea("Spectral Shadows", "Audiogram as score."));
        QVERIFY(vault.addIdea("Spatial Blur", "Cocktail party effect."));

        Backend backend(vaultDir.path(), dataDir.path());
        QCOMPARE(backend.ideas().size(), 2);
        QCOMPARE(backend.ideas().first().toMap().value("stage").toString(), QString("heuristic"));

        backend.cycleIdeaStage(0);
        const auto ideas = backend.ideas();
        bool found = false;
        for (const auto &value : ideas) {
            const auto map = value.toMap();
            if (map.value("title").toString() == "Spatial Blur")
                found = map.value("stage").toString() == "provisional";
        }
        QVERIFY(found);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n sessions": {"one": "%n session", "other": "%n sessions"}})");
        write(dir.path() + "/ru.json", R"({"%n sessions": {"one": "%n сессия", "few": "%n сессии", "many": "%n сессий", "other": "%n сессии"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n sessions", nullptr, 21), QString("%n сессия"));
        QCOMPARE(translator.translate("Main", "%n sessions", nullptr, 22), QString("%n сессии"));
        QCOMPARE(translator.translate("Main", "%n sessions", nullptr, 25), QString("%n сессий"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static void log(Vault &vault, const QDate &date, int minutes)
    {
        Session session;
        session.date = date;
        session.start = QTime(9, 0);
        session.minutes = minutes;
        session.project = "Cartography of Loss";
        session.mode = "Composing";
        QString error;
        QVERIFY2(vault.logSession(session, &error), qPrintable(error));
    }
};

QTEST_MAIN(CadenceTests)
#include "cadence_tests.moc"
