#ifndef COLORS_H
#define COLORS_H

#endif // COLORS_H

/* Terminal colors (16 first used in escape sequence) */
/*
 * Florian Plesker: Had to replace some color due to QT,
 * the orignal names are in the comments.
 */
static const char *colorname[] = {
    /* 8 normal colors */
    "black",
    "#cc0000",  //"red3",
    "#4e9a06",  //"green3",
    "#c4a000",   //"yellow3",
    "#729fcf",   //"blue2",
    "#75507b",   //"magenta3",
    "#06989a",   //"cyan3",
    "#E5E5E5",     //"gray90",

    /* 8 bright colors */
    "#555753",  //"gray50",
    "#ef2929",  //"red",
    "#8ae234",  //"green",
    "#fce94f",  //"yellow",
    "#32afff",  //"#5c5cff",
    "#ad7fa8",  //"magenta",
    "#34e2e2",  //"cyan",
    "white",
};

static const char* customcolors[] = {
    /* more colors can be added after 255 to use with DefaultXX */
    "#cccccc",
    "#555555",
    "#E5E5E5",//"gray90", /* default foreground colour */
    "black", /* default background colour */
};
