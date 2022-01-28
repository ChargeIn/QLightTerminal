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

public slots:
    void updateTerminal(Term* term);

private:
    SimpleTerminal * st;
    QString altView = QString();
    QColor colors[260];

    void paintEvent(QPaintEvent *event) override;

    void loadColors();

    int sixd_to_16bit(int x);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    void  mousePressEvent ( QMouseEvent * event ) override;
};

#endif // QLIGHTTERMINAL_H
