/************************************************************************************************************
*                                                                                                           *
*  File:       swear_machine.c                                                                              *
*  Purpose:    The swear machine challenge.                                                                 *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
#include <ncurses.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_PATH                                                                                    *
*  Purpose:    Path to the flag file.                                                                       *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_PATH ("/Users/ctf/flag")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_MAX_LEN                                                                                 *
*  Purpose:    Maximum length of the flag buffer.                                                           *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_MAX_LEN (256)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_BASE_ADDR                                                                               *
*  Purpose:    Fixed virtual base address at which the flag buffer is mapped.                               *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_BASE_ADDR ((void*)0x200000000ULL)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_MAP_LEN                                                                                 *
*  Purpose:    Total size of the flag mapping: slots times bytes.                                           *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_MAP_LEN (256 * 256)

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
static const char* g_swears[] =
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
*  Function:   draw_swear_window                                                                            *
*  Purpose:    Draws a new ncurses popup window with a random swear.                                        *
*  Parameters: - swear - the swear string to display.                                                       *
*  Returns:    A pointer to the new window, or NULL on failure.                                             *
*                                                                                                           *
************************************************************************************************************/
static
WINDOW*
draw_swear_window(
    const char* swear
)
{
    int max_y = 0;
    int max_x = 0;
    int start_y = 0;
    int start_x = 0;
    WINDOW* win = NULL;

    // Pick a random position that fits on screen
    getmaxyx(stdscr, max_y, max_x);
    start_y = rand() % (max_y - SWEAR_WIN_HEIGHT);
    start_x = rand() % (max_x - SWEAR_WIN_WIDTH);

    // Create and draw the window
    win = newwin(SWEAR_WIN_HEIGHT, SWEAR_WIN_WIDTH, start_y, start_x);
    if (NULL == win)
    {
        goto cleanup;
    }

    // Draw
    box(win, 0, 0);

    // Word-wrap the swear across interior rows (rows 1 .. SWEAR_WIN_HEIGHT-2)
    {
        const int inner_w = SWEAR_WIN_WIDTH - 2;
        const int inner_rows = SWEAR_WIN_HEIGHT - 2;
        const char* p = swear;
        int row = 1;

        while (*p != '\0' && row <= inner_rows)
        {
            // Find how many characters fit on this row
            int len = (int)strlen(p);
            int fit = len < inner_w ? len : inner_w;

            // If text doesn't fit fully, break at last space
            if (len > inner_w)
            {
                int brk = fit;
                while (brk > 0 && p[brk] != ' ')
                {
                    brk--;
                }
                if (brk > 0)
                {
                    fit = brk;
                }
            }

            mvwprintw(win, row, 1, "%.*s", fit, p);
            p += fit;

            // Skip a single space at word-break boundary
            if (*p == ' ')
            {
                p++;
            }

            row++;
        }
    }

    wrefresh(win);

cleanup:

    // Return the window
    return win;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The swear machine binary.                                                      *
*  Returns:    Non-zero in case of failure, zero in case of success.                                        *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    bool ok = false;
    int flag_fd = -1;
    void* flag_map = MAP_FAILED;
    char* flag = NULL;
    uint8_t rand_offset = 0;
    WINDOW* windows[MAX_WINDOWS] = { NULL };
    int window_count = 0;
    int ch = 0;
    const char* swear = NULL;
    size_t counter = 0;

    // Map the entire flag arena at a fixed known base address (256 slots * 256 bytes)
    flag_map = mmap(FLAG_BASE_ADDR, FLAG_MAP_LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if ((MAP_FAILED == flag_map) || (flag_map != FLAG_BASE_ADDR))
    {
        fprintf(stderr, "swear_machine: mmap failed: %d\n", errno);
        goto cleanup;
    }

    // Pick a random slot within the arena
    arc4random_buf(&rand_offset, sizeof(rand_offset));
    flag = (char*)FLAG_BASE_ADDR + ((size_t)rand_offset * FLAG_MAX_LEN);

    // Read the flag into the chosen slot
    flag_fd = open(FLAG_PATH, O_RDONLY | O_CLOEXEC);
    if (-1 == flag_fd)
    {
        fprintf(stderr, "swear_machine: failed to open flag: %d\n", errno);
        goto cleanup;
    }
    read(flag_fd, flag, FLAG_MAX_LEN - 1);
    close(flag_fd);
    flag_fd = -1;

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    // Draw instructions on the background
    mvprintw(0, 0, "Press ENTER for a new swear. Press ESC to quit.");
    refresh();

    // Main event loop
    while (true)
    {
        // Get next keypress
        ch = getch();

        // Handle ESC - exit
        if (27 == ch)
        {
            break;
        }
        
        // Handle ENTER - draw a new swear window
        if (('\n' == ch) || (KEY_ENTER == ch))
        {
            if (window_count >= MAX_WINDOWS)
            {
                break;
            }
            swear = g_swears[rand() % SWEAR_COUNT];
            windows[window_count] = draw_swear_window(swear);
            if (NULL != windows[window_count])
            {
                window_count++;
            }
        }
    }

    // Success
    ok = true;

cleanup:

    // Free ncurses windows
    for (counter = 0; counter < (size_t)window_count; counter++)
    {
        if (NULL != windows[counter])
        {
            delwin(windows[counter]);
            windows[counter] = NULL;
        }
    }

    // Shut down ncurses
    endwin();

    // Free resources
    if (-1 != flag_fd)
    {
        close(flag_fd);
        flag_fd = -1;
    }
    if (MAP_FAILED != flag_map)
    {
        if (NULL != flag)
        {
            memset(flag, 0, FLAG_MAX_LEN);
            flag = NULL;
        }
        munmap(flag_map, FLAG_MAP_LEN);
        flag_map = MAP_FAILED;
    }

    // Indicate result
    return ok ? 0 : 1;
}
