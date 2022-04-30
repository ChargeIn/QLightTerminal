/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#ifndef QLIGHTTERMINAL_H
#define QLIGHTTERMINAL_H

#include <QWidget>
#include <QStringList>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QKeyCombination>
#include <QTimer>
#include <QPointF>
#include <QTime>
#include <QColor>

#include "st.h"

typedef struct {
    Qt::Key key;
    Qt::KeyboardModifier mods;
    char cmd[7];
    size_t cmd_size;
    int nextKey;
} SpecialKey;

typedef struct {
    int width;
    int height;
    int viewPortHeight; // number of lines visible
    int viewPortWidth; // number of characters per line
    int scrollMultiplier; // allows for smooth scrolling
    int fontSize;
    double lineheight;
    double charHeight;
    double charWith;
    int vPadding;
    int hPadding;
    QPointF cursorPos;
} Window;

class QLightTerminal : public QWidget {
    Q_OBJECT

public:
    QLightTerminal(QWidget *parent = nullptr);

public
    slots:
            void updateTerminal(Term * term);

    /*
     * Scrolls the terminal vertically to the given offset
     * Max scroll height is the max line count multiplied by win.scrollMultiplier
     */
    void scrollX(int x);

    void setFontSize(int size, int weight = 500);

    void setBackground(QColor color);

    void close();

    signals:
            void s_closed(); // emitted when the terminal is closed

    void s_error(QString);

protected:
    void keyPressEvent(QKeyEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void focusOutEvent(QFocusEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void updateStyleSheet();

private:
    SimpleTerminal *st;
    QScrollBar scrollbar;
    QHBoxLayout boxLayout;
    QTimer cursorTimer;
    QTimer selectionTimer;
    QTimer resizeTimer;
    Window win;

    double cursorVisible = true;

    void setupScrollbar();

    void paintEvent(QPaintEvent *event) override;

    void updateSelection();

    void resize();

    bool closed = false;
    qint64 lastClick = 0;
    bool mouseDown = false;
    bool selectionStarted = false;
    QPointF lastMousePos; // last tracked mouse pos if mouse down

    /*
     * Special Keyboard Character
     * TODO: Add more
     */
    const int defaultBackground = 259;
    constexpr static const SpecialKey
    keys[25] = {
        { Qt::Key_Left, Qt::KeyboardModifier::NoModifier, "\033[D", 3, 4 },
        { Qt::Key_Left, Qt::ShiftModifier, "\033[1;2D", 7, 3 },
        { Qt::Key_Left, Qt::AltModifier, "\033[1;3D", 7, 2 },
        { Qt::Key_Left, Qt::ControlModifier, "\033[1;5D", 7, 1 },
        { Qt::Key_Up, Qt::NoModifier, "\033[A", 3, 4 },
        { Qt::Key_Up, Qt::ShiftModifier, "\033[1;2A", 7, 3 },
        { Qt::Key_Up, Qt::AltModifier, "\033[1;3A", 7, 2 },
        { Qt::Key_Up, Qt::ControlModifier, "\033[1;5A", 7, 1 },
        { Qt::Key_Right, Qt::NoModifier, "\033[C", 3, 4 },
        { Qt::Key_Right, Qt::ShiftModifier, "\033[1;2C", 7, 3 },
        { Qt::Key_Right, Qt::AltModifier, "\033[1;3C", 7, 2 },
        { Qt::Key_Right, Qt::ControlModifier, "\033[1;5C", 7, 1 },
        { Qt::Key_Down, Qt::NoModifier, "\033[B", 3, 4 },
        { Qt::Key_Down, Qt::ShiftModifier, "\033[1;2B", 7, 3 },
        { Qt::Key_Down, Qt::AltModifier, "\033[1;3B", 7, 2 },
        { Qt::Key_Down, Qt::ControlModifier, "\033[1;5B", 7, 1 },
        { Qt::Key_F1, Qt::NoModifier, "\033OP", 3, 1 },
        { Qt::Key_F2, Qt::NoModifier, "\033OQ", 3, 1 },
        { Qt::Key_F3, Qt::NoModifier, "\033OR", 3, 1 },
        { Qt::Key_F4, Qt::NoModifier, "\033OS", 3, 1 },
        { Qt::Key_F5, Qt::NoModifier, "\033[15~", 6, 1 },
        { Qt::Key_F6, Qt::NoModifier, "\033[17~", 6, 1 },
        { Qt::Key_F7, Qt::NoModifier, "\033[18~", 6, 1 },
        { Qt::Key_F8, Qt::NoModifier, "\033[19~", 6, 1 },
        { Qt::Key_F9, Qt::NoModifier, "\033[20~", 6, 1 },
    };

    /*
     * Terminal colors (same as xterm)
     */
    QColor colors[260] = {
            // 8 normal Colors
            QColor(0, 0, 0),          // Black
            QColor(240, 82, 79),      // Red
            QColor(98, 177, 32),      // Green
            QColor(166, 138, 13),     // Yellow
            QColor(57, 147, 212),     // Blue
            QColor(167, 113, 191),    // Magenta
            QColor(0, 163, 163),      // Cyan
            QColor(128, 128, 128),    // Gray
            // 8 bright colors
                QColor(89, 89, 89),       // Dark Gray
                QColor(255, 64, 80),      // Bright Red
                QColor(79, 196, 20),      // Bright Green
                QColor(229, 191, 0),      // Bright Yellow
                QColor(31, 176, 225),     // Bright Blue
                QColor(237, 126, 237),    // Bright Magenta
                QColor(0, 229, 229),     // Bright Cyan
                QColor(255, 255, 255),    // White
                QColor(0, 0, 0),
                QColor(0, 0, 95),
                QColor(0, 0, 135),
                QColor(0, 0, 175),
                QColor(0, 0, 215),
                QColor(0, 0, 255),
                QColor(0, 95, 0),
                QColor(0, 95, 95),
                QColor(0, 95, 135),
                QColor(0, 95, 175),
                QColor(0, 95, 215),
                QColor(0, 95, 255),
                QColor(0, 135, 0),
                QColor(0, 135, 95),
                QColor(0, 135, 135),
                QColor(0, 135, 175),
                QColor(0, 135, 215),
                QColor(0, 135, 255),
                QColor(0, 175, 0),
                QColor(0, 175, 95),
                QColor(0, 175, 135),
                QColor(0, 175, 175),
                QColor(0, 175, 215),
                QColor(0, 175, 255),
                QColor(0, 215, 0),
                QColor(0, 215, 95),
                QColor(0, 215, 135),
                QColor(0, 215, 175),
                QColor(0, 215, 215),
                QColor(0, 215, 255),
                QColor(0, 255, 0),
                QColor(0, 255, 95),
                QColor(0, 255, 135),
                QColor(0, 255, 175),
                QColor(0, 255, 215),
                QColor(0, 255, 255),
                QColor(95, 0, 0),
                QColor(95, 0, 95),
                QColor(95, 0, 135),
                QColor(95, 0, 175),
                QColor(95, 0, 215),
                QColor(95, 0, 255),
                QColor(95, 95, 0),
                QColor(95, 95, 95),
                QColor(95, 95, 135),
                QColor(95, 95, 175),
                QColor(95, 95, 215),
                QColor(95, 95, 255),
                QColor(95, 135, 0),
                QColor(95, 135, 95),
                QColor(95, 135, 135),
                QColor(95, 135, 175),
                QColor(95, 135, 215),
                QColor(95, 135, 255),
                QColor(95, 175, 0),
                QColor(95, 175, 95),
                QColor(95, 175, 135),
                QColor(95, 175, 175),
                QColor(95, 175, 215),
                QColor(95, 175, 255),
                QColor(95, 215, 0),
                QColor(95, 215, 95),
                QColor(95, 215, 135),
                QColor(95, 215, 175),
                QColor(95, 215, 215),
                QColor(95, 215, 255),
                QColor(95, 255, 0),
                QColor(95, 255, 95),
                QColor(95, 255, 135),
                QColor(95, 255, 175),
                QColor(95, 255, 215),
                QColor(95, 255, 255),
                QColor(135, 0, 0),
                QColor(135, 0, 95),
                QColor(135, 0, 135),
                QColor(135, 0, 175),
                QColor(135, 0, 215),
                QColor(135, 0, 255),
                QColor(135, 95, 0),
                QColor(135, 95, 95),
                QColor(135, 95, 135),
                QColor(135, 95, 175),
                QColor(135, 95, 215),
                QColor(135, 95, 255),
                QColor(135, 135, 0),
                QColor(135, 135, 95),
                QColor(135, 135, 135),
                QColor(135, 135, 175),
                QColor(135, 135, 215),
                QColor(135, 135, 255),
                QColor(135, 175, 0),
                QColor(135, 175, 95),
                QColor(135, 175, 135),
                QColor(135, 175, 175),
                QColor(135, 175, 215),
                QColor(135, 175, 255),
                QColor(135, 215, 0),
                QColor(135, 215, 95),
                QColor(135, 215, 135),
                QColor(135, 215, 175),
                QColor(135, 215, 215),
                QColor(135, 215, 255),
                QColor(135, 255, 0),
                QColor(135, 255, 95),
                QColor(135, 255, 135),
                QColor(135, 255, 175),
                QColor(135, 255, 215),
                QColor(135, 255, 255),
                QColor(175, 0, 0),
                QColor(175, 0, 95),
                QColor(175, 0, 135),
                QColor(175, 0, 175),
                QColor(175, 0, 215),
                QColor(175, 0, 255),
                QColor(175, 95, 0),
                QColor(175, 95, 95),
                QColor(175, 95, 135),
                QColor(175, 95, 175),
                QColor(175, 95, 215),
                QColor(175, 95, 255),
                QColor(175, 135, 0),
                QColor(175, 135, 95),
                QColor(175, 135, 135),
                QColor(175, 135, 175),
                QColor(175, 135, 215),
                QColor(175, 135, 255),
                QColor(175, 175, 0),
                QColor(175, 175, 95),
                QColor(175, 175, 135),
                QColor(175, 175, 175),
                QColor(175, 175, 215),
                QColor(175, 175, 255),
                QColor(175, 215, 0),
                QColor(175, 215, 95),
                QColor(175, 215, 135),
                QColor(175, 215, 175),
                QColor(175, 215, 215),
                QColor(175, 215, 255),
                QColor(175, 255, 0),
                QColor(175, 255, 95),
                QColor(175, 255, 135),
                QColor(175, 255, 175),
                QColor(175, 255, 215),
                QColor(175, 255, 255),
                QColor(215, 0, 0),
                QColor(215, 0, 95),
                QColor(215, 0, 135),
                QColor(215, 0, 175),
                QColor(215, 0, 215),
                QColor(215, 0, 255),
                QColor(215, 95, 0),
                QColor(215, 95, 95),
                QColor(215, 95, 135),
                QColor(215, 95, 175),
                QColor(215, 95, 215),
                QColor(215, 95, 255),
                QColor(215, 135, 0),
                QColor(215, 135, 95),
                QColor(215, 135, 135),
                QColor(215, 135, 175),
                QColor(215, 135, 215),
                QColor(215, 135, 255),
                QColor(215, 175, 0),
                QColor(215, 175, 95),
                QColor(215, 175, 135),
                QColor(215, 175, 175),
                QColor(215, 175, 215),
                QColor(215, 175, 255),
                QColor(215, 215, 0),
                QColor(215, 215, 95),
                QColor(215, 215, 135),
                QColor(215, 215, 175),
                QColor(215, 215, 215),
                QColor(215, 215, 255),
                QColor(215, 255, 0),
                QColor(215, 255, 95),
                QColor(215, 255, 135),
                QColor(215, 255, 175),
                QColor(215, 255, 215),
                QColor(215, 255, 255),
                QColor(255, 0, 0),
                QColor(255, 0, 95),
                QColor(255, 0, 135),
                QColor(255, 0, 175),
                QColor(255, 0, 215),
                QColor(255, 0, 255),
                QColor(255, 95, 0),
                QColor(255, 95, 95),
                QColor(255, 95, 135),
                QColor(255, 95, 175),
                QColor(255, 95, 215),
                QColor(255, 95, 255),
                QColor(255, 135, 0),
                QColor(255, 135, 95),
                QColor(255, 135, 135),
                QColor(255, 135, 175),
                QColor(255, 135, 215),
                QColor(255, 135, 255),
                QColor(255, 175, 0),
                QColor(255, 175, 95),
                QColor(255, 175, 135),
                QColor(255, 175, 175),
                QColor(255, 175, 215),
                QColor(255, 175, 255),
                QColor(255, 215, 0),
                QColor(255, 215, 95),
                QColor(255, 215, 135),
                QColor(255, 215, 175),
                QColor(255, 215, 215),
                QColor(255, 215, 255),
                QColor(255, 255, 0),
                QColor(255, 255, 95),
                QColor(255, 255, 135),
                QColor(255, 255, 175),
                QColor(255, 255, 215),
                QColor(255, 255, 255),
                QColor(8, 8, 8),
                QColor(18, 18, 18),
                QColor(28, 28, 28),
                QColor(38, 38, 38),
                QColor(48, 48, 48),
                QColor(58, 58, 58),
                QColor(68, 68, 68),
                QColor(78, 78, 78),
                QColor(88, 88, 88),
                QColor(98, 98, 98),
                QColor(108, 108, 108),
                QColor(118, 118, 118),
                QColor(128, 128, 128),
                QColor(138, 138, 138),
                QColor(148, 148, 148),
                QColor(158, 158, 158),
                QColor(168, 168, 168),
                QColor(178, 178, 178),
                QColor(188, 188, 188),
                QColor(198, 198, 198),
                QColor(208, 208, 208),
                QColor(218, 218, 218),
                QColor(228, 228, 228),
                QColor(238, 238, 238),
                // Default colors
                QColor(255, 255, 255),
                QColor(85, 85, 85),
                QColor(200, 200, 200),        // Default font color
                QColor(24, 24, 24)            // Default background color
    };
};

#endif // QLIGHTTERMINAL_H
