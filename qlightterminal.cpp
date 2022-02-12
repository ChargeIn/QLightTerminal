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
#include <QClipboard>
#include <QGuiApplication>
#include <QPointF>

#if DEBUGGING
    #include <QDebug>
#endif

QLightTerminal::QLightTerminal(QWidget *parent): QWidget(parent), scrollbar(Qt::Orientation::Vertical), boxLayout(this), cursorTimer(this),
    win{0,0,0,100,14, 8.5, 2, 8, QPoint(0,0)}
{  
    // set up terminal
    st = new SimpleTerminal();

    // setup default style
    setStyleSheet("background: #181818; font-size: 14px; font-weight: 500;");
    setFont(QFont("Mono")); // terminal is based on monospace fonts
    int linespacing = fontMetrics().lineSpacing();
    win.lineheight = linespacing*1.25;

    // set up scrollbar
    boxLayout.setSpacing(0);
    boxLayout.setContentsMargins(0,0,0,0);
    boxLayout.addWidget(&scrollbar);
    boxLayout.setAlignment(&scrollbar, Qt::AlignRight);

    connect(&scrollbar, &QScrollBar::valueChanged, this, &QLightTerminal::scrollX);

    win.viewPortHeight = win.height/win.lineheight;
    setupScrollbar();

    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);

    // set up blinking cursor
    connect(&cursorTimer, &QTimer::timeout, this, [this](){
        cursorVisible = !cursorVisible;
        update(win.cursorPos.x() - 0.1, win.cursorPos.y() - 14, 10, win.lineheight);
    });
    cursorTimer.start(750);
}

void QLightTerminal::updateTerminal(Term* term){
    cursorVisible = true;
    cursorTimer.start(750);

    if(st->term.histi != scrollbar.maximum()){
        bool isMax = scrollbar.value() == scrollbar.value();
        scrollbar.setMaximum(st->term.histi*win.scrollMultiplier);

        // stick to the bottom
        if(isMax){
            scrollbar.setValue(scrollbar.maximum());
        }
        scrollbar.setVisible(scrollbar.maximum() != 0);
    }
    update();
}

void QLightTerminal::scrollX(int n)
{
    int scroll = (st->term.scr - (scrollbar.maximum() - scrollbar.value())/win.scrollMultiplier);

    if(scroll < 0){
        st->kscrollup(-scroll);
    } else {
        st->kscrolldown(scroll);
    }
    update();
}

void QLightTerminal::paintEvent(QPaintEvent *event){
    QPainter painter(this);
    painter.setBackgroundMode(Qt::BGMode::OpaqueMode);

    QString line;
    uint32_t fgColor = 0;
    uint32_t bgColor = 0;
    uint32_t cfgColor = 0; // control for change
    uint32_t cbgColor = 0; // control for change
    ushort mode = -1;
    int offset;
    bool changed = false;

    // caluate the view port
    int drawOffset = MAX(event->rect().y() - win.vPadding, 0)/win.lineheight;   // line index offset of the viewPort
    int drawHeight = (event->rect().height())/win.lineheight;                   // height of the viewPort in lines
    int drawEnd = drawOffset + drawHeight;                                      // last line index of the viewPort

    int i = MIN(drawEnd, win.viewPortHeight);
    int stop = MAX(i - drawHeight, 0);
    int yPos = i*win.lineheight + win.vPadding;                // y position of the the lastViewPortLine

    int temp;

    while(i > stop){
        i--;

        offset = win.hPadding;
        line = QString();

        // same logic as TLine from st-utils
        Glyph* tLine = ((i) < st->term.scr ? st->term.hist[((i) + st->term.histi - \
                                                    st->term.scr + HISTSIZE + 1) % HISTSIZE] : \
                                                    st->term.line[(i) - st->term.scr]);

        for(int j = 0; j < st->term.col; j++){
            Glyph g = tLine[j];
            if(g.mode == ATTR_WDUMMY)
                    continue;

            if(cfgColor != g.fg){
                fgColor = g.fg;
                cfgColor = g.fg;
                changed = true;
            }

            if(cbgColor != g.bg){
                bgColor = g.bg;
                cbgColor = g.bg;
                changed = true;
            }

            if(mode != g.mode){
                mode = g.mode;
                changed = true;

                if ((g.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
                    painter.setOpacity(0.5);
                } else {
                    painter.setOpacity(1);
                }

                if (g.mode & ATTR_REVERSE) {
                    qDebug() << g.mode;
                    temp = fgColor;
                    fgColor = bgColor;
                    bgColor = temp;
                }

                if (g.mode & ATTR_INVISIBLE)
                    fgColor = bgColor;
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

            line += QChar(g.u);
        }
        painter.drawText(QPoint(offset,yPos), line);
        yPos -= win.lineheight;
    }

    if(st->term.scr != 0){
        return; // do not draw cursor is scrolled
    }

    // draw cursor
    painter.setBackgroundMode(Qt::BGMode::TransparentMode);
    painter.setPen(colors[st->term.c.attr.fg]);
    line = QString();

    for(int i = 0; i < st->term.c.x; i++) {
        line += QChar(st->term.line[st->term.c.y][i].u);
    }
    int cursorOffset = QFontMetrics(painter.font()).size(Qt::TextSingleLine, line).width();

    double cursorPos = MIN(st->term.c.y+1, (int) win.viewPortHeight); // line of the cursor

    win.cursorPos = QPoint(cursorOffset + win.hPadding, cursorPos*win.lineheight + win.vPadding);

    if(!cursorVisible){
        return;
    }
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

        if (base.mode & ATTR_BLINK && win.mode & MODE_BLINK)
            fg = bg;
    */
}

/*
 * Override keyEvents and send the input to the shell
 */
void QLightTerminal::keyPressEvent(QKeyEvent *e){
    QString input = e->text();
    Qt::KeyboardModifiers mods = e->modifiers();

    // paste event
    if(
        e->key() == 86
        && mods & Qt::KeyboardModifier::ShiftModifier
        && mods & Qt::KeyboardModifier::ControlModifier)
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        QString clippedText = clipboard->text();
        QByteArray data = clippedText.toLocal8Bit();
        st->ttywrite(data.data(), data.size(),1);
        return;
    }

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
        int key = e->key();
        int end = 24;

        if(key > keys[end].key || key < keys[0].key){
            return;
        }

        for(int i = 0; i < end;){
            int nextKey = keys[i].nextKey;
            if(key == keys[i].key){
                for(int j = i; j < i + nextKey; j++){
                    if(mods.testFlag(keys[j].mods)){
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

    // draw cursor
    cursorVisible = true;
    update(win.cursorPos.x()-1, win.cursorPos.y() - fontMetrics().lineSpacing(), 8, win.lineheight); // draw cursor
    cursorTimer.start(750);
}

void QLightTerminal::mouseDoubleClickEvent(QMouseEvent *event){
    // TODO
    QPointF pos = event->position();
    int col = (pos.x() - win.hPadding)/win.charWith;
    int row = (pos.y() - win.vPadding)/win.lineheight;

    st->selstart(col, row, SNAP_WORD);
}

void QLightTerminal::resizeEvent(QResizeEvent *event)
{
    win.height = event->size().height();
    win.width = event->size().width();
    win.viewPortHeight = (win.height-win.vPadding*2)/win.lineheight;

    int col;
    // TODO: figure out why fontMetrics().maxWidth() is returning wrong size;
    // for now replaced with 8.5 (win.charWidth)

    col = (event->size().width() - 2 * win.hPadding) / win.charWith;
    col = MAX(1, col);

    st->tresize(col, win.viewPortHeight);
    st->ttyresize(col*8.5, win.viewPortHeight*win.lineheight);

    event->accept();
}

void QLightTerminal::wheelEvent(QWheelEvent *event)
{
    QPoint numPixels = event->pixelDelta();
    event->accept();

    if (!numPixels.isNull()) {
        // TODO: test if this works << needs mouse with finer resolution
        scrollbar.setValue(scrollbar.value() - numPixels.y()*2);
        scrollX(scrollbar.value());
        return;
    }

    QPoint numDegrees = event->angleDelta();

    if (!numDegrees.isNull()) {
        scrollbar.setValue(scrollbar.value() - numDegrees.y()*2);
        scrollX(scrollbar.value());
        return;
    }
}

void QLightTerminal::focusOutEvent(QFocusEvent *event)
{
    cursorTimer.stop();
    cursorVisible = false;
    update(win.cursorPos.x()-1, win.cursorPos.y() - fontMetrics().lineSpacing(), 8, win.lineheight); // hide cursor
}

void QLightTerminal::setupScrollbar(){
    scrollbar.setMaximum(0); // will set in the update Terminal function
    scrollbar.setValue(0);
    scrollbar.setPageStep(100*win.scrollMultiplier);
    scrollbar.setVisible(false);
    scrollbar.setStyleSheet(R"(
                        QScrollBar::sub-line:vertical, QScrollBar::add-line:vertical {
                            height: 0;
                        }

                        QScrollBar::vertical  {
                            margin: 0;
                            width: 9px;
                            padding-right: 1px;
                        }

                        QScrollBar::handle {
                            background: #aaaaaa;
                            border-radius: 4px;
                        }
    )");
}
