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

    void paintEvent(QPaintEvent *event) override;

    int sixd_to_16bit(int x);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    void  mousePressEvent ( QMouseEvent * event ) override;


private:
    /*
     * Terminal colors (same as xterm)
     */
    constexpr static const QColor colors[260] = {
        // 8 normal Colors
        QColor(0,0,0),          // Black
        QColor(240,82,79),      // Red
        QColor(92,150,44),      // Green
        QColor(166,138,13),     // Yellow
        QColor(57,147,212),     // Blue
        QColor(167,113,191),    // Magenta
        QColor(0,163,163),      // Cyan
        QColor(128,128,128),    // Gray
        // 8 bright colors
        QColor(89,89,89),       // Dark Gray
        QColor(255,64,80),      // Bright Red
        QColor(79,196,20),      // Bright Green
        QColor(229,191,0),      // Bright Yellow
        QColor(31,176,225),     // Bright Blue
        QColor(237,126,237),    // Bright Magenta
        QColor(0,229,229),     // Bright Cyan
        QColor(255,255,255),    // White
        QColor(0,0,0),
        QColor(0,0,95),
        QColor(0,0,135),
        QColor(0,0,175),
        QColor(0,0,215),
        QColor(0,0,255),
        QColor(0,95,0),
        QColor(0,95,95),
        QColor(0,95,135),
        QColor(0,95,175),
        QColor(0,95,215),
        QColor(0,95,255),
        QColor(0,135,0),
        QColor(0,135,95),
        QColor(0,135,135),
        QColor(0,135,175),
        QColor(0,135,215),
        QColor(0,135,255),
        QColor(0,175,0),
        QColor(0,175,95),
        QColor(0,175,135),
        QColor(0,175,175),
        QColor(0,175,215),
        QColor(0,175,255),
        QColor(0,215,0),
        QColor(0,215,95),
        QColor(0,215,135),
        QColor(0,215,175),
        QColor(0,215,215),
        QColor(0,215,255),
        QColor(0,255,0),
        QColor(0,255,95),
        QColor(0,255,135),
        QColor(0,255,175),
        QColor(0,255,215),
        QColor(0,255,255),
        QColor(95,0,0),
        QColor(95,0,95),
        QColor(95,0,135),
        QColor(95,0,175),
        QColor(95,0,215),
        QColor(95,0,255),
        QColor(95,95,0),
        QColor(95,95,95),
        QColor(95,95,135),
        QColor(95,95,175),
        QColor(95,95,215),
        QColor(95,95,255),
        QColor(95,135,0),
        QColor(95,135,95),
        QColor(95,135,135),
        QColor(95,135,175),
        QColor(95,135,215),
        QColor(95,135,255),
        QColor(95,175,0),
        QColor(95,175,95),
        QColor(95,175,135),
        QColor(95,175,175),
        QColor(95,175,215),
        QColor(95,175,255),
        QColor(95,215,0),
        QColor(95,215,95),
        QColor(95,215,135),
        QColor(95,215,175),
        QColor(95,215,215),
        QColor(95,215,255),
        QColor(95,255,0),
        QColor(95,255,95),
        QColor(95,255,135),
        QColor(95,255,175),
        QColor(95,255,215),
        QColor(95,255,255),
        QColor(135,0,0),
        QColor(135,0,95),
        QColor(135,0,135),
        QColor(135,0,175),
        QColor(135,0,215),
        QColor(135,0,255),
        QColor(135,95,0),
        QColor(135,95,95),
        QColor(135,95,135),
        QColor(135,95,175),
        QColor(135,95,215),
        QColor(135,95,255),
        QColor(135,135,0),
        QColor(135,135,95),
        QColor(135,135,135),
        QColor(135,135,175),
        QColor(135,135,215),
        QColor(135,135,255),
        QColor(135,175,0),
        QColor(135,175,95),
        QColor(135,175,135),
        QColor(135,175,175),
        QColor(135,175,215),
        QColor(135,175,255),
        QColor(135,215,0),
        QColor(135,215,95),
        QColor(135,215,135),
        QColor(135,215,175),
        QColor(135,215,215),
        QColor(135,215,255),
        QColor(135,255,0),
        QColor(135,255,95),
        QColor(135,255,135),
        QColor(135,255,175),
        QColor(135,255,215),
        QColor(135,255,255),
        QColor(175,0,0),
        QColor(175,0,95),
        QColor(175,0,135),
        QColor(175,0,175),
        QColor(175,0,215),
        QColor(175,0,255),
        QColor(175,95,0),
        QColor(175,95,95),
        QColor(175,95,135),
        QColor(175,95,175),
        QColor(175,95,215),
        QColor(175,95,255),
        QColor(175,135,0),
        QColor(175,135,95),
        QColor(175,135,135),
        QColor(175,135,175),
        QColor(175,135,215),
        QColor(175,135,255),
        QColor(175,175,0),
        QColor(175,175,95),
        QColor(175,175,135),
        QColor(175,175,175),
        QColor(175,175,215),
        QColor(175,175,255),
        QColor(175,215,0),
        QColor(175,215,95),
        QColor(175,215,135),
        QColor(175,215,175),
        QColor(175,215,215),
        QColor(175,215,255),
        QColor(175,255,0),
        QColor(175,255,95),
        QColor(175,255,135),
        QColor(175,255,175),
        QColor(175,255,215),
        QColor(175,255,255),
        QColor(215,0,0),
        QColor(215,0,95),
        QColor(215,0,135),
        QColor(215,0,175),
        QColor(215,0,215),
        QColor(215,0,255),
        QColor(215,95,0),
        QColor(215,95,95),
        QColor(215,95,135),
        QColor(215,95,175),
        QColor(215,95,215),
        QColor(215,95,255),
        QColor(215,135,0),
        QColor(215,135,95),
        QColor(215,135,135),
        QColor(215,135,175),
        QColor(215,135,215),
        QColor(215,135,255),
        QColor(215,175,0),
        QColor(215,175,95),
        QColor(215,175,135),
        QColor(215,175,175),
        QColor(215,175,215),
        QColor(215,175,255),
        QColor(215,215,0),
        QColor(215,215,95),
        QColor(215,215,135),
        QColor(215,215,175),
        QColor(215,215,215),
        QColor(215,215,255),
        QColor(215,255,0),
        QColor(215,255,95),
        QColor(215,255,135),
        QColor(215,255,175),
        QColor(215,255,215),
        QColor(215,255,255),
        QColor(255,0,0),
        QColor(255,0,95),
        QColor(255,0,135),
        QColor(255,0,175),
        QColor(255,0,215),
        QColor(255,0,255),
        QColor(255,95,0),
        QColor(255,95,95),
        QColor(255,95,135),
        QColor(255,95,175),
        QColor(255,95,215),
        QColor(255,95,255),
        QColor(255,135,0),
        QColor(255,135,95),
        QColor(255,135,135),
        QColor(255,135,175),
        QColor(255,135,215),
        QColor(255,135,255),
        QColor(255,175,0),
        QColor(255,175,95),
        QColor(255,175,135),
        QColor(255,175,175),
        QColor(255,175,215),
        QColor(255,175,255),
        QColor(255,215,0),
        QColor(255,215,95),
        QColor(255,215,135),
        QColor(255,215,175),
        QColor(255,215,215),
        QColor(255,215,255),
        QColor(255,255,0),
        QColor(255,255,95),
        QColor(255,255,135),
        QColor(255,255,175),
        QColor(255,255,215),
        QColor(255,255,255),
        QColor(8,8,8),
        QColor(18,18,18),
        QColor(28,28,28),
        QColor(38,38,38),
        QColor(48,48,48),
        QColor(58,58,58),
        QColor(68,68,68),
        QColor(78,78,78),
        QColor(88,88,88),
        QColor(98,98,98),
        QColor(108,108,108),
        QColor(118,118,118),
        QColor(128,128,128),
        QColor(138,138,138),
        QColor(148,148,148),
        QColor(158,158,158),
        QColor(168,168,168),
        QColor(178,178,178),
        QColor(188,188,188),
        QColor(198,198,198),
        QColor(208,208,208),
        QColor(218,218,218),
        QColor(228,228,228),
        QColor(238,238,238),
        // Default colors
        QColor(204,204,204),
        QColor(85,85,85),
        QColor(187,187,187),        // Default font color
        QColor(28,32,34)            // Default background color
    };
};

#endif // QLIGHTTERMINAL_H
