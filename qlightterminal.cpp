/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "qlightterminal.h"

#include <QByteArray>
#include <QTextCursor>
#include <QPainter>
#include <QKeyEvent>
#include <QPoint>

#if DEBUGGING
    #include <QDebug>
#endif

// Add a zero width space at the end to start the font
#define TERM_PREFIX_FONT "<font color=\"#4fb4e1\">&#8203;<b>"
#define TERM_OUTPUT_FONT "<font color=\"#144054\">&#8203;"
#define USER_FONT "</b><font color=\"#ffffff\">&#8203;"
#define CLOSE_FONT "</font>"

QLightTerminal::QLightTerminal(): QWidget()
{
    st = new SimpleTerminal();

    // setup default style
    setStyleSheet("background: #1c2022; font-size: 14px; font-weight: 500;");
    setFont(QFont("mono"));

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);
}

QLightTerminal::~QLightTerminal(){
}

void QLightTerminal::updateTerminal(Term* term){
    this->update();
}

void QLightTerminal::paintEvent(QPaintEvent *event){

    int lineSpacing = this->fontMetrics().lineSpacing();

    QPainter painter(this);

    painter.setPen(QColor(255,255,255));

    QString line;

    int i = st->term.row;
    while(i > 0){
        i--;

        if(!st->term.dirty[i]){
            //return;
        }
        st->term.dirty[i] = 0;

        Line base = st->term.line[0];

        line = QString();

        if (IS_TRUECOL(base->fg)) {
            painter.setPen(QColor(TRUERED(base->fg),TRUEGREEN(base->fg),TRUEBLUE(base->fg)));
        } else {
        }

        for(int j = 0; j < st->term.col; j++){
            if(st->term.line[i][j].u){

                if(st->term.line[i][j].mode == ATTR_WDUMMY)
                        continue;
                line += QChar(st->term.line[i][j].u);
            }
        }

        painter.drawText(QPoint(0,(i+1)*lineSpacing), line);
    }


   /**
    *  TODO Add later
    *  if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
            colfg.red = fg->color.red / 2;
            colfg.green = fg->color.green / 2;
            colfg.blue = fg->color.blue / 2;
            colfg.alpha = fg->color.alpha;
            XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
            fg = &revfg;
        }

        if (base.mode & ATTR_REVERSE) {
            temp = fg;
            fg = bg;
            bg = temp;
        }

        if (base.mode & ATTR_BLINK && win.mode & MODE_BLINK)
            fg = bg;

        if (base.mode & ATTR_INVISIBLE)
            fg = bg;
    */
}

/*
 * Override keyEvents and send the input to the shell
 */
void QLightTerminal::keyPressEvent(QKeyEvent *e){
    QString input = e->text();

    QByteArray text;
    if(input.contains('\n')){
        text = input.left(input.indexOf('\n')+1).toUtf8();
    } else {
        text = e->text().toUtf8();
    }
    st->ttywrite(text, text.size(),1);
}

void QLightTerminal::mousePressEvent(QMouseEvent *event){
    setFocus();
}
