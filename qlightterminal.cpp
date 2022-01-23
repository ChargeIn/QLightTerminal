/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "qlightterminal.h"

#include <QByteArray>
#include <QTextCursor>

#if DEBUGGING
    #include <QDebug>
#endif

// Add a zero width space at the end to start the font
#define TERM_PREFIX_FONT "<font color=\"#4fb4e1\">&#8203;<b>"
#define TERM_OUTPUT_FONT "<font color=\"#144054\">&#8203;"
#define USER_FONT "</b><font color=\"#ffffff\">&#8203;"
#define CLOSE_FONT "</font>"

QLightTerminal::QLightTerminal(): QPlainTextEdit()
{
    st = new SimpleTerminal();

    // setup default style
    setMaximumBlockCount(100);
    setStyleSheet("background: #1c2022; font-size: 14px; font-weight: 500;");
    setBackgroundVisible(true);
    setFont(QFont("mono"));
    setCursorWidth(8);


    connect(st, &SimpleTerminal::s_error, this, [](QString error){ qDebug() << "Error from st: " << error;});
    connect(st, &SimpleTerminal::updateView, this, &QLightTerminal::updateTerminal);
}

QLightTerminal::~QLightTerminal(){
}

void QLightTerminal::updateTerminal(Term* term){

    QString test = QString();
    for(int i = 0; i < term->row; i++){
        for(int j = 0; j < term->col; j++){
            if(term->line[i][j].u){
                test += QChar(term->line[i][j].u);
            }
        }
    }

    setPlainText(test);
}

void QLightTerminal::deleteLastLine() {
   setFocus();
   QTextCursor storeCursorPos = this->textCursor();
   moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
   moveCursor(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
   moveCursor(QTextCursor::End, QTextCursor::KeepAnchor);
   textCursor().removeSelectedText();
   textCursor().deletePreviousChar();
   setTextCursor(storeCursorPos);
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
