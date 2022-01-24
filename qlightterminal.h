/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#ifndef QLIGHTTERMINAL_H
#define QLIGHTTERMINAL_H

#include <QWidget>
#include <QStringList>

#include "st.h"

class QLightTerminal : public QWidget
{
public:
    QLightTerminal();
    ~QLightTerminal();

public slots:
    void updateTerminal(Term* term);

private:
    SimpleTerminal * st;
    QString altView = QString();

    void paintEvent(QPaintEvent *event) override;

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    void  mousePressEvent ( QMouseEvent * event ) override;
};

#endif // QLIGHTTERMINAL_H
