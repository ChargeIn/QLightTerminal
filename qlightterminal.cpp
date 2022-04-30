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
#include <QFontMetricsF>

QLightTerminal::QLightTerminal(QWidget *parent) : QWidget(parent), scrollbar(Qt::Orientation::Vertical),
                                                  boxLayout(this), cursorTimer(this), selectionTimer(this),
                                                  win{0, 0, 0, 0, 100, 10, 10, 10, 8.42, 2, 8, QPoint(0, 0)} {
    // set up terminal
    st = new SimpleTerminal();

    // setup default style
    // Note: font size is not reliable use win.charWidth for length computation
    setAttribute(Qt::WA_StyledBackground, true);
    this->setFontSize(10, 500);
    this->updateStyleSheet();

    // set up scrollbar
    boxLayout.setSpacing(0);
    boxLayout.setContentsMargins(0, 0, 0, 0);
    boxLayout.addWidget(&scrollbar);
    boxLayout.setAlignment(&scrollbar, Qt::AlignRight);

    connect(&scrollbar, &QScrollBar::valueChanged, this, &QLightTerminal::scrollX);

    win.viewPortHeight = win.height / win.lineheight;
    setupScrollbar();

    connect(st, &SimpleTerminal::s_error, this, [this](QString error) { emit s_error("Error from st: " + error); });
    connect(st, &SimpleTerminal::s_updateView, this, &QLightTerminal::updateTerminal);

    // set up blinking cursor
    connect(&cursorTimer, &QTimer::timeout, this, [this]() {
        cursorVisible = !cursorVisible;
        // update only takes int position and not floating point -> add padding
        update(win.cursorPos.x() - 1, win.cursorPos.y() - win.charHeight - 1, win.charWith + 2, win.lineheight + 2);
    });
    cursorTimer.start(750);

    // allows for auto scrolling on selection reaching the borders
    connect(&selectionTimer, &QTimer::timeout, this, &QLightTerminal::updateSelection);

    // debounce resizing
    connect(&resizeTimer, &QTimer::timeout, this, &QLightTerminal::resize);

    // connect close event of the tty
    connect(st, &SimpleTerminal::s_closed, this, &QLightTerminal::close);
}

void QLightTerminal::close() {
    setDisabled(true);
    closed = true;

    scrollbar.setVisible(false);

    cursorTimer.stop();
    selectionTimer.stop();
    update();

    emit s_closed();
}

void QLightTerminal::updateTerminal(Term *term) {
    cursorVisible = true;
    cursorTimer.start(750);

    if (st->term.histi != scrollbar.maximum()) {
        bool isMax = scrollbar.value() == scrollbar.value();
        scrollbar.setMaximum(st->term.histi * win.scrollMultiplier);

        // stick to the bottom
        if (isMax) {
            scrollbar.setValue(scrollbar.maximum());
        }
        scrollbar.setVisible(scrollbar.maximum() != 0);
    }
    update();
}

void QLightTerminal::scrollX(int n) {
    int scroll = (st->term.scr - (scrollbar.maximum() - scrollbar.value()) / win.scrollMultiplier);

    if (scroll < 0) {
        st->kscrollup(-scroll);
    } else {
        st->kscrolldown(scroll);
    }
    update();
}

void QLightTerminal::setFontSize(int size, int weight) {
    QFont mono = QFont("Monospace", size, weight);
    mono.setFixedPitch(true);
    mono.setStyleHint(QFont::Monospace);

    setFont(mono); // font must be monospace

    QFontMetricsF metric = QFontMetricsF(mono);

    int linespacing = metric.lineSpacing();
    this->win.lineheight = linespacing * 1.25;
    this->win.fontSize = size;
    this->win.charWith = metric.averageCharWidth();
    this->win.charHeight = metric.lineSpacing();
}

void QLightTerminal::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setBackgroundMode(Qt::BGMode::OpaqueMode);

    if (closed) {
        painter.drawText(QPointF(win.hPadding, win.lineheight + win.vPadding), "Terminal is closed.");
        return;
    }

    QString line;
    uint32_t fgColor = 0;
    uint32_t bgColor = 0;
    uint32_t cfgColor = 0; // control for change detection
    uint32_t cbgColor = 0; // control for change detection
    ushort mode = -1;
    double offset;
    bool changed = false;

    // calculate the view port
    int drawOffset = MAX(event->rect().y() - win.vPadding, 0) / win.lineheight;   // line index offset of the viewPort
    int drawHeight = (event->rect().height()) / win.lineheight;                   // height of the viewPort in lines
    int drawEnd = drawOffset + drawHeight;                                      // last line index of the viewPort

    int i = MIN(drawEnd, win.viewPortHeight);
    int stop = MAX(i - drawHeight, 0);
    double yPos = i * win.lineheight + win.vPadding;                // y position of the the lastViewPortLine

    int temp;

    while (i > stop) {
        i--;

        offset = win.hPadding;
        line = QString();

        // same logic as TLine from st-utils
        Glyph *tLine = ((i) < st->term.scr ? st->term.hist[((i) + st->term.histi - \
                                                    st->term.scr + HISTSIZE + 1) % HISTSIZE] : \
                                                    st->term.line[(i) - st->term.scr]);

        for (int j = 0; j < st->term.col; j++) {
            Glyph g = tLine[j];
            if (g.mode == ATTR_WDUMMY)
                continue;

            if (cfgColor != g.fg) {
                fgColor = g.fg;
                cfgColor = g.fg;
                changed = true;
            }

            if (cbgColor != g.bg) {
                bgColor = g.bg;
                cbgColor = g.bg;
                changed = true;
            }

            if (st->selected(j, i)) {
                g.mode ^= ATTR_REVERSE;
            }

            if (mode != g.mode) {
                mode = g.mode;
                changed = true;
                fgColor = cfgColor; // reset color
                bgColor = cbgColor;

                if ((g.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
                    painter.setOpacity(0.5);
                } else {
                    painter.setOpacity(1);
                }

                if (g.mode & ATTR_REVERSE) {
                    temp = fgColor;
                    fgColor = bgColor;
                    bgColor = temp;
                }

                if (g.mode & ATTR_INVISIBLE)
                    fgColor = bgColor;
            }

            // draw current line state and change color
            if (changed) {
                painter.drawText(QPointF(offset, yPos), line);
                offset += line.size() * win.charWith;

                if (IS_TRUECOL(fgColor)) {
                    painter.setPen(QColor(TRUERED(fgColor), TRUEGREEN(fgColor), TRUEBLUE(fgColor)));
                } else {
                    painter.setPen(colors[fgColor]);
                }

                if (IS_TRUECOL(bgColor)) {
                    painter.setBackground(QBrush(QColor(TRUERED(bgColor), TRUEGREEN(bgColor), TRUEBLUE(bgColor))));
                } else {
                    painter.setBackground(QBrush(colors[bgColor]));
                }

                line = QString();
                changed = false;
            }

            line += QChar(g.u);
        }
        painter.drawText(QPointF(offset, yPos), line);
        yPos -= win.lineheight;
    }

    if (st->term.scr != 0) {
        return; // do not draw, cursor is scrolled out of view
    }

    // draw cursor
    // drawn by reversing foreground color and background color
    painter.setPen(colors[st->term.c.attr.bg]);
    painter.setBackground(QBrush(colors[st->term.c.attr.fg]));
    line = QString();

    for (int i = 0; i < st->term.c.x; i++) {
        line += QChar(st->term.line[st->term.c.y][i].u);
    }
    int cursorOffset = line.size() * win.charWith;

    double cursorPos = MIN(st->term.c.y + 1, win.viewPortHeight); // line of the cursor

    win.cursorPos = QPointF(cursorOffset + win.hPadding, cursorPos * win.lineheight + win.vPadding);

    if (!cursorVisible) {
        return;
    }

    QChar charAtCursor = QChar(st->term.line[st->term.c.y][st->term.c.x].u);
    painter.drawText(win.cursorPos, charAtCursor);

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
void QLightTerminal::keyPressEvent(QKeyEvent *e) {
    QString input = e->text();
    Qt::KeyboardModifiers mods = e->modifiers();
    int key = e->key();

    if (key == Qt::Key_Backspace) {
        if (mods.testFlag(Qt::KeyboardModifier::AltModifier)) {
            st->ttywrite("\033\177", 2, 1);
        } else {
            st->ttywrite("\177", 1, 1);
        }
        return;
    }

    // paste event
    if (
            key == 86
            && mods & Qt::KeyboardModifier::ShiftModifier
            && mods & Qt::KeyboardModifier::ControlModifier) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        QString clippedText = clipboard->text();
        QByteArray data = clippedText.toLocal8Bit();
        st->ttywrite(data.data(), data.size(), 1);
        return;
    }

    // copy event
    if (
            key == 67
            && mods & Qt::KeyboardModifier::ShiftModifier
            && mods & Qt::KeyboardModifier::ControlModifier) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        QString clippedText = QString(st->getsel());
        clipboard->setText(clippedText);
        return;
    }


    // normal input
    if (input != "") {
        QByteArray text;
        if (input.contains('\n')) {
            text = input.left(input.indexOf('\n') + 1).toUtf8();
        } else {
            text = e->text().toUtf8();
        }
        st->ttywrite(text, text.size(), 1);
    } else {
        // special keys
        // TODO: Add more short cuts
        int end = 24;

        if (key > keys[end].key || key < keys[0].key) {
            return;
        }

        for (int i = 0; i < end;) {
            int nextKey = keys[i].nextKey;
            if (key == keys[i].key) {
                for (int j = i; j < i + nextKey; j++) {
                    if (mods.testFlag(keys[j].mods)) {
                        st->ttywrite(keys[j].cmd, keys[j].cmd_size, 1);
                        return;
                    }
                }
            }
            i += nextKey;
        }
    }
}

void QLightTerminal::mousePressEvent(QMouseEvent *event) {
    setFocus();
    mouseDown = true;
    lastMousePos = event->pos();

    // reset old selection
    st->selclear();
    update();

    // select line if tripple click
    if (QDateTime::currentMSecsSinceEpoch() - lastClick < 500) {
        lastClick = 0;
        QPointF pos = event->position();
        int col = (pos.x() - win.hPadding) / win.charWith;
        int row = (pos.y() - win.vPadding) / win.lineheight;

        st->selstart(col, row, SNAP_LINE);
    }

    // draw cursor
    cursorVisible = true;
    update(win.cursorPos.x() - 1, win.cursorPos.y() - fontMetrics().lineSpacing(), 8, win.lineheight); // draw cursor
    cursorTimer.start(750);
}

void QLightTerminal::mouseReleaseEvent(QMouseEvent *event) {
    mouseDown = false;

    // close selection if started
    if (selectionStarted) {
        QPointF pos = event->position();
        int col = (pos.x() - win.hPadding) / win.charWith;
        int row = (pos.y() - win.vPadding) / win.lineheight;

        col = MIN(col, win.viewPortWidth - 1);
        row = MIN(row, win.viewPortHeight - 1);

        st->selextend(col, row, SEL_REGULAR, 1);
        selectionStarted = false;
        selectionTimer.stop();
        update();
    }
};

void QLightTerminal::mouseMoveEvent(QMouseEvent *event) {
    // selection handling
    if (mouseDown) {
        lastMousePos = event->position();

        if (!selectionStarted) {
            QPointF pos = event->position();
            int col = (pos.x() - win.hPadding) / win.charWith;
            double row = (pos.y() - win.vPadding) / win.lineheight;
            st->selstart(col, row, 0);
            selectionTimer.start(100);
            selectionStarted = true;
        }
    }
}

void QLightTerminal::updateStyleSheet() {
    QString stylesheet;

    stylesheet += "background-color:" + this->colors[this->defaultBackground].name() + ";";

    setStyleSheet(stylesheet);
};

void QLightTerminal::updateSelection() {
    if (selectionStarted) {
        int col = (lastMousePos.x() - win.hPadding) / win.charWith;
        double row = (lastMousePos.y() - win.vPadding) / win.lineheight;

        if (row < 0.4) {
            // scroll up
            double scroll = MIN(scrollbar.value() / win.lineheight, (row - 0.4) * -2);
            scrollbar.setValue(scrollbar.value() - scroll * win.scrollMultiplier);
        }
        if (row > win.viewPortHeight - 0.4) {
            // scroll down
            double scroll = MIN((scrollbar.maximum() - scrollbar.value()) / win.lineheight,
                                (row - win.viewPortHeight + 0.4) * 2);
            scrollbar.setValue(scrollbar.value() + scroll * win.scrollMultiplier);
        }

        col = MIN(col, win.viewPortWidth - 1);
        row = MIN(row, win.viewPortHeight - 1);

        st->selextend(col, row, SEL_REGULAR, 0);
        update();
    }
}

void QLightTerminal::mouseDoubleClickEvent(QMouseEvent *event) {
    QPointF pos = event->position();
    int col = (pos.x() - win.hPadding) / win.charWith;
    int row = (pos.y() - win.vPadding) / win.lineheight;

    st->selclear();
    st->selstart(col, row, SNAP_WORD);

    lastClick = QDateTime::currentMSecsSinceEpoch();

    update();
}

void QLightTerminal::resizeEvent(QResizeEvent *event) {
    if (closed) {
        return;
    }

    win.height = event->size().height();
    win.width = event->size().width();

    if (resizeTimer.isActive()) {
        return;
    }

    resizeTimer.start(300);

    event->accept();
}

void QLightTerminal::resize() {
    // allow more padding at the bottom
    int rows = (win.height - win.vPadding * 2 - win.lineheight / 2) / win.lineheight;


    int cols = (win.width - 2 * win.hPadding) / win.charWith;
    cols = MAX(1, cols);

    if (cols == win.viewPortWidth && rows == win.viewPortHeight) {
        return;
    }

    win.viewPortWidth = cols;
    win.viewPortHeight = rows;

    st->tresize(cols, win.viewPortHeight);
    st->ttyresize(cols * 8.5, win.viewPortHeight * win.lineheight);
}

void QLightTerminal::wheelEvent(QWheelEvent *event) {
    QPoint numPixels = event->pixelDelta();
    event->accept();

    if (!numPixels.isNull()) {
        // TODO: test if this works << needs mouse with finer resolution
        scrollbar.setValue(scrollbar.value() - numPixels.y() * 2);
        return;
    }

    QPoint numDegrees = event->angleDelta();

    if (!numDegrees.isNull()) {
        scrollbar.setValue(scrollbar.value() - numDegrees.y() * 2);
        return;
    }
}

void QLightTerminal::focusOutEvent(QFocusEvent *event) {
    cursorTimer.stop();
    cursorVisible = false;
    // redraw cursor position
    // update only takes int position and not floating point -> add padding
    update(win.cursorPos.x() - 1, win.cursorPos.y() - win.charHeight - 1, win.charWith + 2, win.lineheight + 2);
}

void QLightTerminal::setupScrollbar() {
    scrollbar.setMaximum(0); // will set in the update Terminal function
    scrollbar.setValue(0);
    scrollbar.setPageStep(100 * win.scrollMultiplier);
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
