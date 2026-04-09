/************************************************************************************************************
*                                                                                                           *
*  File:       swear_machine.c                                                                              *
*  Purpose:    The swear machine challenge.                                                                 *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   SWEAR_WIN_HEIGHT                                                                             *
*  Purpose:    Height of each swear popup window.                                                           *
*                                                                                                           *
************************************************************************************************************/
#define SWEAR_WIN_HEIGHT (5)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   SWEAR_WIN_WIDTH                                                                              *
*  Purpose:    Width of each swear popup window.                                                            *
*                                                                                                           *
************************************************************************************************************/
#define SWEAR_WIN_WIDTH (40)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_WINDOWS                                                                                  *
*  Purpose:    Maximum number of swear popup windows allowed before quitting.                               *
*                                                                                                           *
************************************************************************************************************/
#define MAX_WINDOWS (20)

/************************************************************************************************************
*                                                                                                           *
*  Array:      g_swears                                                                                     *
*  Purpose:    Built-in list of swear words/phrases to display.                                             *
*                                                                                                           *
************************************************************************************************************/
static
const char*
g_swears[] =
{
    "Your exploit works... as long as nothing changes.",
    "I like how your payload is very... theoretical.",
    "That\'s a really creative solution to the wrong problem.",
    "Your shellcode is so clean - it does absolutely nothing.",
    "I think your bug is actually a feature... for defenders.",
    "You didn\'t fail the challenge, you just explored all the incorrect paths first.",
    "Your brute force script has great personality.",
    "That\'s not a race condition, that\'s a lifestyle choice.",
    "I admire your commitment to undefined behavior.",
    "Your reverse engineering approach is very... forward thinking."
};

/************************************************************************************************************
*                                                                                                           *
*  Constant:   SWEAR_COUNT                                                                                  *
*  Purpose:    Number of swears in g_swears.                                                                *
*                                                                                                           *
************************************************************************************************************/
#define SWEAR_COUNT ((int)(sizeof(g_swears) / sizeof(g_swears[0])))

/************************************************************************************************************
*                                                                                                           *
*  Constant:   DEFAULT_ROWS                                                                                 *
*  Purpose:    The default number of rows in a terminal.                                                    *
*                                                                                                           *
************************************************************************************************************/
#define DEFAULT_ROWS (24)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   DEFAULT_COLS                                                                                 *
*  Purpose:    The default number of columns in a terminal.                                                 *
*                                                                                                           *
************************************************************************************************************/
#define DEFAULT_COLS (80)

/************************************************************************************************************
*                                                                                                           *
*  Function:   term_size                                                                                    *
*  Purpose:    Returns the current terminal dimensions.                                                     *
*  Parameters: - rows - receives the number of rows.                                                        *
*              - cols - receives the number of columns.                                                     *
*                                                                                                           *
************************************************************************************************************/
static
void
term_size(
    int* rows,
    int* cols
)
{
    struct winsize ws = { 0 };

    // Fetch via IOCTL or get the default size
    if (0 == ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
    {
        *rows = (int)ws.ws_row;
        *cols = (int)ws.ws_col;
    }
    else
    {
        *rows = DEFAULT_ROWS;
        *cols = DEFAULT_COLS;
    }
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   term_raw                                                                                     *
*  Purpose:    Puts the terminal into raw mode, saving the prior settings.                                  *
*  Parameters: - saved - receives the prior terminal settings.                                              *
*                                                                                                           *
************************************************************************************************************/
static
void
term_raw(
    struct termios* saved
)
{
    struct termios raw = { 0 };

    tcgetattr(STDIN_FILENO, saved);
    raw = *saved;
    raw.c_lflag &= ~ (unsigned long)(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   term_restore                                                                                 *
*  Purpose:    Restores terminal settings previously saved by term_raw.                                     *
*  Parameters: - saved - the terminal settings to restore.                                                  *
*                                                                                                           *
************************************************************************************************************/
static
void
term_restore(
    const struct termios* saved
)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, saved);
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   draw_swear_window                                                                            *
*  Purpose:    Draws a popup window with a swear at the given terminal position.                            *
*  Parameters: - swear     - the swear string to display.                                                   *
*              - start_row - top row of the window (1-based).                                               *
*              - start_col - left column of the window (1-based).                                           *
*                                                                                                           *
************************************************************************************************************/
static
void
draw_swear_window(
    const char* swear,
    int         start_row,
    int         start_col
)
{
    int c = 0;
    int row = 0;
    int len = 0;
    int fit = 0;
    int brk = 0;
    const char* p = NULL;
    const int inner_w = SWEAR_WIN_WIDTH - 2;
    const int inner_rows = SWEAR_WIN_HEIGHT - 2;

    // Top border
    printf("\033[%d;%dH\u250c", start_row, start_col);
    for (c = 0; c < inner_w; c++)
    {
        printf("\u2500");
    }
    printf("\u2510");

    // Inner rows: side borders with blank fill
    for (row = 1; row < SWEAR_WIN_HEIGHT - 1; row++)
    {
        printf("\033[%d;%dH\u2502%-*s\u2502", start_row + row, start_col, inner_w, "");
    }

    // Word-wrap swear text into inner rows
    p = swear;
    for (row = 1; (row <= inner_rows) && ('\0' != *p); row++)
    {
        len = (int)strlen(p);
        fit = (len < inner_w) ? len : inner_w;
        if (len > inner_w)
        {
            brk = fit;
            while ((0 < brk) && (' ' != p[brk]))
            {
                brk--;
            }
            if (0 < brk)
            {
                fit = brk;
            }
        }
        printf("\033[%d;%dH%.*s", start_row + row, start_col + 1, fit, p);
        p += fit;
        if (' ' == *p)
        {
            p++;
        }
    }

    // Bottom border
    printf("\033[%d;%dH\u2514", start_row + SWEAR_WIN_HEIGHT - 1, start_col);
    for (c = 0; c < inner_w; c++)
    {
        printf("\u2500");
    }
    printf("\u2518");
    fflush(stdout);
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The swear machine binary.                                                      *
*  Returns:    Zero upon success, non-zero upon failure..                                                   *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    bool ok = false;
    char* reset_args[] = { "/usr/bin/tput", "reset", NULL };
    pid_t pid = -1;
    struct termios saved_term = { 0 };
    int rows = 0;
    int cols = 0;
    int window_count = 0;
    unsigned char ch = 0;
    int start_row = 0;
    int start_col = 0;

    // Reset terminal -- fork so the child inherits euid and tput runs elevated,
    // allowing terminfo's reset_file to be opened with full privileges.
    pid = fork();
    if (0 > pid)
    {
        fprintf(stderr, "Forking failed\n");
        goto cleanup;
    }
    if (0 == pid)
    {
        execv("/usr/bin/tput", reset_args);
        fprintf(stderr, "Terminal reset failed\n");
        goto cleanup;
    }
    (void)waitpid(pid, NULL, 0);
    
    // Seed RNG
    srand((unsigned int)time(NULL));

    // Enter raw mode, hide cursor, clear screen, draw instructions
    term_raw(&saved_term);
    printf("\033[?25l\033[2J\033[H");
    printf("\033[1;1HPress ENTER for a new swear. Press ESC to quit.");
    fflush(stdout);

    // Main event loop
    for (;;)
    {
        if (1 != read(STDIN_FILENO, &ch, 1))
        {
            break;
        }

        // ESC - exit
        if (27 == ch)
        {
            break;
        }

        // ENTER - draw a new swear window
        if (('\n' == ch) || ('\r' == ch))
        {
            if (window_count >= MAX_WINDOWS)
            {
                break;
            }
            term_size(&rows, &cols);
            if ((rows > SWEAR_WIN_HEIGHT + 2) && (cols > SWEAR_WIN_WIDTH))
            {
                start_row = 2 + rand() % (rows - SWEAR_WIN_HEIGHT - 1);
                start_col = 1 + rand() % (cols - SWEAR_WIN_WIDTH);
                draw_swear_window(g_swears[rand() % SWEAR_COUNT], start_row, start_col);
                window_count++;
            }
        }
    }

    // Restore terminal
    term_restore(&saved_term);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    // Success
    ok = true;

cleanup:

    // Finish
    return ok ? 0 : -1;
}
