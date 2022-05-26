# QLightTerminal

[![Current Version](https://img.shields.io/badge/version-1.1.1-green.svg)](https://github.com/ChargeIn/QLightTerm)

A simple lightweight terminal widget for Qt6.

![Terminal Preview](https://github.com/ChargeIn/QLightTerm/blob/master/example/demo.png)

## Usage

Download the latest QLightTerminal-Pri source and include all files in a sub folder of your project. The widget can be
included into your project by adding following line to your .pro file.

```
include(<path-to-subdir>/QLightTerminal/QLightTerminal.pri);
```

Afterwards it can be used as followed:

```
File: main.cpp

#include "libs/QLightTerminal/qlightterminal.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QLightTerminal* terminal = new QLightTerminal();
    
    // optional style options
    terminal->setFontSize(10, 500);
    terminal->setBackground(QColor(24, 24, 24));
    terminal->setPadding(8,0);
    terminal-> setLineHeightScale(1.25);
    
    setCentralWidget(terminal);
}
```

## Testing

&#9989; Linux Ubuntu (Ubuntu, ZorinOS)\
&#9989; Arch (Manjaro)\
❓ Windows

---

## License

> You can check out the full license [here](https://github.com/ChargeIn/QLightTerm/blob/master/LICENSE)

As stated by the Qt open source premise this project is license under *LGPL* v3. Since some parts of the project are
based on the [Suckless Simple Terminal](https://st.suckless.org/)
the following files fall under the *Mit*-License: *st.cpp*, *st.h*,*st-utils.h*.
