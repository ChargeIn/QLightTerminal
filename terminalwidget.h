/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QPlainTextEdit>
#include <QStringList>

#include "shell.h"

/*
 * Represents the type of output to be expected from the shell
 */
enum OutputState {
    UserInput,
    TerminalOutput
};

class TerminalWidget : public QPlainTextEdit
{
public:
    TerminalWidget();
    ~TerminalWidget();

    void writeUserInput(char c);

public slots:
    void processShellOutput(QString text);

    /*
     * Wirtes the user shell output to the terminal display;
     */
    void writeUserInput(QString output);

    /*
     * Writes the terminal output to the terminal display;
     */
    void writeTerminalOutput(QString output);

private:
    Shell* _shell;
    QTextDocument * _doc;

    /*
     * If the shell output is user input echo
     */
    bool inputEcho;

    /*
     * Deletes the last line of the Terminal
     */
    void deleteLastLine();
protected:
    virtual void keyPressEvent(QKeyEvent *e) override;
    OutputState outputState;
    QString lastline;
};

#endif // TERMINALWIDGET_H
