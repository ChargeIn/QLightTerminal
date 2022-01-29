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

QLightTerminal::QLightTerminal(): QWidget()
{
    st = new SimpleTerminal();

    // setup default style
    setStyleSheet("background: #161616; font-size: 14px; font-weight: 500;");
    setFont(QFont("mono"));
    lineheight = fontMetrics().lineSpacing()*1.25;

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);
}

void QLightTerminal::updateTerminal(Term* term){
    update();
}

int QLightTerminal::sixd_to_16bit(int x)
{
    return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

void QLightTerminal::paintEvent(QPaintEvent *event){

    int lineSpacing = fontMetrics().lineSpacing();

    QPainter painter(this);
    painter.setBackgroundMode(Qt::BGMode::OpaqueMode);

    QString line;
    uint32_t fgColor = 0;
    uint32_t bgColor = 0;
    int offset;
    bool changed = false;

    int i = st->term.row;
    while(i > 0){
        i--;

        st->term.dirty[i] = 0;
        offset = hPadding;
        line = QString();
        Glyph* tLine = st->term.line[i];

        for(int j = 0; j < st->term.col; j++){
            if(tLine[j].mode == ATTR_WDUMMY)
                    continue;

            if(fgColor != tLine[j].fg){
                fgColor = tLine[j].fg;
                changed = true;
            }

            if(bgColor != tLine[j].bg){
                bgColor = tLine[j].bg;
                changed = true;
            }


            // draw line till now and change color
            if(changed){
                painter.drawText(QPoint(offset, (i+1)*lineheight + vPadding), line);
                offset += QFontMetrics(painter.font()).size(Qt::TextSingleLine, line).width();

                if (IS_TRUECOL(fgColor)) {
                    painter.setPen(QColor(TRUERED(fgColor),TRUEGREEN(fgColor),TRUEBLUE(fgColor)));
                } else {
                   painter.setPen(colors[fgColor]);
                }

                if (IS_TRUECOL(bgColor)) {
                    painter.setBackground(QColor(TRUERED(bgColor),TRUEGREEN(bgColor),TRUEBLUE(bgColor)));
                } else {
                    painter.setBackground(QBrush(colors[bgColor]));
                }

                line = QString();
                changed = false;
            }

            line += QChar(st->term.line[i][j].u);
        }
        painter.drawText(QPoint(offset,(i+1)*lineheight + vPadding), line);
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
