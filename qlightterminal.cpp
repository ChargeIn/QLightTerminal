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

#if DEBUGGING
    #include <QDebug>
#endif

QLightTerminal::QLightTerminal(int maxLines, QWidget *parent): QWidget(parent), scrollbar(Qt::Orientation::Vertical), boxLayout(this), cursorTimer(this),
    win{0,200,0,0, 0,100.0,14, 0, 2, 8, QPoint(0,0)}
{  
    win.maxLineCount = maxLines;

    // setup default style
    setStyleSheet("background: #181818; font-size: 14px; font-weight: 500;");
    setFont(QFont("Source Code Pro"));
    win.lineheight = fontMetrics().lineSpacing()*1.25;

    // set up scrollbar
    boxLayout.setSpacing(0);
    boxLayout.setContentsMargins(0,0,0,0);
    boxLayout.addWidget(&scrollbar);
    boxLayout.setAlignment(&scrollbar, Qt::AlignRight);

    connect(&scrollbar, &QScrollBar::valueChanged, this, &QLightTerminal::scrollX);

    win.viewPortHeight = win.height/win.lineheight;
    setupScrollbar(win.viewPortHeight);

    // set up terminal
    st = new SimpleTerminal(maxLines);

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);

    // set up blinking cursor
    connect(&cursorTimer, &QTimer::timeout, this, [this](){
        cursorVisible = !cursorVisible;
        update(win.cursorPos.x()-1, win.cursorPos.y() - fontMetrics().lineSpacing(), 8, win.lineheight);
    });
    cursorTimer.start(750);
}

void QLightTerminal::updateTerminal(Term* term){
    cursorVisible = true;
    cursorTimer.start(750);

    if(term->lastLine != win.viewPortMaxScrollY){
        win.viewPortMaxScrollY = term->lastLine;

        double newMax = MAX(win.viewPortMaxScrollY - win.viewPortHeight, 0)*win.scrollMultiplier;
        bool bottom = scrollbar.value() == scrollbar.maximum();

        scrollbar.setMaximum(newMax);
        scrollbar.setPageStep(win.viewPortHeight*win.scrollMultiplier);

        // stick to bottom
        if(bottom){
            scrollbar.setValue(newMax);
            win.viewPortScrollY = (scrollbar.maximum() - scrollbar.value())/win.scrollMultiplier;
        }
    }

    update();
}

void QLightTerminal::scrollX(int x)
{
    win.viewPortScrollY = (scrollbar.maximum() - scrollbar.value())/win.scrollMultiplier;
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

    // caluate the view port
    double lastline = win.viewPortMaxScrollY - win.viewPortScrollY; // last line which should be drawn
    double drawOffset = event->rect().y()/win.lineheight;
    double drawHeight = event->rect().height()/win.lineheight;
    double start = lastline - drawHeight - drawOffset;
    start = MAX(start+1, 0);
    double end = start + drawHeight;
    end = MIN(end, win.maxLineCount);

    int i = end;
    int stop = start;
    double yPos = (drawOffset + drawHeight - 1)*win.lineheight;

    while(i > stop){
        i--;

        st->term.dirty[i] = 0;
        offset = win.hPadding;
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
        yPos -= win.lineheight;
    }

    if(!cursorVisible){
        return;
    }

    // draw cursor
    painter.setBackgroundMode(Qt::BGMode::TransparentMode);
    line = QString();

    for(int i = 0; i < st->term.c.x; i++) {
        line += QChar(st->term.line[st->term.c.y][i].u);
    }
    int cursorOffset = QFontMetrics(painter.font()).size(Qt::TextSingleLine, line).width();

    painter.setPen(colors[st->term.c.attr.fg]);
    win.cursorPos = QPoint(cursorOffset + win.hPadding, (MIN(st->term.c.y+1, win.viewPortHeight-1)+win.viewPortScrollY)*win.lineheight - win.vPadding);
    painter.drawText(win.cursorPos, QChar(st->term.c.attr.u));

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
        Qt::KeyboardModifiers mods = e->modifiers();
        int key = e->key();
        int end = 24;

        if(key > keys[end].key || key < keys[0].key){
            return;
        }

        for(int i = 0; i < end;){
            int nextKey = keys[i].nextKey;
            if(key == keys[i].key){
                for(int j = i; j < i + nextKey; j++){
                    if(mods == keys[j].mods){
                        st->ttywrite(keys[j].cmd, keys[j].cmd_size,1);
                        return;
                    }
                }
            }
            i += nextKey;
        }
    }
}

void QLightTerminal::mousePressEvent(QMouseEvent *event){
    setFocus();
}

void QLightTerminal::resizeEvent(QResizeEvent *event)
{
    win.height = event->size().height();
    win.width = event->size().width();
    win.viewPortHeight = win.height/win.lineheight;

    // multiply by 100 to allow for smoother scrolling
    scrollbar.setMaximum((win.maxLineCount-win.viewPortHeight)*win.scrollMultiplier);

    int col;
    // TODO: figure out why fontMetrics().maxWidth() is returning wrong size;
    // for now replaced with 8.5

    col = (event->size().width() - 2 * win.hPadding) / 8.5;
    col = MAX(1, col);

    st->tresize(col, win.maxLineCount);
    st->ttyresize(col*8.5, win.maxLineCount*win.lineheight);

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

void QLightTerminal::setupScrollbar(int maxLines){
    double scrollHeight = (maxLines-win.viewPortHeight)*win.scrollMultiplier;
    scrollbar.setMaximum(scrollHeight);
    scrollbar.setValue(scrollHeight);
    scrollbar.setPageStep(scrollHeight/10);
    scrollbar.setStyleSheet(R"(
                        QScrollBar::sub-line:vertical, QScrollBar::add-line:vertical {
                            height: 0;
                        }

                        QScrollBar::vertical  {
                            margin: 0;
                        }
    )");
}
