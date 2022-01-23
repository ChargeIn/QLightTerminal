/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#ifndef QLIGHTTERMINAL_H
#define QLIGHTTERMINAL_H

#include <QPlainTextEdit>
#include <QStringList>

#include "st.h"

class QLightTerminal : public QPlainTextEdit
{
public:
    QLightTerminal();
    ~QLightTerminal();

public slots:
    void updateTerminal(Term* term);

private:
    SimpleTerminal * st;

    /*
     * Deletes the last line of the Terminal
     */
    void deleteLastLine();
protected:
    virtual void keyPressEvent(QKeyEvent *e) override;
};

#endif // QLIGHTTERMINAL_H
