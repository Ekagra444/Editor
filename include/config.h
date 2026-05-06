#ifndef CONFIG_H
#define CONFIG_H

// ---- Layout ----
#define SIDEBAR_WIDTH    250
#define TAB_BAR_HEIGHT    30
#define TAB_PADDING       20
#define EDITOR_X         260   // sidebar_width + 10
#define EDITOR_Y_OFFSET   40
#define STATUS_BAR_PADDING  5

// ---- Window defaults ----
#define WINDOW_W  800
#define WINDOW_H  600
#define WINDOW_TITLE "Ekagra's Editor"

// ---- Font ----
#define FONT_PATH "/Users/ekagraagrawal/Library/Fonts/FiraCode-Regular.ttf"
#define FONT_SIZE  24

// ---- Frame timing ----
#define FRAME_DELAY_MS 16

// ---- Syntax highlight keywords ----
static const char *KEYWORDS[] = {
    "int", "return", "if", "else", "while", "for",
    "void", "char", "float", "double"
};
static const int KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

#endif // CONFIG_H
