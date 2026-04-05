// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "console.h"
#include <QPointer>
#include <QSocketNotifier>
#include <cstdio>
#include <unistd.h>

namespace usdviewer {

class ConsolePrivate : public QObject {
    Q_OBJECT
public:
    ConsolePrivate();
    ~ConsolePrivate();
    void init();
    bool start();
    void stop();
    bool isRunning() const;

    QString text() const;
    QStringList lines() const;

public Q_SLOTS:
    void readAvailable();

public:
    struct Data {
        int readFd = -1;
        int writeFd = -1;
        int oldStdout = -1;
        int oldStderr = -1;
        bool running = false;
        QString buffer;
        QSocketNotifier* notifier = nullptr;
        QPointer<Console> console;
    };
    Data d;
};

ConsolePrivate::ConsolePrivate() {}

ConsolePrivate::~ConsolePrivate() { stop(); }

void
ConsolePrivate::init()
{}

bool
ConsolePrivate::start()
{
    if (d.running)
        return true;

    int pipeFds[2] = { -1, -1 };
    if (::pipe(pipeFds) != 0)
        return false;

    d.readFd = pipeFds[0];
    d.writeFd = pipeFds[1];

    d.oldStdout = ::dup(STDOUT_FILENO);
    d.oldStderr = ::dup(STDERR_FILENO);
    if (d.oldStdout < 0 || d.oldStderr < 0) {
        stop();
        return false;
    }

    if (::dup2(d.writeFd, STDOUT_FILENO) < 0 || ::dup2(d.writeFd, STDERR_FILENO) < 0) {
        stop();
        return false;
    }

    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    d.notifier = new QSocketNotifier(d.readFd, QSocketNotifier::Read, this);
    connect(d.notifier, &QSocketNotifier::activated, this, &ConsolePrivate::readAvailable);

    d.running = true;
    return true;
}

void
ConsolePrivate::stop()
{
    if (d.notifier) {
        delete d.notifier;
        d.notifier = nullptr;
    }

    if (d.oldStdout >= 0) {
        ::dup2(d.oldStdout, STDOUT_FILENO);
        ::close(d.oldStdout);
        d.oldStdout = -1;
    }

    if (d.oldStderr >= 0) {
        ::dup2(d.oldStderr, STDERR_FILENO);
        ::close(d.oldStderr);
        d.oldStderr = -1;
    }

    if (d.readFd >= 0) {
        ::close(d.readFd);
        d.readFd = -1;
    }

    if (d.writeFd >= 0) {
        ::close(d.writeFd);
        d.writeFd = -1;
    }

    d.running = false;
}

bool
ConsolePrivate::isRunning() const
{
    return d.running;
}

QString
ConsolePrivate::text() const
{
    return d.buffer;
}

QStringList
ConsolePrivate::lines() const
{
    return d.buffer.split('\n', Qt::KeepEmptyParts);
}

void
ConsolePrivate::readAvailable()
{
    char chunk[4096];
    const ssize_t count = ::read(d.readFd, chunk, sizeof(chunk));
    if (count <= 0)
        return;

    const QString text = QString::fromLocal8Bit(chunk, int(count));
    d.buffer += text;

    if (d.console)
        Q_EMIT d.console->textAppended(text);
}

Console::Console(QObject* parent)
    : QObject(parent)
    , p(new ConsolePrivate)
{
    p->init();
    p->d.console = this;
}

Console::~Console() {}

bool
Console::start()
{
    return p->start();
}

void
Console::stop()
{
    p->stop();
}

bool
Console::isRunning() const
{
    return p->isRunning();
}

QString
Console::text() const
{
    return p->text();
}

QStringList
Console::lines() const
{
    return p->lines();
}

}  // namespace usdviewer

#include "console.moc"
