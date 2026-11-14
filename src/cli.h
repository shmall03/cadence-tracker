#pragma once

#include <QStringList>

class Backend;

// The headless half of the app: log a finished session, read what is already
// logged, and install the agent skill. No display is touched.
class Cli {
public:
    explicit Cli(Backend &backend) : m_backend(backend) {}

    int run(const QStringList &arguments);

private:
    int log(const QStringList &arguments);
    int sessions(const QStringList &arguments);
    int ideas(const QStringList &arguments);
    int projects(const QStringList &arguments);
    int skill(const QStringList &arguments);

    Backend &m_backend;
};
