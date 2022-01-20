/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "shell.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <QDebug>

Shell::Shell(QObject *parent)
    : QObject{parent}, pty{-1,-1}
{
    openPty();

    if(pty.master > -1){
        setupPty();
    }

    if(pty.master > 1){
        _readNotifier = new QSocketNotifier(pty.master, QSocketNotifier::Read);
        _readNotifier->setEnabled(true);

        connect(_readNotifier,&QSocketNotifier::activated, this, &Shell::readStandardOuput);
    }
}

Shell::~Shell(){
    closePty();

    _readNotifier->disconnect();
    delete _readNotifier;
}

void Shell::openPty(){

    // open pty master device
    // O_RDWR -> Open write/read
    // O_NOCCTY -> Do not become controlling terminal
    pty.master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if(pty.master == -1){
        emit s_error("Error on posix_openpt.");
        return;
    }

    // adjust file system permissions to be able to open the slave device
    if(::grantpt(pty.master) == -1){
        emit s_error("Could not adjust slave device permissions.");
        return;
    }

    // unlock slave device
    if(::unlockpt(pty.master) == -1){
        emit s_error("Could not unlock slave device.");
        return;
    }

    // retriev slave path
    char* slavePath = ::ptsname(pty.master);
    if(slavePath == NULL){
        ::close(pty.master);
        pty.master = -1;
        emit s_error("Could not retrieve slave path.");
        return;
    }

    // open slave file
    pty.slave = ::open(slavePath, O_RDWR | O_NOCTTY);
    if(pty.slave == -1){
        ::close(pty.master);
        pty.master = -1;
        emit s_error("could not open slave fd");
        return;
    }
}

void Shell::setupPty() {
    pid_t p;
    char* env[] = { "TERM=dumb", NULL };

    // create new process
    p = ::fork();

    if(p == 0){
        // Child

        // close parent since we are one the slave side
        ::close(pty.master);

        // create mew session grp
        ::setsid();

        // ioctl: Manipulated the device parameter.
        // TIOCSCTTY: Make the terminal the controlling terminal
        if (::ioctl(pty.slave, TIOCSCTTY, NULL) == -1){
            closePty();
            emit s_error("Error setting the controlling terminal.");
        }

        // set output channel of slave
        ::dup2(pty.slave, 0);
        ::dup2(pty.slave, 1);
        ::dup2(pty.slave, 2);

        // execute shell on slave device
        ::execle(qgetenv("SHELL"), "-" + qgetenv("SHELL"), NULL ,env);
        return;
    } if( p > 0) {
        // Parent

        // close slave side
        close(pty.slave);
        return;
    }

    closePty();
    emit s_error("Error while creating new slave process.");
}

void Shell::closePty(){
    if(pty.slave > 1){
        ::close(pty.slave);
        pty.slave = -1;
    }

    if(pty.master > -1){
        ::close(pty.master);
        pty.master = -1;
    }
}

void Shell::write(QByteArray input){
    if(pty.master > -1){
        ::write(pty.master, input, input.size());
    } else {
        emit s_error("Pseudo terminal is not open.");
    }
}

void Shell::readStandardOuput(){
    char buffer[4096];
    int count = 4095;

    QString output;

    while(count == 4095) {
        count = ::read(pty.master, buffer, sizeof(buffer));
        if(count >= 0) {
            output.append(QByteArray(buffer, count));
        } else {
            emit s_error("IO Error");
        }
    }

    emit s_standardOutput(output);
}

void Shell::readErrorOutput(){
   // To do...
}
