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

#include "colors.h"

QLightTerminal::QLightTerminal(): QWidget()
{
    loadColors();

    st = new SimpleTerminal();

    // setup default style
    setStyleSheet("background: #1c2022; font-size: 14px; font-weight: 500;");
    setFont(QFont("mono"));

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);
}

void QLightTerminal::updateTerminal(Term* term){
    update();
}

void QLightTerminal::loadColors(){

    for(int i = 0; i < 16; i++){
        colors[i] = QColor(::colorname[i]);
        qDebug() << colors[i];
    }

    for(int i = 16; i < 260; i++){

        if (i < 6*6*6+16) { /* same colors as xterm */
            colors[i] = QColor(
                sixd_to_16bit( ((i-16)/36)%6 ),
                sixd_to_16bit( ((i-16)/6) %6 ),
                sixd_to_16bit( ((i-16)/1) %6 ),
                0xffff
            );
        } else { /* greyscale */
            int red =  0x0808 + 0x0a0a * (i - (6*6*6+16));
            colors[i] = QColor(red,red,red, 0xffff);
        }
        qDebug() << colors[i];

    }
    for(int i = 0; i < 4; i++){
        colors[256 + i] = QColor(::customcolors[i]);
        qDebug() << colors[i];
    }

}

int QLightTerminal::sixd_to_16bit(int x)
{
    return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

void QLightTerminal::paintEvent(QPaintEvent *event){

    int lineSpacing = this->fontMetrics().lineSpacing();

    QPainter painter(this);

    QString line;

    int i = st->term.row;
    while(i > 0){
        i--;

        if(!st->term.dirty[i]){
            //return;
        }
        st->term.dirty[i] = 0;

        line = QString();

        for(int j = 0; j < st->term.col; j++){
            if(st->term.line[i][j].u){

                if(st->term.line[i][j].mode == ATTR_WDUMMY)
                        continue;

                Glyph_ l =  st->term.line[i][j];

                //qDebug() << l.bg << ' ' << l.fg << ' ' << l.mode << ' ' << QChar(l.u);

                if (IS_TRUECOL(st->term.line[i][j].fg)) {
                    painter.setPen(QColor(TRUERED(st->term.line[i][j].fg),TRUEGREEN(st->term.line[i][j].fg),TRUEBLUE(st->term.line[i][j].fg)));
                } else {
                   painter.setPen(colors[st->term.line[i][j].fg]);
                }

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
