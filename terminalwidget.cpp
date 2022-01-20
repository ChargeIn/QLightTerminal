/*
 *  Copyright© Florian Plesker <florian.plesker@web.de>
 */

#include "terminalwidget.h"

#include <QChar>
#include <QTextBlock>
#include <QRegularExpression>
#include <QStringList>
#include <QFont>
#include <QTextCursor>

#if DEBUGGING
    #include <QDebug>
#endif

// Add a zero width space at the end to start the font
#define TERM_PREFIX_FONT "<font color=\"#4fb4e1\">&#8203;<b>"
#define TERM_OUTPUT_FONT "<font color=\"#144054\">&#8203;"
#define USER_FONT "</b><font color=\"#ffffff\">&#8203;"
#define CLOSE_FONT "</font>"

TerminalWidget::TerminalWidget(): QPlainTextEdit(), lastline{""}
{
    _shell = new Shell();
    outputState = OutputState::TerminalOutput;
    _doc = document();

    // setup default style
    setMaximumBlockCount(100);
    setStyleSheet("background: #1c2022; font-size: 14px; font-weight: 500;");
    setBackgroundVisible(true);
    setFont(QFont("mono"));
    setCursorWidth(8);

    inputEcho = false;

    connect(_shell, &Shell::s_error, this, [](QString error){ qDebug() << error;});
    connect(_shell, &Shell::s_standardOutput, this, &TerminalWidget::processShellOutput);
}

TerminalWidget::~TerminalWidget(){
    delete _shell;
}


void TerminalWidget::processShellOutput(QString output){

#if DEBUGGING
    qDebug() << "Process shell output: " << output;
#endif

    output = output.replace("\r\n", "\n");

    if(outputState == OutputState::TerminalOutput){
        writeTerminalOutput(output);
    } else {
        writeUserInput(output);
    }
}

/**
 * State: TerminalOutput
 *
 * Write data to the terminal and check if the lastlast line
 * ends with the terminal prefix
 */
void TerminalWidget::writeTerminalOutput(QString output){
    qDebug() << "Temrinal Output" << output;

    int endOfLine = output.lastIndexOf('\n');
    QString lastOutLine;

    textCursor().insertText(output.left(endOfLine+1));

    int size = output.size();
    int sizeLeft = size - endOfLine - 1;

    if(sizeLeft == 0 ){
        return;
    }

    lastOutLine = output.right(sizeLeft);
    lastline += lastOutLine;

    if(lastline.size() < 2){
        return;
    }

    if(lastline.last(2) == "$ "){
        deleteLastLine();
        textCursor().insertHtml(CLOSE_FONT + (TERM_PREFIX_FONT + lastline.toHtmlEscaped()) + USER_FONT);
        outputState = OutputState::UserInput;
    } else {
        textCursor().insertText(lastOutLine);
    }
}

/**
 * State: UserInput
 * Write to the terminal till a line break appreas and chang into terminal state
 */
void TerminalWidget::writeUserInput(QString input){

    int size = input.size();

    for(int i = 0; i < size; i++){
        switch ((input)[i].toLatin1())
        {
          case '\b'      : textCursor().deletePreviousChar();                       break; // backspace;
          case '\t'      : /* TODO */                                               break; // tab;
          case '\r'      : textCursor().movePosition(QTextCursor::StartOfLine);     break; // to the start of the line
          case 0x07      : /* TODO */                                               break; // bell;
          case '\n'      :
            textCursor().insertText("\n");
            textCursor().insertHtml(CLOSE_FONT);
            textCursor().insertHtml(TERM_OUTPUT_FONT);
            lastline = "";
            outputState = OutputState::TerminalOutput;

            if(i < size -1){
                writeTerminalOutput(input.right(size-i+1));
            }
            return;
          default        : textCursor().insertText(input[i]);                 break;
        };
    }

}


void TerminalWidget::deleteLastLine() {
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
void TerminalWidget::keyPressEvent(QKeyEvent *e){

    /**
     * Only accept if the terminal is ready
     */
    if(outputState != OutputState::UserInput){
        return;
    }

    QString input = e->text();

    if(input.contains('\n')){
        _shell->write(input.left(input.indexOf('\n')+1).toUtf8());
    } else {
        _shell->write(e->text().toUtf8());
    }
}
