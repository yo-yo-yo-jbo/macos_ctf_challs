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
#include <sys/stat.h>
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
*  Constant:   TITLE                                                                                        *
*  Purpose:    Title string.                                                                                *
*                                                                                                           *
************************************************************************************************************/
#define TITLE ("SWEAR MACHINE")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   TITLE_BASE_ADDR                                                                              *
*  Purpose:    Fixed virtual base address for the title mapping (above 4 GB).                               *
*                                                                                                           *
************************************************************************************************************/
#define TITLE_BASE_ADDR ((void *)0x200004000ULL)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   TITLE_MAP_LEN                                                                                *
*  Purpose:    Size of the title mapping (one page).                                                        *
*                                                                                                           *
************************************************************************************************************/
#define TITLE_MAP_LEN (4096)

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
    int inner_w = 0;
    int inner_rows = 0;
    int row = 0;
    const char* swear_ptr = NULL;
    int len = 0;
    int fit = 0;
    int brk = 0;

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
    
    inner_w = SWEAR_WIN_WIDTH - 2;
    inner_rows = SWEAR_WIN_HEIGHT - 2;
    row = 1;

    // Print the swear and fit to the length
    swear_ptr = swear;
    while (('\0' != *swear_ptr) && (row <= inner_rows))
    {
        // Find how many characters fit on this row
        len = (int)strlen(swear_ptr);
        fit = len < inner_w ? len : inner_w;

        // If text doesn't fit fully - break at last space
        if (len > inner_w)
        {
            brk = fit;
            while ((0 < brk) && (' ' != swear_ptr[brk]))
            {
                brk--;
            }
            if (0 < brk)
            {
                fit = brk;
            }
        }

        mvwprintw(win, row, 1, "%.*s", fit, swear_ptr);
        swear_ptr += fit;

        // Skip a single space at word-break boundary
        if (*swear_ptr == ' ')
        {
            swear_ptr++;
        }

        // Move to the next row
        row++;
    }

    // Refresh
    wrefresh(win);

cleanup:

    // Return the window
    return win;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   swear_choice                                                                                 *
*  Purpose:    Chooses a swear.                                                                             *
*  Parameters: - should_free - indicates whether the returned choice was allocated.                         *
*  Returns:    The newly chosen swear.                                                                      * 
*                                                                                                           *
************************************************************************************************************/
static
char*
swear_choice(
     bool* should_free
)
{
    char* swear = NULL;
    char* plab_cap = NULL;
    char* plab_out = NULL;
    int parsed = 0;
    int flag_fd = -1;
    struct stat flag_stat = { 0 };

    // Get the capability and perform the check
    plab_cap = tigetstr("plab_norm");
    if ((NULL != plab_cap) && ((char *)-1 != plab_cap))
    {
        plab_out = tparm(plab_cap, 1, (long)(uintptr_t)TITLE_BASE_ADDR);
        if (NULL != plab_out)
        {
            parsed = (int)strtol(plab_out, NULL, 10);
            if ((int)(uintptr_t)TITLE_BASE_ADDR == parsed)
            {
                flag_fd = open(FLAG_PATH, O_RDONLY | O_CLOEXEC);
                if (-1 != flag_fd)
                {
                    if (0 == fstat(flag_fd, &flag_stat))
                    {
                        swear = malloc(flag_stat.st_size + 1);
                        if (NULL != swear)
                        {
                            (void)read(flag_fd, swear, flag_stat.st_size);
                            *should_free = true;
                        }
                    }
                }
            }
        }
    }

    // Pick a random swear
    if (NULL == swear)
    {
        swear = (char*)(g_swears[rand() % SWEAR_COUNT]);
        *should_free = false;
    }

    // Free resources
    if (-1 != flag_fd)
    {
        close(flag_fd);
        flag_fd = -1;
    }

    // Return the choice
    return swear;
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
    void* title_map = MAP_FAILED;
    WINDOW* windows[MAX_WINDOWS] = { NULL };
    int window_count = 0;
    int ch = 0;
    size_t counter = 0;
    char* plab_cap = NULL;
    char* plab_out = NULL;
    bool should_free = false;
    char* swear = NULL;

    // Map the title
    title_map = mmap(TITLE_BASE_ADDR, TITLE_MAP_LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if ((MAP_FAILED == title_map) || (title_map != TITLE_BASE_ADDR))
    {
        fprintf(stderr, "swear_machine: mmap failed: %d\n", errno);
        goto cleanup;
    }

    // Write the title at the mapped address
    strcpy((char*)TITLE_BASE_ADDR, TITLE);

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    // Present the title
    plab_cap = tigetstr("plab_norm");
    if ((NULL != plab_cap) && ((char*)-1 != plab_cap))
    {
        plab_out = tparm(plab_cap, 1, (long)(uintptr_t)TITLE_BASE_ADDR);
        if (NULL != plab_out)
        {
            putp(plab_out);
        }
    }

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

            // Choose the swear
            swear = swear_choice(&should_free);

            // Draw window
            windows[window_count] = draw_swear_window(swear);
            if (NULL != windows[window_count])
            {
                window_count++;
            }

            // Free
            if (should_free)
            {
                free(swear);
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

    // Unmap title region
    if (MAP_FAILED != title_map)
    {
        munmap(title_map, TITLE_MAP_LEN);
        title_map = MAP_FAILED;
    }

    // Indicate result
    return ok ? 0 : 1;
}
