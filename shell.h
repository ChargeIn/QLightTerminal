/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#ifndef SHELL_H
#define SHELL_H

#include <QObject>
#include <QSocketDescriptor>
#include <QSocketNotifier>
#include <QByteArray>

// stores the file descriptors
struct PTY {
    int master, slave;
};

class Shell : public QObject
{
    Q_OBJECT

    /**
     * Creates a master and slave pair and opens them
     */
    void openPty();


    /*
     * creates a new shell process based on the master/slave pair
     */
    void setupPty();

    /*
     * Close master and slave
     */
    void closePty();

public:
    explicit Shell(QObject *parent = nullptr);
     ~Shell();

    PTY pty;

public slots:
    void readStandardOuput();
    void readErrorOutput();

    void write(QByteArray input);

signals:
    void s_error(QString error);
    void s_standardOutput(QString output);
    void s_errorOutput(QString error);

private:
    QSocketNotifier* _readNotifier;
};

#endif // SHELL_H
