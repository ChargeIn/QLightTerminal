#include "st.h"

#include <QDebug>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>

#include <QString>

#if   defined(__linux)
 #include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 #include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
 #include <libutil.h>
#endif

SimpleTerminal::SimpleTerminal(): QObject()
{
    readBufSize = sizeof(readBuf)/sizeof(readBuf[0]);

    tnew(80, 24);
    ttynew();

    readNotifier = new QSocketNotifier(master, QSocketNotifier::Read);
    readNotifier->setEnabled(true);

    connect(readNotifier,&QSocketNotifier::activated, this, &SimpleTerminal::ttyread);
}

void SimpleTerminal::tnew(int col, int row)
{
    term = (Term){ .c = { .attr = { .fg = defaultfg, .bg = defaultbg } } };
    tresize(col, row);
    treset();
}

void SimpleTerminal::closePty(){
    if(slave > 1){
        ::close(slave);
        slave = -1;
    }

    if(master > -1){
        ::close(master);
        master = -1;
    }
}

void SimpleTerminal::ttynew()
{
    master = -1;
    slave = -1;

    /* seems to work fine on linux, openbsd and freebsd */
    if (::openpty(&master, &slave, NULL, NULL, NULL) < 0){
        emit s_error("Could not open new file descriptor.");
        return;
    }

    switch (processId = fork()) {
    case -1:
        closePty();
        emit s_error("Could not fork process.");
        return;
        break;
    case 0:
        ::close(master);
        ::setsid(); /* create a new process group */
        ::dup2(slave, 0);
        ::dup2(slave, 1);
        ::dup2(slave, 2);
        if (::ioctl(slave, TIOCSCTTY, NULL) < 0){
            closePty();
            emit s_error("Error setting the controlling terminal.");
            return;
        }
        if (slave > 2)
            ::close(slave);
#ifdef __OpenBSD__
        if (::pledge("stdio getpw proc exec", NULL) == -1){
            closePty();
            emit s_error("Error on pledge.");
            return;
        }
#endif
        execsh();
        break;
    default:
#ifdef __OpenBSD__
        if (::pledge("stdio rpath tty proc", NULL) == -1){
            closePty();
            emit s_error("Error on pledge.");
            return;
        }
#endif
        ::close(slave);
        break;
    }
}

void SimpleTerminal::execsh()
{
    const struct passwd *pw;

    errno = 0;
    if ((pw = ::getpwuid(getuid())) == NULL) {
        if (errno){
            closePty();
            emit s_error("Error on getpwuid.");
            return;
        }
        else {
            closePty();
            emit s_error("Could not retrive user identity.");
            return;
        }
    }

    QByteArray shell = qgetenv("SHELL");
    if (shell.size() == 0){
        shell = (pw->pw_shell[0]) ? pw->pw_shell : "/bin/sh";
    }

    ::unsetenv("COLUMNS");
    ::unsetenv("LINES");
    ::unsetenv("TERMCAP");
    ::setenv("LOGNAME", pw->pw_name, 1);
    ::setenv("USER", pw->pw_name, 1);
    ::setenv("SHELL", shell, 1);
    ::setenv("HOME", pw->pw_dir, 1);
    // TODO figure out a way to use tic command with custom term info file
    ::setenv("TERM", "xterm-256color", 1);

    char* args[] = {shell.data(), NULL, NULL};

    ::execvp(shell.constData(), args);
}

size_t SimpleTerminal::ttyread()
{
    int ret, written;

    /* append read bytes to unprocessed bytes */
    ret = ::read(master, readBuf+readBufPos, readBufSize-readBufPos);

    switch (ret) {
    case 0:
        return 0;
    case -1:
        closePty();
        emit s_error("Could not read from shell.");
        return 0;
    default:
        readBufPos += ret;
        written = twrite(readBuf, readBufPos, 0);
        readBufPos -= written;
        /* keep any incomplete UTF-8 byte sequence for the next call */
        if (readBufPos > 0){
            ::memmove(readBuf, readBuf + written, readBufPos);
        }

        emit updateView(&term);
        return ret;
    }
}

void SimpleTerminal::tresize(int col, int row)
{
    int i;
    int minrow = MIN(row, term.row);
    int mincol = MIN(col, term.col);
    int *bp;
    TCursor c;

    if (col < 1 || row < 1) {
        fprintf(stderr,
                "tresize: error resizing to %dx%d\n", col, row);
        return;
    }

    /*
     * slide screen to keep cursor where we expect it -
     * tscrollup would work here, but we can optimize to
     * memmove because we're freeing the earlier lines
     */
    for (i = 0; i <= term.c.y - row; i++) {
        free(term.line[i]);
        free(term.alt[i]);
    }
    /* ensure that both src and dst are not NULL */
    if (i > 0) {
        memmove(term.line, term.line + i, row * sizeof(Line));
        memmove(term.alt, term.alt + i, row * sizeof(Line));
    }
    for (i += row; i < term.row; i++) {
        free(term.line[i]);
        free(term.alt[i]);
    }

    /* resize to new height */
    term.line = (Line*) realloc(term.line, row * sizeof(Line));
    term.alt  = (Line*) realloc(term.alt,  row * sizeof(Line));
    term.dirty = (int*) realloc(term.dirty, row * sizeof(*term.dirty));
    term.tabs = (int*) realloc(term.tabs, col * sizeof(*term.tabs));

    if(term.line == NULL || term.alt == NULL || term.dirty == NULL || term.tabs == NULL){
        emit s_error("Error on resize");
        return;
    }


    /* resize each row to new width, zero-pad if needed */
    for (i = 0; i < minrow; i++) {
        term.line[i] = (Glyph_*)realloc(term.line[i], col * sizeof(Glyph));
        term.alt[i]  = (Glyph_*)realloc(term.alt[i],  col * sizeof(Glyph));

        if(term.line[i] == NULL ||  term.alt[i] == NULL){
            emit s_error("Error on resize");
            return;
        }
    }

    /* allocate any new rows */
    for (/* i = minrow */; i < row; i++) {
        term.line[i] = (Glyph_*) malloc(col * sizeof(Glyph));
        term.alt[i] = (Glyph_*) malloc(col * sizeof(Glyph));

        if(term.line[i] == NULL ||  term.alt[i] == NULL){
            emit s_error("Error on resize");
            return;
        }
    }
    if (col > term.col) {
        bp = term.tabs + term.col;

        memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
        while (--bp > term.tabs && !*bp)
            /* nothing */ ;
        for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces)
            *bp = 1;
    }
    /* update terminal size */
    term.col = col;
    term.row = row;
    /* reset scrolling region */
    tsetscroll(0, row-1);
    /* make use of the LIMIT in tmoveto */
    tmoveto(term.c.x, term.c.y);
    /* Clearing both screens (it makes dirty all lines) */
    c = term.c;
    for (i = 0; i < 2; i++) {
        if (mincol < col && 0 < minrow) {
            tclearregion(mincol, 0, col - 1, minrow - 1);
        }
        if (0 < col && minrow < row) {
            tclearregion(0, minrow, col - 1, row - 1);
        }
        tswapscreen();
        tcursor(CURSOR_LOAD);
    }
    term.c = c;
}


void SimpleTerminal::tputc(Rune u)
{
    char c[UTF_SIZ];
    int control;
    int width, len;
    Glyph *gp;

    control = ISCONTROL(u);
    if (u < 127 || !IS_SET(term.mode, MODE_UTF8)) {
        c[0] = u;
        width = len = 1;
    } else {
        len = utf8encode(u, c);
        if (!control && (width = wcwidth(u)) == -1)
            width = 1;
    }

    if (IS_SET(term.mode, MODE_PRINT)){
        tprinter(c, len);
    }


    /*
     * STR sequence must be checked before anything else
     * because it uses all following characters until it
     * receives a ESC, a SUB, a ST or any other C1 control
     * character.
     */
    if (term.esc & ESC_STR) {
        if (u == '\a' || u == 030 || u == 032 || u == 033 ||
           ISCONTROLC1(u)) {
            term.esc &= ~(ESC_START|ESC_STR);
            term.esc |= ESC_STR_END;
            goto check_control_code;
        }

        if (strescseq.len+len >= strescseq.siz) {
            /*
             * Here is a bug in terminals. If the user never sends
             * some code to stop the str or esc command, then st
             * will stop responding. But this is better than
             * silently failing with unknown characters. At least
             * then users will report back.
             *
             * In the case users ever get fixed, here is the code:
             */
            /*
             * term.esc = 0;
             * strhandle();
             */
            if (strescseq.siz > (SIZE_MAX - UTF_SIZ) / 2)
                return;
            strescseq.siz *= 2;
            strescseq.buf = (char*) realloc(strescseq.buf, strescseq.siz);

            if(strescseq.buf == NULL){
                emit s_error("Could not realloc buffer.");
                return;
            }
        }

        memmove(&strescseq.buf[strescseq.len], c, len);
        strescseq.len += len;
        return;
    }

check_control_code:
    /*
     * Actions of control codes must be performed as soon they arrive
     * because they can be embedded inside a control sequence, and
     * they must not cause conflicts with sequences.
     */
    if (control) {
        tcontrolcode(u);
        /*
         * control codes are not shown ever
         */
        if (!term.esc)
            term.lastc = 0;
        return;
    } else if (term.esc & ESC_START) {
        if (term.esc & ESC_CSI) {
            csiescseq.buf[csiescseq.len++] = u;
            if (BETWEEN(u, 0x40, 0x7E)
                    || csiescseq.len >= \
                    sizeof(csiescseq.buf)-1) {
                term.esc = 0;
                csiparse();
                csihandle();
            }
            return;
        } else if (term.esc & ESC_UTF8) {
            tdefutf8(u);
        } else if (term.esc & ESC_ALTCHARSET) {
            tdeftran(u);
        } else if (term.esc & ESC_TEST) {
            tdectest(u);
        } else {
            if (!eschandle(u))
                return;
            /* sequence already finished */
        }
        term.esc = 0;
        /*
         * All characters which form part of a sequence are not
         * printed
         */
        return;
    }
    if (selected(term.c.x, term.c.y))
        selclear();

    gp = &term.line[term.c.y][term.c.x];
    if (IS_SET(term.mode, MODE_WRAP) && (term.c.state & CURSOR_WRAPNEXT)) {
        gp->mode |= ATTR_WRAP;
        tnewline(1);
        gp = &term.line[term.c.y][term.c.x];
    }

    if (IS_SET(term.mode, MODE_INSERT) && term.c.x+width < term.col)
        memmove(gp+width, gp, (term.col - term.c.x - width) * sizeof(Glyph));

    if (term.c.x+width > term.col) {
        tnewline(1);
        gp = &term.line[term.c.y][term.c.x];
    }

    tsetchar(u, &term.c.attr, term.c.x, term.c.y);
    term.lastc = u;

    if (width == 2) {
        gp->mode |= ATTR_WIDE;
        if (term.c.x+1 < term.col) {
            if (gp[1].mode == ATTR_WIDE && term.c.x+2 < term.col) {
                gp[2].u = ' ';
                gp[2].mode &= ~ATTR_WDUMMY;
            }
            gp[1].u = '\0';
            gp[1].mode = ATTR_WDUMMY;
        }
    }
    if (term.c.x+width < term.col) {
        tmoveto(term.c.x+width, term.c.y);
    } else {
        term.c.state |= CURSOR_WRAPNEXT;
    }
}

int SimpleTerminal::twrite(const char *buf, int size, int show_ctrl)
{
    int charsize;
    Rune u;
    int n;

    for (n = 0; n < size; n += charsize) {
        if (IS_SET(term.mode, MODE_UTF8)) {
            /* process a complete utf8 char */
            charsize = utf8decode(buf + n, &u, size - n);
            if (charsize == 0)
                break;
        } else {
            u = buf[n] & 0xFF;
            charsize = 1;
        }
        if (show_ctrl && ISCONTROL(u)) {
            if (u & 0x80) {
                u &= 0x7f;
                tputc('^');
                tputc('[');
            } else if (u != '\n' && u != '\r' && u != '\t') {
                u ^= 0x40;
                tputc('^');
            }
        }
        tputc(u);
    }
    return n;
}

void SimpleTerminal::ttywrite(const char *s, size_t n, int may_echo)
{
    const char *next;

    if (may_echo && IS_SET(term.mode, MODE_ECHO))
        twrite(s, n, 1);

    if (!IS_SET(term.mode, MODE_CRLF)) {
        ttywriteraw(s, n);
        return;
    }

    /* This is similar to how the kernel handles ONLCR for ttys */
    while (n > 0) {
        if (*s == '\r') {
            next = s + 1;
            ttywriteraw("\r\n", 2);
        } else {
            next = (char*) memchr(s, '\r', n);
            DEFAULT(next, s + n);
            ttywriteraw(s, next - s);
        }
        n -= next - s;
        s = next;
    }
}

void SimpleTerminal::ttywriteraw(const char *s, size_t n)
{
    fd_set wfd, rfd;
    ssize_t r;
    size_t lim = 256;

    /*
     * Remember that we are using a pty, which might be a modem line.
     * Writing too much will clog the line. That's why we are doing this
     * dance.
     * FIXME: Migrate the world to Plan 9.
     */
    while (n > 0) {
        FD_ZERO(&wfd);
        FD_ZERO(&rfd);
        FD_SET(master, &wfd);
        FD_SET(master, &rfd);

        /* Check if we can write. */
        if (pselect(master+1, &rfd, &wfd, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            emit s_error("Pselect failed in ttywriteraw");
            return;
        }
        if (FD_ISSET(master, &wfd)) {
            /*
             * Only write the bytes written by ttywrite() or the
             * default of 256. This seems to be a reasonable value
             * for a serial line. Bigger values might clog the I/O.
             */
            if ((r = write(master, s, (n < lim)? n : lim)) < 0){
                emit s_error("Error on write in ttywriteraw.");
                return;
            }
            if (r < n) {
                /*
                 * We weren't able to write out everything.
                 * This means the buffer is getting full
                 * again. Empty it.
                 */
                if (n < lim)
                    lim = ttyread();
                n -= r;
                s += r;
            } else {
                /* All bytes have been written. */
                break;
            }
        }
        if (FD_ISSET(master, &rfd))
            lim = ttyread();
    }
    return;
}


size_t SimpleTerminal::utf8decode(const char *c, Rune *u, size_t clen)
{
    size_t i, j, len, type;
    Rune udecoded;

    *u = UTF_INVALID;
    if (!clen)
        return 0;
    udecoded = utf8decodebyte(c[0], &len);
    if (!BETWEEN(len, 1, UTF_SIZ))
        return 1;
    for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
        udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;
    *u = udecoded;
    utf8validate(u, len);

    return len;
}

Rune SimpleTerminal::utf8decodebyte(char c, size_t *i)
{
    for (*i = 0; *i < LEN(utfmask); ++(*i))
        if (((uchar)c & utfmask[*i]) == utfbyte[*i])
            return (uchar)c & ~utfmask[*i];

    return 0;
}

size_t SimpleTerminal::utf8encode(Rune u, char *c)
{
    size_t len, i;

    len = utf8validate(&u, 0);
    if (len > UTF_SIZ)
        return 0;

    for (i = len - 1; i != 0; --i) {
        c[i] = utf8encodebyte(u, 0);
        u >>= 6;
    }
    c[0] = utf8encodebyte(u, len);

    return len;
}

char SimpleTerminal::utf8encodebyte(Rune u, size_t i)
{
    return utfbyte[i] | (u & ~utfmask[i]);
}


size_t SimpleTerminal::utf8validate(Rune *u, size_t i)
{
    if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
        *u = UTF_INVALID;
    for (i = 1; *u > utfmax[i]; ++i)
        ;

    return i;
}


void SimpleTerminal::tcontrolcode(uchar ascii)
{
    switch (ascii) {
    case '\t':   /* HT */
        tputtab(1);
        return;
    case '\b':   /* BS */
        tmoveto(term.c.x-1, term.c.y);
        return;
    case '\r':   /* CR */
        tmoveto(0, term.c.y);
        return;
    case '\f':   /* LF */
    case '\v':   /* VT */
    case '\n':   /* LF */
        /* go to first col if the mode is set */
        tnewline(IS_SET(term.mode, MODE_CRLF));
        return;
    case '\a':   /* BEL */
        if (term.esc & ESC_STR_END) {
            /* backwards compatibility to xterm */
            strhandle();
        } else {
            bell();
        }
        break;
    case '\033': /* ESC */
        csireset();
        term.esc &= ~(ESC_CSI|ESC_ALTCHARSET|ESC_TEST);
        term.esc |= ESC_START;
        return;
    case '\016': /* SO (LS1 -- Locking shift 1) */
    case '\017': /* SI (LS0 -- Locking shift 0) */
        term.charset = 1 - (ascii - '\016');
        return;
    case '\032': /* SUB */
        tsetchar('?', &term.c.attr, term.c.x, term.c.y);
        /* FALLTHROUGH */
    case '\030': /* CAN */
        csireset();
        break;
    case '\005': /* ENQ (IGNORED) */
    case '\000': /* NUL (IGNORED) */
    case '\021': /* XON (IGNORED) */
    case '\023': /* XOFF (IGNORED) */
    case 0177:   /* DEL (IGNORED) */
        return;
    case 0x80:   /* TODO: PAD */
    case 0x81:   /* TODO: HOP */
    case 0x82:   /* TODO: BPH */
    case 0x83:   /* TODO: NBH */
    case 0x84:   /* TODO: IND */
        break;
    case 0x85:   /* NEL -- Next line */
        tnewline(1); /* always go to first col */
        break;
    case 0x86:   /* TODO: SSA */
    case 0x87:   /* TODO: ESA */
        break;
    case 0x88:   /* HTS -- Horizontal tab stop */
        term.tabs[term.c.x] = 1;
        break;
    case 0x89:   /* TODO: HTJ */
    case 0x8a:   /* TODO: VTS */
    case 0x8b:   /* TODO: PLD */
    case 0x8c:   /* TODO: PLU */
    case 0x8d:   /* TODO: RI */
    case 0x8e:   /* TODO: SS2 */
    case 0x8f:   /* TODO: SS3 */
    case 0x91:   /* TODO: PU1 */
    case 0x92:   /* TODO: PU2 */
    case 0x93:   /* TODO: STS */
    case 0x94:   /* TODO: CCH */
    case 0x95:   /* TODO: MW */
    case 0x96:   /* TODO: SPA */
    case 0x97:   /* TODO: EPA */
    case 0x98:   /* TODO: SOS */
    case 0x99:   /* TODO: SGCI */
        break;
    case 0x9a:   /* DECID -- Identify Terminal */
        ttywrite(vtiden, strlen(vtiden), 0);
        break;
    case 0x9b:   /* TODO: CSI */
    case 0x9c:   /* TODO: ST */
        break;
    case 0x90:   /* DCS -- Device Control String */
    case 0x9d:   /* OSC -- Operating System Command */
    case 0x9e:   /* PM -- Privacy Message */
    case 0x9f:   /* APC -- Application Program Command */
        tstrsequence(ascii);
        return;
    }
    /* only CAN, SUB, \a and C1 chars interrupt a sequence */
    term.esc &= ~(ESC_STR_END|ESC_STR);
}

void SimpleTerminal::tputtab(int n)
{
    uint x = term.c.x;

    if (n > 0) {
        while (x < term.col && n--)
            for (++x; x < term.col && !term.tabs[x]; ++x)
                /* nothing */ ;
    } else if (n < 0) {
        while (x > 0 && n++)
            for (--x; x > 0 && !term.tabs[x]; --x)
                /* nothing */ ;
    }
    term.c.x = LIMIT(x, 0, term.col-1);
}

void SimpleTerminal::tmoveto(int x, int y)
{
    int miny, maxy;

    if (term.c.state & CURSOR_ORIGIN) {
        miny = term.top;
        maxy = term.bot;
    } else {
        miny = 0;
        maxy = term.row - 1;
    }
    term.c.state &= ~CURSOR_WRAPNEXT;
    term.c.x = LIMIT(x, 0, term.col-1);
    term.c.y = LIMIT(y, miny, maxy);
}

void SimpleTerminal::tstrsequence(uchar c)
{
    switch (c) {
    case 0x90:   /* DCS -- Device Control String */
        c = 'P';
        break;
    case 0x9f:   /* APC -- Application Program Command */
        c = '_';
        break;
    case 0x9e:   /* PM -- Privacy Message */
        c = '^';
        break;
    case 0x9d:   /* OSC -- Operating System Command */
        c = ']';
        break;
    }
    strreset();
    strescseq.type = c;
    term.esc |= ESC_STR;
}


void SimpleTerminal::strreset(void)
{
    char* buf = (char*) realloc(strescseq.buf, STR_BUF_SIZ);

    if(buf == NULL){
        emit s_error("Error while realloc in strreset.");
        return;
    }

    strescseq = (STREscape){
        .buf = buf,
        .siz = STR_BUF_SIZ,
    };
}

void SimpleTerminal::tnewline(int first_col)
{
    int y = term.c.y;

    if (y == term.bot) {
        tscrollup(term.top, 1);
    } else {
        y++;
    }
    tmoveto(first_col ? 0 : term.c.x, y);
}

void SimpleTerminal::tscrollup(int orig, int n)
{
    int i;
    Line temp;

    LIMIT(n, 0, term.bot-orig+1);

    tclearregion(0, orig, term.col-1, orig+n-1);
    tsetdirt(orig+n, term.bot);

    for (i = orig; i <= term.bot-n; i++) {
        temp = term.line[i];
        term.line[i] = term.line[i+n];
        term.line[i+n] = temp;
    }

    selscroll(orig, -n);
}

void SimpleTerminal::tclearregion(int x1, int y1, int x2, int y2)
{
    int x, y, temp;
    Glyph *gp;

    if (x1 > x2)
        temp = x1, x1 = x2, x2 = temp;
    if (y1 > y2)
        temp = y1, y1 = y2, y2 = temp;

    LIMIT(x1, 0, term.col-1);
    LIMIT(x2, 0, term.col-1);
    LIMIT(y1, 0, term.row-1);
    LIMIT(y2, 0, term.row-1);

    for (y = y1; y <= y2; y++) {
        term.dirty[y] = 1;
        for (x = x1; x <= x2; x++) {
            gp = &term.line[y][x];
            if (selected(x, y))
                selclear();
            gp->fg = term.c.attr.fg;
            gp->bg = term.c.attr.bg;
            gp->mode = 0;
            gp->u = ' ';
        }
    }
}

int SimpleTerminal::selected(int x, int y)
{
    if (sel.mode == SEL_EMPTY || sel.ob.x == -1 ||
            sel.alt != IS_SET(term.mode, MODE_ALTSCREEN))
        return 0;

    if (sel.type == SEL_RECTANGULAR)
        return BETWEEN(y, sel.nb.y, sel.ne.y)
            && BETWEEN(x, sel.nb.x, sel.ne.x);

    return BETWEEN(y, sel.nb.y, sel.ne.y)
        && (y != sel.nb.y || x >= sel.nb.x)
        && (y != sel.ne.y || x <= sel.ne.x);
}

void SimpleTerminal::selclear(void)
{
    if (sel.ob.x == -1)
        return;
    sel.mode = SEL_IDLE;
    sel.ob.x = -1;
    tsetdirt(sel.nb.y, sel.ne.y);
}

void SimpleTerminal::selscroll(int orig, int n)
{
    if (sel.ob.x == -1)
        return;

    if (BETWEEN(sel.nb.y, orig, term.bot) != BETWEEN(sel.ne.y, orig, term.bot)) {
        selclear();
    } else if (BETWEEN(sel.nb.y, orig, term.bot)) {
        sel.ob.y += n;
        sel.oe.y += n;
        if (sel.ob.y < term.top || sel.ob.y > term.bot ||
            sel.oe.y < term.top || sel.oe.y > term.bot) {
            selclear();
        } else {
            selnormalize();
        }
    }
}

void SimpleTerminal::selnormalize(void)
{
    int i;

    if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y) {
        sel.nb.x = sel.ob.y < sel.oe.y ? sel.ob.x : sel.oe.x;
        sel.ne.x = sel.ob.y < sel.oe.y ? sel.oe.x : sel.ob.x;
    } else {
        sel.nb.x = MIN(sel.ob.x, sel.oe.x);
        sel.ne.x = MAX(sel.ob.x, sel.oe.x);
    }
    sel.nb.y = MIN(sel.ob.y, sel.oe.y);
    sel.ne.y = MAX(sel.ob.y, sel.oe.y);

    selsnap(&sel.nb.x, &sel.nb.y, -1);
    selsnap(&sel.ne.x, &sel.ne.y, +1);

    /* expand selection over line breaks */
    if (sel.type == SEL_RECTANGULAR)
        return;
    i = tlinelen(sel.nb.y);
    if (i < sel.nb.x)
        sel.nb.x = i;
    if (tlinelen(sel.ne.y) <= sel.ne.x)
        sel.ne.x = term.col - 1;
}

void SimpleTerminal::tsetdirt(int top, int bot)
{
    int i;

    LIMIT(top, 0, term.row-1);
    LIMIT(bot, 0, term.row-1);

    for (i = top; i <= bot; i++)
        term.dirty[i] = 1;
}

void SimpleTerminal::strhandle(void)
{
    char *p = NULL, *dec;
    int j, narg, par;

    term.esc &= ~(ESC_STR_END|ESC_STR);
    strparse();
    par = (narg = strescseq.narg) ? atoi(strescseq.args[0]) : 0;

    switch (strescseq.type) {
    case ']': /* OSC -- Operating System Command */
        switch (par) {
        case 0:
            if (narg > 1) {
                xsettitle(strescseq.args[1]);
                xseticontitle(strescseq.args[1]);
            }
            return;
        case 1:
            if (narg > 1)
                xseticontitle(strescseq.args[1]);
            return;
        case 2:
            if (narg > 1)
                xsettitle(strescseq.args[1]);
            return;
        case 52:
            if (narg > 2 && allowwindowops) {
                dec = base64dec(strescseq.args[2]);
                if (dec) {
                    xsetsel(dec);
                    xclipcopy();
                } else {
                    fprintf(stderr, "erresc: invalid base64\n");
                }
            }
            return;
        case 10:
            if (narg < 2)
                break;

            p = strescseq.args[1];

            if (!strcmp(p, "?"))
                osc_color_response(defaultfg, 10);
            else if (xsetcolorname(defaultfg, p))
                fprintf(stderr, "erresc: invalid foreground color: %s\n", p);
            else
                redraw();
            return;
        case 11:
            if (narg < 2)
                break;

            p = strescseq.args[1];

            if (!strcmp(p, "?"))
                osc_color_response(defaultbg, 11);
            else if (xsetcolorname(defaultbg, p))
                fprintf(stderr, "erresc: invalid background color: %s\n", p);
            else
                redraw();
            return;
        case 12:
            if (narg < 2)
                break;

            p = strescseq.args[1];

            if (!strcmp(p, "?"))
                osc_color_response(defaultcs, 12);
            else if (xsetcolorname(defaultcs, p))
                fprintf(stderr, "erresc: invalid cursor color: %s\n", p);
            else
                redraw();
            return;
        case 4: /* color set */
            if (narg < 3)
                break;
            p = strescseq.args[2];
            /* FALLTHROUGH */
        case 104: /* color reset */
            j = (narg > 1) ? atoi(strescseq.args[1]) : -1;

            if (p && !strcmp(p, "?"))
                osc4_color_response(j);
            else if (xsetcolorname(j, p)) {
                if (par == 104 && narg <= 1)
                    return; /* color reset without parameter */
                fprintf(stderr, "erresc: invalid color j=%d, p=%s\n",
                        j, p ? p : "(null)");
            } else {
                /*
                 * TODO if defaultbg color is changed, borders
                 * are dirty
                 */
                redraw();
            }
            return;
        }
        break;
    case 'k': /* old title set compatibility */
        xsettitle(strescseq.args[0]);
        return;
    case 'P': /* DCS -- Device Control String */
    case '_': /* APC -- Application Program Command */
    case '^': /* PM -- Privacy Message */
        return;
    }

    fprintf(stderr, "erresc: unknown str ");
    strdump();
}

void SimpleTerminal::strparse(void)
{
    int c;
    char *p = strescseq.buf;

    strescseq.narg = 0;
    strescseq.buf[strescseq.len] = '\0';

    if (*p == '\0')
        return;

    while (strescseq.narg < STR_ARG_SIZ) {
        strescseq.args[strescseq.narg++] = p;
        while ((c = *p) != ';' && c != '\0')
            ++p;
        if (c == '\0')
            return;
        *p++ = '\0';
    }
}

void SimpleTerminal::strdump(void)
{
    size_t i;
    uint c;

    fprintf(stderr, "ESC%c", strescseq.type);
    for (i = 0; i < strescseq.len; i++) {
        c = strescseq.buf[i] & 0xff;
        if (c == '\0') {
            putc('\n', stderr);
            return;
        } else if (isprint(c)) {
            putc(c, stderr);
        } else if (c == '\n') {
            fprintf(stderr, "(\\n)");
        } else if (c == '\r') {
            fprintf(stderr, "(\\r)");
        } else if (c == 0x1b) {
            fprintf(stderr, "(\\e)");
        } else {
            fprintf(stderr, "(%02x)", c);
        }
    }
    fprintf(stderr, "ESC\\\n");
}

void SimpleTerminal::tsetchar(Rune u, const Glyph *attr, int x, int y)
{
    static const char *vt100_0[62] = { /* 0x41 - 0x7e */
        "↑", "↓", "→", "←", "█", "▚", "☃", /* A - G */
        0, 0, 0, 0, 0, 0, 0, 0, /* H - O */
        0, 0, 0, 0, 0, 0, 0, 0, /* P - W */
        0, 0, 0, 0, 0, 0, 0, " ", /* X - _ */
        "◆", "▒", "␉", "␌", "␍", "␊", "°", "±", /* ` - g */
        "␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", /* h - o */
        "⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", /* p - w */
        "│", "≤", "≥", "π", "≠", "£", "·", /* x - ~ */
    };

    /*
     * The table is proudly stolen from rxvt.
     */
    if (term.trantbl[term.charset] == CS_GRAPHIC0 &&
       BETWEEN(u, 0x41, 0x7e) && vt100_0[u - 0x41])
        utf8decode(vt100_0[u - 0x41], &u, UTF_SIZ);

    if (term.line[y][x].mode & ATTR_WIDE) {
        if (x+1 < term.col) {
            term.line[y][x+1].u = ' ';
            term.line[y][x+1].mode &= ~ATTR_WDUMMY;
        }
    } else if (term.line[y][x].mode & ATTR_WDUMMY) {
        term.line[y][x-1].u = ' ';
        term.line[y][x-1].mode &= ~ATTR_WIDE;
    }

    term.dirty[y] = 1;
    term.line[y][x] = *attr;
    term.line[y][x].u = u;
}

void SimpleTerminal::csireset(void)
{
    memset(&csiescseq, 0, sizeof(csiescseq));
}

void SimpleTerminal::csiparse(void)
{
    char *p = csiescseq.buf, *np;
    long int v;

    csiescseq.narg = 0;
    if (*p == '?') {
        csiescseq.priv = 1;
        p++;
    }

    csiescseq.buf[csiescseq.len] = '\0';
    while (p < csiescseq.buf+csiescseq.len) {
        np = NULL;
        v = strtol(p, &np, 10);
        if (np == p)
            v = 0;
        if (v == LONG_MAX || v == LONG_MIN)
            v = -1;
        csiescseq.arg[csiescseq.narg++] = v;
        p = np;
        if (*p != ';' || csiescseq.narg == ESC_ARG_SIZ)
            break;
        p++;
    }
    csiescseq.mode[0] = *p++;
    csiescseq.mode[1] = (p < csiescseq.buf+csiescseq.len) ? *p : '\0';
}

void SimpleTerminal::csihandle(void)
{
    char buf[40];
    int len;

    switch (csiescseq.mode[0]) {
    default:
    unknown:
        fprintf(stderr, "erresc: unknown csi ");
        csidump();
        /* die(""); */
        break;
    case '@': /* ICH -- Insert <n> blank char */
        DEFAULT(csiescseq.arg[0], 1);
        tinsertblank(csiescseq.arg[0]);
        break;
    case 'A': /* CUU -- Cursor <n> Up */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x, term.c.y-csiescseq.arg[0]);
        break;
    case 'B': /* CUD -- Cursor <n> Down */
    case 'e': /* VPR --Cursor <n> Down */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x, term.c.y+csiescseq.arg[0]);
        break;
    case 'i': /* MC -- Media Copy */
        switch (csiescseq.arg[0]) {
        case 0:
            tdump();
            break;
        case 1:
            tdumpline(term.c.y);
            break;
        case 2:
            tdumpsel();
            break;
        case 4:
            term.mode &= ~MODE_PRINT;
            break;
        case 5:
            term.mode |= MODE_PRINT;
            break;
        }
        break;
    case 'c': /* DA -- Device Attributes */
        if (csiescseq.arg[0] == 0)
            ttywrite(vtiden, strlen(vtiden), 0);
        break;
    case 'b': /* REP -- if last char is printable print it <n> more times */
        DEFAULT(csiescseq.arg[0], 1);
        if (term.lastc)
            while (csiescseq.arg[0]-- > 0)
                tputc(term.lastc);
        break;
    case 'C': /* CUF -- Cursor <n> Forward */
    case 'a': /* HPR -- Cursor <n> Forward */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x+csiescseq.arg[0], term.c.y);
        break;
    case 'D': /* CUB -- Cursor <n> Backward */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x-csiescseq.arg[0], term.c.y);
        break;
    case 'E': /* CNL -- Cursor <n> Down and first col */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(0, term.c.y+csiescseq.arg[0]);
        break;
    case 'F': /* CPL -- Cursor <n> Up and first col */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(0, term.c.y-csiescseq.arg[0]);
        break;
    case 'g': /* TBC -- Tabulation clear */
        switch (csiescseq.arg[0]) {
        case 0: /* clear current tab stop */
            term.tabs[term.c.x] = 0;
            break;
        case 3: /* clear all the tabs */
            memset(term.tabs, 0, term.col * sizeof(*term.tabs));
            break;
        default:
            goto unknown;
        }
        break;
    case 'G': /* CHA -- Move to <col> */
    case '`': /* HPA */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(csiescseq.arg[0]-1, term.c.y);
        break;
    case 'H': /* CUP -- Move to <row> <col> */
    case 'f': /* HVP */
        DEFAULT(csiescseq.arg[0], 1);
        DEFAULT(csiescseq.arg[1], 1);
        tmoveato(csiescseq.arg[1]-1, csiescseq.arg[0]-1);
        break;
    case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
        DEFAULT(csiescseq.arg[0], 1);
        tputtab(csiescseq.arg[0]);
        break;
    case 'J': /* ED -- Clear screen */
        switch (csiescseq.arg[0]) {
        case 0: /* below */
            tclearregion(term.c.x, term.c.y, term.col-1, term.c.y);
            if (term.c.y < term.row-1) {
                tclearregion(0, term.c.y+1, term.col-1,
                        term.row-1);
            }
            break;
        case 1: /* above */
            if (term.c.y > 1)
                tclearregion(0, 0, term.col-1, term.c.y-1);
            tclearregion(0, term.c.y, term.c.x, term.c.y);
            break;
        case 2: /* all */
            tclearregion(0, 0, term.col-1, term.row-1);
            break;
        default:
            goto unknown;
        }
        break;
    case 'K': /* EL -- Clear line */
        switch (csiescseq.arg[0]) {
        case 0: /* right */
            tclearregion(term.c.x, term.c.y, term.col-1,
                    term.c.y);
            break;
        case 1: /* left */
            tclearregion(0, term.c.y, term.c.x, term.c.y);
            break;
        case 2: /* all */
            tclearregion(0, term.c.y, term.col-1, term.c.y);
            break;
        }
        break;
    case 'S': /* SU -- Scroll <n> line up */
        DEFAULT(csiescseq.arg[0], 1);
        tscrollup(term.top, csiescseq.arg[0]);
        break;
    case 'T': /* SD -- Scroll <n> line down */
        DEFAULT(csiescseq.arg[0], 1);
        tscrolldown(term.top, csiescseq.arg[0]);
        break;
    case 'L': /* IL -- Insert <n> blank lines */
        DEFAULT(csiescseq.arg[0], 1);
        tinsertblankline(csiescseq.arg[0]);
        break;
    case 'l': /* RM -- Reset Mode */
        tsetmode(csiescseq.priv, 0, csiescseq.arg, csiescseq.narg);
        break;
    case 'M': /* DL -- Delete <n> lines */
        DEFAULT(csiescseq.arg[0], 1);
        tdeleteline(csiescseq.arg[0]);
        break;
    case 'X': /* ECH -- Erase <n> char */
        DEFAULT(csiescseq.arg[0], 1);
        tclearregion(term.c.x, term.c.y,
                term.c.x + csiescseq.arg[0] - 1, term.c.y);
        break;
    case 'P': /* DCH -- Delete <n> char */
        DEFAULT(csiescseq.arg[0], 1);
        tdeletechar(csiescseq.arg[0]);
        break;
    case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
        DEFAULT(csiescseq.arg[0], 1);
        tputtab(-csiescseq.arg[0]);
        break;
    case 'd': /* VPA -- Move to <row> */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveato(term.c.x, csiescseq.arg[0]-1);
        break;
    case 'h': /* SM -- Set terminal mode */
        tsetmode(csiescseq.priv, 1, csiescseq.arg, csiescseq.narg);
        break;
    case 'm': /* SGR -- Terminal attribute (color) */
        tsetattr(csiescseq.arg, csiescseq.narg);
        break;
    case 'n': /* DSR – Device Status Report (cursor position) */
        if (csiescseq.arg[0] == 6) {
            len = snprintf(buf, sizeof(buf), "\033[%i;%iR",
                    term.c.y+1, term.c.x+1);
            ttywrite(buf, len, 0);
        }
        break;
    case 'r': /* DECSTBM -- Set Scrolling Region */
        if (csiescseq.priv) {
            goto unknown;
        } else {
            DEFAULT(csiescseq.arg[0], 1);
            DEFAULT(csiescseq.arg[1], term.row);
            tsetscroll(csiescseq.arg[0]-1, csiescseq.arg[1]-1);
            tmoveato(0, 0);
        }
        break;
    case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
        tcursor(CURSOR_SAVE);
        break;
    case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
        tcursor(CURSOR_LOAD);
        break;
    case ' ':
        switch (csiescseq.mode[1]) {
        case 'q': /* DECSCUSR -- Set Cursor Style */
            if (xsetcursor(csiescseq.arg[0]))
                goto unknown;
            break;
        default:
            goto unknown;
        }
        break;
    }
}

void SimpleTerminal::tsetscroll(int t, int b)
{
    int temp;

    LIMIT(t, 0, term.row-1);
    LIMIT(b, 0, term.row-1);
    if (t > b) {
        temp = t;
        t = b;
        b = temp;
    }
    term.top = t;
    term.bot = b;
}

void SimpleTerminal::tcursor(int mode)
{
    static TCursor c[2];
    int alt = IS_SET(term.mode, MODE_ALTSCREEN);

    if (mode == CURSOR_SAVE) {
        c[alt] = term.c;
    } else if (mode == CURSOR_LOAD) {
        term.c = c[alt];
        tmoveto(c[alt].x, c[alt].y);
    }
}

void SimpleTerminal::csidump(void)
{
    size_t i;
    uint c;

    fprintf(stderr, "ESC[");
    for (i = 0; i < csiescseq.len; i++) {
        c = csiescseq.buf[i] & 0xff;
        if (isprint(c)) {
            putc(c, stderr);
        } else if (c == '\n') {
            fprintf(stderr, "(\\n)");
        } else if (c == '\r') {
            fprintf(stderr, "(\\r)");
        } else if (c == 0x1b) {
            fprintf(stderr, "(\\e)");
        } else {
            fprintf(stderr, "(%02x)", c);
        }
    }
    putc('\n', stderr);
}

void SimpleTerminal::tdumpline(int n)
{
    char buf[UTF_SIZ];
    const Glyph *bp, *end;

    bp = &term.line[n][0];
    end = &bp[MIN(tlinelen(n), term.col) - 1];
    if (bp != end || bp->u != ' ') {
        for ( ; bp <= end; ++bp)
            tprinter(buf, utf8encode(bp->u, buf));
    }
    tprinter("\n", 1);
}

int SimpleTerminal::tlinelen(int y)
{
    int i = term.col;

    if (term.line[y][i - 1].mode & ATTR_WRAP)
        return i;

    while (i > 0 && term.line[y][i - 1].u == ' ')
        --i;

    return i;
}


void SimpleTerminal::tdump(void)
{
    int i;

    for (i = 0; i < term.row; ++i)
        tdumpline(i);
}

void SimpleTerminal::tinsertblank(int n)
{
    int dst, src, size;
    Glyph *line;

    LIMIT(n, 0, term.col - term.c.x);

    dst = term.c.x + n;
    src = term.c.x;
    size = term.col - dst;
    line = term.line[term.c.y];

    memmove(&line[dst], &line[src], size * sizeof(Glyph));
    tclearregion(src, term.c.y, dst - 1, term.c.y);
}

void SimpleTerminal::tinsertblankline(int n)
{
    if (BETWEEN(term.c.y, term.top, term.bot))
        tscrolldown(term.c.y, n);    void tmoveato(int x, int y);
}

void SimpleTerminal::tscrolldown(int orig, int n)
{
    int i;
    Line temp;

    LIMIT(n, 0, term.bot-orig+1);

    tsetdirt(orig, term.bot-n);
    tclearregion(0, term.bot-n+1, term.col-1, term.bot);

    for (i = term.bot; i >= orig+n; i--) {
        temp = term.line[i];
        term.line[i] = term.line[i-n];
        term.line[i-n] = temp;
    }

    selscroll(orig, n);
}

/* for absolute user moves, when decom is set */
void SimpleTerminal::tmoveato(int x, int y)
{
    tmoveto(x, y + ((term.c.state & CURSOR_ORIGIN) ? term.top: 0));
}

void SimpleTerminal::tdeletechar(int n)
{
    int dst, src, size;
    Glyph *line;

    LIMIT(n, 0, term.col - term.c.x);

    dst = term.c.x;
    src = term.c.x + n;
    size = term.col - src;
    line = term.line[term.c.y];

    memmove(&line[dst], &line[src], size * sizeof(Glyph));
    tclearregion(term.col-n, term.c.y, term.col-1, term.c.y);
}

void SimpleTerminal::tdeftran(char ascii)
{
    static char cs[] = "0B";
    static int vcs[] = {CS_GRAPHIC0, CS_USA};
    char *p;

    if ((p = strchr(cs, ascii)) == NULL) {
        fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
    } else {
        term.trantbl[term.icharset] = vcs[p - cs];
    }
}

void SimpleTerminal::tdectest(char c)
{
    int x, y;

    if (c == '8') { /* DEC screen alignment test. */
        for (x = 0; x < term.col; ++x) {
            for (y = 0; y < term.row; ++y)
                tsetchar('E', &term.c.attr, x, y);
        }
    }
}

void SimpleTerminal::tdefutf8(char ascii)
{
    if (ascii == 'G')
        term.mode |= MODE_UTF8;
    else if (ascii == '@')
        term.mode &= ~MODE_UTF8;
}

void SimpleTerminal::tdeleteline(int n)
{
    if (BETWEEN(term.c.y, term.top, term.bot))
        tscrollup(term.c.y, n);
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int SimpleTerminal::eschandle(uchar ascii)
{
    switch (ascii) {
    case '[':
        term.esc |= ESC_CSI;
        return 0;
    case '#':
        term.esc |= ESC_TEST;
        return 0;
    case '%':
        term.esc |= ESC_UTF8;
        return 0;
    case 'P': /* DCS -- Device Control String */
    case '_': /* APC -- Application Program Command */
    case '^': /* PM -- Privacy Message */
    case ']': /* OSC -- Operating System Command */
    case 'k': /* old title set compatibility */
        tstrsequence(ascii);
        return 0;
    case 'n': /* LS2 -- Locking shift 2 */
    case 'o': /* LS3 -- Locking shift 3 */
        term.charset = 2 + (ascii - 'n');
        break;
    case '(': /* GZD4 -- set primary charset G0 */
    case ')': /* G1D4 -- set secondary charset G1 */
    case '*': /* G2D4 -- set tertiary charset G2 */
    case '+': /* G3D4 -- set quaternary charset G3 */
        term.icharset = ascii - '(';
        term.esc |= ESC_ALTCHARSET;
        return 0;
    case 'D': /* IND -- Linefeed */
        if (term.c.y == term.bot) {
            tscrollup(term.top, 1);
        } else {
            tmoveto(term.c.x, term.c.y+1);
        }
        break;
    case 'E': /* NEL -- Next line */
        tnewline(1); /* always go to first col */
        break;
    case 'H': /* HTS -- Horizontal tab stop */
        term.tabs[term.c.x] = 1;
        break;
    case 'M': /* RI -- Reverse index */
        if (term.c.y == term.top) {
            tscrolldown(term.top, 1);
        } else {
            tmoveto(term.c.x, term.c.y-1);
        }
        break;
    case 'Z': /* DECID -- Identify Terminal */
        ttywrite(vtiden, strlen(vtiden), 0);
        break;
    case 'c': /* RIS -- Reset to initial state */
        treset();
        resettitle();
        xloadcols();
        break;
    case '=': /* DECPAM -- Application keypad */
        // TODO xsetmode(1, MODE_APPKEYPAD);
        break;
    case '>': /* DECPNM -- Normal keypad */
        // TODO xsetmode(0, MODE_APPKEYPAD);
        break;
    case '7': /* DECSC -- Save Cursor */
        tcursor(CURSOR_SAVE);
        break;
    case '8': /* DECRC -- Restore Cursor */
        tcursor(CURSOR_LOAD);
        break;
    case '\\': /* ST -- String Terminator */
        if (term.esc & ESC_STR_END)
            strhandle();
        break;
    default:
        fprintf(stderr, "erresc: unknown sequence ESC 0x%02X '%c'\n",
            (uchar) ascii, isprint(ascii)? ascii:'.');
        break;
    }
    return 1;
}

void SimpleTerminal::treset(void)
{
    uint i;

    term.c = (TCursor){{
        .mode = ATTR_NULL,
        .fg = defaultfg, // see define default fg
        .bg = defaultbg // see define default bg
    }, .x = 0, .y = 0, .state = CURSOR_DEFAULT};

    memset(term.tabs, 0, term.col * sizeof(*term.tabs));
    for (i = tabspaces; i < term.col; i += tabspaces)
        term.tabs[i] = 1;
    term.top = 0;
    term.bot = term.row - 1;
    term.mode = MODE_WRAP|MODE_UTF8;
    memset(term.trantbl, CS_USA, sizeof(term.trantbl));
    term.charset = 0;

    for (i = 0; i < 2; i++) {
        tmoveto(0, 0);
        tcursor(CURSOR_SAVE);
        tclearregion(0, 0, term.col-1, term.row-1);
        tswapscreen();
    }
}

void SimpleTerminal::tswapscreen(void)
{
    Line *tmp = term.line;

    term.line = term.alt;
    term.alt = tmp;
    term.mode ^= MODE_ALTSCREEN;
    tfulldirt();
}

void SimpleTerminal::tfulldirt(void)
{
    tsetdirt(0, term.row-1);
}

void SimpleTerminal::tdumpsel(void)
{
    char *ptr;

    if ((ptr = getsel())) {
        tprinter(ptr, strlen(ptr));
        free(ptr);
    }
}

char* SimpleTerminal::getsel(void)
{
    char *str, *ptr;
    int y, bufsize, lastx, linelen;
    const Glyph *gp, *last;

    if (sel.ob.x == -1)
        return NULL;

    bufsize = (term.col+1) * (sel.ne.y-sel.nb.y+1) * UTF_SIZ;
    char* buf = (char*) malloc(bufsize);

    if(buf == NULL){
        emit s_error("Error on malloc in getsel.");
        return NULL;
    }

    ptr = str = buf;

    /* append every set & selected glyph to the selection */
    for (y = sel.nb.y; y <= sel.ne.y; y++) {
        if ((linelen = tlinelen(y)) == 0) {
            *ptr++ = '\n';
            continue;
        }

        if (sel.type == SEL_RECTANGULAR) {
            gp = &term.line[y][sel.nb.x];
            lastx = sel.ne.x;
        } else {
            gp = &term.line[y][sel.nb.y == y ? sel.nb.x : 0];
            lastx = (sel.ne.y == y) ? sel.ne.x : term.col-1;
        }
        last = &term.line[y][MIN(lastx, linelen-1)];
        while (last >= gp && last->u == ' ')
            --last;

        for ( ; gp <= last; ++gp) {
            if (gp->mode & ATTR_WDUMMY)
                continue;

            ptr += utf8encode(gp->u, ptr);
        }

        /*
         * Copy and pasting of line endings is inconsistent
         * in the inconsistent terminal and GUI world.
         * The best solution seems like to produce '\n' when
         * something is copied from st and convert '\n' to
         * '\r', when something to be pasted is received by
         * st.
         * FIXME: Fix the computer world.
         */
        if ((y < sel.ne.y || lastx >= linelen) &&
            (!(last->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
            *ptr++ = '\n';
    }
    *ptr = 0;
    return str;
}

void SimpleTerminal::osc_color_response(int index, int num)
{
    int n;
    char buf[32];
    unsigned char r, g, b;

    if (xgetcolor(index, &r, &g, &b)) {
        fprintf(stderr, "erresc: failed to fetch osc color %d\n", index);
        return;
    }

    n = snprintf(buf, sizeof buf, "\033]%d;rgb:%02x%02x/%02x%02x/%02x%02x\007",
             num, r, r, g, g, b, b);

    ttywrite(buf, n, 1);
}

void SimpleTerminal::redraw(void)
{
    tfulldirt();
    draw();
}

char * SimpleTerminal::base64dec(const char *src)
{
    size_t in_len = strlen(src);
    char *result, *dst;

    if (in_len % 4)
        in_len += 4 - (in_len % 4);

    char* buf =(char*) malloc(in_len / 4 * 3 + 1);

    if(buf == NULL){
        emit s_error("Error on malloc in base64dec");
        return NULL;
    }

    result = dst = buf;
    while (*src) {
        int a = base64_digits[(unsigned char) base64dec_getc(&src)];
        int b = base64_digits[(unsigned char) base64dec_getc(&src)];
        int c = base64_digits[(unsigned char) base64dec_getc(&src)];
        int d = base64_digits[(unsigned char) base64dec_getc(&src)];

        /* invalid input. 'a' can be -1, e.g. if src is "\n" (c-str) */
        if (a == -1 || b == -1)
            break;

        *dst++ = (a << 2) | ((b & 0x30) >> 4);
        if (c == -1)
            break;
        *dst++ = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
        if (d == -1)
            break;
        *dst++ = ((c & 0x03) << 6) | d;
    }
    *dst = '\0';
    return result;
}

char SimpleTerminal::base64dec_getc(const char **src)
{
    while (**src && !isprint(**src))
        (*src)++;
    return **src ? *((*src)++) : '=';  /* emulate padding if string ends */
}


void SimpleTerminal::osc4_color_response(int num)
{
    int n;
    char buf[32];
    unsigned char r, g, b;

    if (xgetcolor(num, &r, &g, &b)) {
        emit s_error("erresc: failed to fetch osc4 color: " + QString::number(num));
        return;
    }

    n = snprintf(buf, sizeof buf, "\033]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\007",
             num, r, r, g, g, b, b);

    ttywrite(buf, n, 1);
}

void SimpleTerminal::selsnap(int *x, int *y, int direction)
{
    int newx, newy, xt, yt;
    int delim, prevdelim;
    const Glyph *gp, *prevgp;

    switch (sel.snap) {
    case SNAP_WORD:
        /*
         * Snap around if the word wraps around at the end or
         * beginning of a line.
         */
        prevgp = &term.line[*y][*x];
        prevdelim = ISDELIM(prevgp->u);
        for (;;) {
            newx = *x + direction;
            newy = *y;
            if (!BETWEEN(newx, 0, term.col - 1)) {
                newy += direction;
                newx = (newx + term.col) % term.col;
                if (!BETWEEN(newy, 0, term.row - 1))
                    break;

                if (direction > 0)
                    yt = *y, xt = *x;
                else
                    yt = newy, xt = newx;
                if (!(term.line[yt][xt].mode & ATTR_WRAP))
                    break;
            }

            if (newx >= tlinelen(newy))
                break;

            gp = &term.line[newy][newx];
            delim = ISDELIM(gp->u);
            if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim
                    || (delim && gp->u != prevgp->u)))
                break;

            *x = newx;
            *y = newy;
            prevgp = gp;
            prevdelim = delim;
        }
        break;
    case SNAP_LINE:
        /*
         * Snap around if the the previous line or the current one
         * has set ATTR_WRAP at its end. Then the whole next or
         * previous line will be selected.
         */
        *x = (direction < 0) ? 0 : term.col - 1;
        if (direction < 0) {
            for (; *y > 0; *y += direction) {
                if (!(term.line[*y-1][term.col-1].mode
                        & ATTR_WRAP)) {
                    break;
                }
            }
        } else if (direction > 0) {
            for (; *y < term.row-1; *y += direction) {
                if (!(term.line[*y][term.col-1].mode
                        & ATTR_WRAP)) {
                    break;
                }
            }
        }
        break;
    }
}

void SimpleTerminal::tsetattr(const int *attr, int l)
{
    int i;
    int32_t idx;

    for (i = 0; i < l; i++) {
        switch (attr[i]) {
        case 0:
            term.c.attr.mode &= ~(
                ATTR_BOLD       |
                ATTR_FAINT      |
                ATTR_ITALIC     |
                ATTR_UNDERLINE  |
                ATTR_BLINK      |
                ATTR_REVERSE    |
                ATTR_INVISIBLE  |
                ATTR_STRUCK     );
            term.c.attr.fg = defaultfg;
            term.c.attr.bg = defaultbg;
            break;
        case 1:
            term.c.attr.mode |= ATTR_BOLD;
            break;
        case 2:
            term.c.attr.mode |= ATTR_FAINT;
            break;
        case 3:
            term.c.attr.mode |= ATTR_ITALIC;
            break;
        case 4:
            term.c.attr.mode |= ATTR_UNDERLINE;
            break;
        case 5: /* slow blink */
            /* FALLTHROUGH */
        case 6: /* rapid blink */
            term.c.attr.mode |= ATTR_BLINK;
            break;
        case 7:
            term.c.attr.mode |= ATTR_REVERSE;
            break;
        case 8:
            term.c.attr.mode |= ATTR_INVISIBLE;
            break;
        case 9:
            term.c.attr.mode |= ATTR_STRUCK;
            break;
        case 22:
            term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
            break;
        case 23:
            term.c.attr.mode &= ~ATTR_ITALIC;
            break;
        case 24:
            term.c.attr.mode &= ~ATTR_UNDERLINE;
            break;
        case 25:
            term.c.attr.mode &= ~ATTR_BLINK;
            break;
        case 27:
            term.c.attr.mode &= ~ATTR_REVERSE;
            break;
        case 28:
            term.c.attr.mode &= ~ATTR_INVISIBLE;
            break;
        case 29:
            term.c.attr.mode &= ~ATTR_STRUCK;
            break;
        case 38:
            if ((idx = tdefcolor(attr, &i, l)) >= 0)
                term.c.attr.fg = idx;
            break;
        case 39:
            term.c.attr.fg = defaultfg;
            break;
        case 48:
            if ((idx = tdefcolor(attr, &i, l)) >= 0)
                term.c.attr.bg = idx;
            break;
        case 49:
            term.c.attr.bg = defaultbg;
            break;
        default:
            if (BETWEEN(attr[i], 30, 37)) {
                term.c.attr.fg = attr[i] - 30;
            } else if (BETWEEN(attr[i], 40, 47)) {
                term.c.attr.bg = attr[i] - 40;
            } else if (BETWEEN(attr[i], 90, 97)) {
                term.c.attr.fg = attr[i] - 90 + 8;
            } else if (BETWEEN(attr[i], 100, 107)) {
                term.c.attr.bg = attr[i] - 100 + 8;
            } else {
                emit s_error("erresc(default): gfx attr " + QString::number(attr[i]) + " unknown.");
                csidump();
            }
            break;
        }
    }
}

int32_t SimpleTerminal::tdefcolor(const int *attr, int *npar, int l)
{
    int32_t idx = -1;
    uint r, g, b;

    switch (attr[*npar + 1]) {
    case 2: /* direct color in RGB space */
        if (*npar + 4 >= l) {
            emit s_error("erresc(38): Incorrect number of parameters " + QString::number(*npar));
            break;
        }
        r = attr[*npar + 2];
        g = attr[*npar + 3];
        b = attr[*npar + 4];
        *npar += 4;
        if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) || !BETWEEN(b, 0, 255)) {
            emit s_error("erresc: bad rgb color " + QString::number(r) + "," + QString::number(g) + "," + QString::number(b));
        } else {

            idx = TRUECOLOR(r, g, b);
        }
        break;
    case 5: /* indexed color */
        if (*npar + 2 >= l) {
            emit s_error("erresc(38): Incorrect number of parameters " + QString::number(*npar));
            break;
        }
        *npar += 2;
        if (!BETWEEN(attr[*npar], 0, 255))
            emit s_error("erresc: bad fgcolor " + QString::number(attr[*npar]));
        else
            idx = attr[*npar];
        break;
    case 0: /* implemented defined (only foreground) */
    case 1: /* transparent */
    case 3: /* direct color in CMY space */
    case 4: /* direct color in CMYK space */
    default:
        emit s_error("erresc(38): gfx attr " + QString::number(attr[*npar]) + " unknown\n");
        break;
    }

    return idx;
}



// ------------------------------ TODO ------------------------
void SimpleTerminal::xsetsel(char *strvtiden)
{
    // TODO
    qDebug() << "xsetsel";
}

void SimpleTerminal::xclipcopy(void)
{
    // TODO
    qDebug() << "xclipcopy";
}

void SimpleTerminal::xseticontitle(char *p)
{
    // TODO
    qDebug() << "xseticontitle: " << p;
}

void SimpleTerminal::draw(void)
{
    // TODO
    qDebug() << "Draw";
}


int SimpleTerminal::xsetcolorname(int x, const char *name)
{
    // TODO
    qDebug() << "xsetcolorname";
    return 0;
}

int SimpleTerminal::xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b)
{
    // TODO
    qDebug() << "xgetcolor";
    return 0;
}


void SimpleTerminal::xloadcols()
{
    // TODO
    qDebug() << "xloadcols";
}

void SimpleTerminal::xsettitle(char *p){
    // TODO
    qDebug() << "xsettitle:"  << p ;
}

void SimpleTerminal::resettitle(void)
{
    // TODO
    qDebug() << "resettitle";
}

void SimpleTerminal::xsetmode(int set, unsigned int flags)
{
    // TODO
    qDebug() << "xsetmode";
}


int SimpleTerminal::xsetcursor(int cursor)
{
    // TODO
    qDebug() << "xsetcursor";
}

void SimpleTerminal::tsetmode(int priv, int set, const int *args, int narg)
{
    // TODO
    // add mode support
    qDebug() << "tsetmode";
}


void SimpleTerminal::tprinter(char*s, size_t len){
    // TODO
    qDebug() << "tprinter";
}

void SimpleTerminal::bell(){
    // TODO
    qDebug() << "bell";
}
