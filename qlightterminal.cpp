/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "qlightterminal.h"

#include <QByteArray>
#include <QTextCursor>
#include <QPainter>
#include <QKeyEvent>
#include <QPoint>
#include <QLayout>
#include <QGraphicsAnchorLayout>
#include <QPoint>
#include <QKeyCombination>

#if DEBUGGING
    #include <QDebug>
#endif

QLightTerminal::QLightTerminal(int maxLines): QWidget(), scrollbar(Qt::Orientation::Vertical), boxLayout(this)
{
    maxLineCount = maxLines;

    // setup default style
    setStyleSheet("background: #161616; font-size: 14px; font-weight: 500;");
    setFont(QFont("mono"));
    lineheight = fontMetrics().lineSpacing()*1.25;

    // set up scrollbar
    boxLayout.setSpacing(0);
    boxLayout.setContentsMargins(0,0,0,0);
    boxLayout.addWidget(&scrollbar);
    boxLayout.setAlignment(&scrollbar, Qt::AlignRight);

    connect(&scrollbar, &QScrollBar::valueChanged, this, &QLightTerminal::scrollX);

    viewPortHeight = height/lineheight;
    double scrollHeight = (maxLines-viewPortHeight)*scrollMultiplier;
    scrollbar.setMaximum(scrollHeight);
    scrollbar.setValue(scrollHeight);

    // set up terminal
    st = new SimpleTerminal(maxLines);

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);
}

void QLightTerminal::updateTerminal(Term* term){
    update();
}

void QLightTerminal::scrollX(int x)
{
    viewPortScrollX = maxLineCount - viewPortHeight - x/scrollMultiplier;
    update();
}

void QLightTerminal::paintEvent(QPaintEvent *event){

    QPainter painter(this);
    painter.setBackgroundMode(Qt::BGMode::OpaqueMode);

    QString line;
    uint32_t fgColor = 0;
    uint32_t bgColor = 0;
    int offset;
    bool changed = false;

    // apply a padding of two 2 lines
    double start = MAX(MIN(st->term.c.y-viewPortScrollX+2, maxLineCount), viewPortHeight);
    double end = MAX(0, start-viewPortHeight - 2);
    double yPos = viewPortHeight*lineheight + ((int) start - start)*lineheight;

    // draw the terminal text
    int i = start;
    while(i > end){
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


            // draw line state now and change color
            if(changed){
                painter.drawText(QPoint(offset, yPos), line);
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
        painter.drawText(QPoint(offset,yPos), line);
        yPos -= lineheight;
    }


    // draw cursor
    line = QString();

    for(int i = 0; i < st->term.c.x; i++) {
        line += QChar(st->term.line[st->term.c.y][i].u);
    }
    int cursorOffset = QFontMetrics(painter.font()).size(Qt::TextSingleLine, line).width();

    painter.setPen(colors[st->term.c.attr.fg]);
    painter.drawText(QPoint(cursorOffset + hPadding,(st->term.c.y+1)*lineheight), QChar(st->term.c.attr.u));

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

    // normal input
    if(input != ""){
        QByteArray text;
        if(input.contains('\n')){
            text = input.left(input.indexOf('\n')+1).toUtf8();
        } else {
            text = e->text().toUtf8();
        }
        st->ttywrite(text, text.size(),1);
    } else {
        // special keys
        // TODO: Add more short cuts
        // TODO: Refactor the if statements to dynamically add the escape sequence and modifier
        switch(e->key()){
        case Qt::Key_Up:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[A", 3,1);
            } else if(mods == Qt::ShiftModifier){
                st->ttywrite("\033[1;2A",5,1);
            } else if(mods == Qt::AltModifier){
                st->ttywrite("\033[1;3A",5,1);
            } else if(mods == Qt::ControlModifier){
                st->ttywrite("\033[1;5A",5,1);
            } else {
                st->ttywrite("\033[A", 3,1);
            }
            break;
        case Qt::Key_Down:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[B", 3,1);
            } else if(mods == Qt::ShiftModifier){
                st->ttywrite("\033[1;2B",5,1);
            } else if(mods == Qt::AltModifier){
                st->ttywrite("\033[1;3B",5,1);
            } else if(mods == Qt::ControlModifier){
                st->ttywrite("\033[1;5B",5,1);
            } else {
                st->ttywrite("\033[B", 3,1);
            }
            break;
        case Qt::Key_Left:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[D", 3,1);
            } else if(mods == Qt::ShiftModifier){
                st->ttywrite("\033[1;2D",5,1);
            } else if(mods == Qt::AltModifier){
                st->ttywrite("\033[1;3D",5,1);
            } else if(mods == Qt::ControlModifier){
                st->ttywrite("\033[1;5D",5,1);
            } else {
                st->ttywrite("\033[D", 3,1);
            }
            break;
        case Qt::Key_Right:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[C", 3,1);
            } else if(mods == Qt::ShiftModifier){
                st->ttywrite("\033[1;2C",5,1);
            } else if(mods == Qt::AltModifier){
                st->ttywrite("\033[1;3C",5,1);
            } else if(mods == Qt::ControlModifier){
                st->ttywrite("\033[1;5C",5,1);
            } else {
                st->ttywrite("\033[C", 3,1);
            }
            break;
        case Qt::Key_F1:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033OP",3,1);
            }
            break;
        case Qt::Key_F2:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033OQ",3,1);
            }
            break;
        case Qt::Key_F3:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033OR",3,1);
            }
            break;
        case Qt::Key_F4:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033OS",3,1);
            }
            break;
        case Qt::Key_F5:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[15~",6,1);
            }
            break;
        case Qt::Key_F6:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[17~",6,1);
            }
            break;
        case Qt::Key_F7:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[18~",6,1);
            }
            break;
        case Qt::Key_F8:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[19~",6,1);
            }
            break;
        case Qt::Key_F9:
            if(mods == Qt::NoModifier){
                st->ttywrite("\033[20~",6,1);
            }
            break;
        default:
            // do nothing;
            break;
        }
    }
}

void QLightTerminal::mousePressEvent(QMouseEvent *event){
    setFocus();
}

void QLightTerminal::resizeEvent(QResizeEvent *event)
{
    height = event->size().height();
    viewPortHeight = height/lineheight;

    // multiply by 100 to allow for smoother scrolling
    scrollbar.setMaximum((maxLineCount-viewPortHeight)*scrollMultiplier);

    int col;
    // TODO: figure out why fontMetrics().maxWidth() is returning wrong size;
    // for now replaced with 8.5

    col = (event->size().width() - 2 * hPadding) / 8.5;
    col = MAX(1, col);

    st->tresize(col, maxLineCount);
    st->ttyresize(col*8.5, maxLineCount*lineheight);

    update();

    event->accept();
}

void QLightTerminal::wheelEvent(QWheelEvent *event)
{
    QPoint numPixels = event->pixelDelta();
    event->accept();

    if (!numPixels.isNull()) {
        // TODO: test if this works << needs mouse with finer resolution
        scrollbar.setValue(scrollbar.value() - numPixels.y()*1.5);
        scrollX(scrollbar.value());
        return;
    }

    QPoint numDegrees = event->angleDelta();

    if (!numDegrees.isNull()) {
        scrollbar.setValue(scrollbar.value() - numDegrees.y()*1.5);
        scrollX(scrollbar.value());
        return;
    }
}
