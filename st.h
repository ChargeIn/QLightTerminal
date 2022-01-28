#ifndef ST_H
#define ST_H

#include <QObject>
#include <QString>
#include <QSocketNotifier>

#include "st-utils.h"

class SimpleTerminal: public QObject
{
    Q_OBJECT
public:
    Term term;

    SimpleTerminal();

    void tnew(int col, int row);

    void ttynew();

    void execsh();

    void closePty();

    void tresize(int col, int row);

    int twrite(const char* buf, int size, int show_ctrl);

    void ttywrite(const char *s, size_t n, int may_echo);

    void ttywriteraw(const char *s, size_t n);

public slots:
    size_t ttyread();

signals:
    void s_error(QString);

    void updateView(Term* state);

private:
    int master, slave;
    pid_t processId;

    char readBuf[BUFSIZ];
    int readBufPos = 0;
    int readBufSize = 0;

    QSocketNotifier* readNotifier;
    Selection sel;
    CSIEscape csiescseq;
    STREscape strescseq;

    /*
     * Default colors (colorname index)
     * foreground, background, cursor, reverse cursor
     */
    unsigned int defaultfg = 258;
    unsigned int defaultbg = 259;
    unsigned int defaultcs = 256;
    unsigned int defaultrcs = 257;

    const int tabspaces = 8;

    /* allow certain non-interactive (insecure) window operations such as:
       setting the clipboard text */
    int allowwindowops = 0;

    char* vtiden = "\033[?6c";

    const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
    const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
    const Rune utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
    const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

    size_t utf8decode(const char *c, Rune *u, size_t clen);
    Rune utf8decodebyte(char c, size_t *i);
    size_t utf8validate(Rune *u, size_t i);
    size_t utf8encode(Rune u, char *c);
    char utf8encodebyte(Rune u, size_t i);
    void tputc(Rune u);
    void tcontrolcode(uchar ascii);
    void tputtab(int n);
    void tmoveto(int x, int y);
    void tstrsequence(uchar c);
    void strreset(void);
    void tclearregion(int x1, int y1, int x2, int y2);
    int  selected(int x, int y);
    void selclear(void);
    void selscroll(int orig, int n);
    void selnormalize(void);
    void strhandle(void);
    void strparse(void);
    void strdump(void);
    void tsetchar(Rune u, const Glyph *attr, int x, int y);
    void csireset(void);
    void csiparse(void);
    void csihandle(void);
    void csidump(void);
    void tsetdirt(int top, int bot);
    void tdump(void);
    void tdumpline(int n);
    int  tlinelen(int y);
    void tnewline(int first_col);
    void tinsertblank(int n);
    void tinsertblankline(int n);
    void tscrollup(int orig, int n);
    void tscrolldown(int orig, int n);
    void tsetscroll(int t, int b);
    void tcursor(int mode);
    void tmoveato(int x, int y);
    void tdeletechar(int n);
    void tdeftran(char ascii);
    void tdectest(char c);
    void tdefutf8(char ascii);
    void tdeleteline(int n);
    int  eschandle(uchar ascii);
    void treset(void);
    void tfulldirt(void);
    void tdumpsel(void);
    char* getsel(void);
    void osc_color_response(int index, int num);
    void redraw(void);
    void draw(void);
    char* base64dec(const char *src);
    char base64dec_getc(const char **src);
    void osc4_color_response(int num);
    void selsnap(int *x, int *y, int direction);
    void tsetattr(const int *attr, int l);
    int32_t tdefcolor(const int *attr, int *npar, int l);

    // TODO
    void xsetsel(char *str);
    void xclipcopy(void);
    void xseticontitle(char *p);
    int xsetcolorname(int x, const char *name);
    int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b);
    void xsettitle(char *p);
    void xloadcols(void);
    void tswapscreen(void);
    void resettitle(void);
    void xsetmode(int set, unsigned int flags);
    int xsetcursor(int cursor);

    void bell(void); // TODO
    void tprinter(char*s, size_t len); // TODO
    void tsetmode(int priv, int set, const int *args, int narg); // TODO
};

#endif // ST_H
