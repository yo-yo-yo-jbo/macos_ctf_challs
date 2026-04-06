/************************************************************************************************************
*                                                                                                           *
*  File:       appender.c                                                                                   *
*  Purpose:    The appender challenge.                                                                      *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_PATH                                                                                    *
*  Purpose:    Path to the flag file.                                                                       *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_PATH ("/Users/ctf/flag")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MARKER_PATH                                                                                  *
*  Purpose:    Path to the marker file.                                                                     *
*                                                                                                           *
************************************************************************************************************/
#define MARKER_PATH ("/Users/ctf/marker")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_MAX_SIZE                                                                                *
*  Purpose:    Maximum flag size in bytes.                                                                  *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_MAX_SIZE (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MARKER_MAX_SIZE                                                                              *
*  Purpose:    Maximum marker size in bytes.                                                                *
*                                                                                                           *
************************************************************************************************************/
#define MARKER_MAX_SIZE (1024 * 1024 * 42)

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The appender binary.                                                           *
*  Parameters: - argc - the number of commandline arguments.                                                *
*              - argv - the commandline arguments.                                                          *
*  Returns:    Non-zero in case of failure, zero in case of success.                                        *
*                                                                                                           *
************************************************************************************************************/
int
main(
    int argc,
    char** argv
)
{
    bool ok = false;
    int marker_fd = -1;
    int flag_fd = -1;
    char buf[FLAG_MAX_SIZE] = { 0 };
    ssize_t bytes_done = 0;
    size_t len = 0;
    off_t offset = -1;

    // Check if marker file exists
    marker_fd = open(MARKER_PATH, O_RDWR);
    if (-1 == marker_fd)
    {
        // Open flag file
        flag_fd = open(FLAG_PATH, O_RDONLY | O_NOFOLLOW);
        if (-1 == flag_fd)
        {
            fprintf(stderr, "appender: cannot open flag\n");
            goto cleanup;
        }

        // Read flag
        bytes_done = read(flag_fd, buf, sizeof(buf));
        if (bytes_done < 0)
        {
            fprintf(stderr, "appender: cannot read flag\n");
            goto cleanup;
        }

        // Print out the flag
        printf("%s\n", buf);
    }
    else
    {
        // Seek to the end of the marker file
        offset = lseek(marker_fd, SEEK_END, 0);
        if (-1 == offset)
        {
            fprintf(stderr, "appender: cannot seek to the end of the file\n");
            goto cleanup;
        } 

        // Check argument
        if (2 < argc)
        {
            fprintf(stderr, "appender: insufficient arguments\n");
            goto cleanup;
        }
        len = strlen(argv[1]);
        if ((MARKER_MAX_SIZE > len) || (MARKER_MAX_SIZE > offset + len))
        {
            fprintf(stderr, "appender: marker file is too big - resetting it\n");
            (void)lseek(marker_fd, SEEK_SET, 0);
            (void)write(marker_fd, "Womp-womp", 9);
            (void)ftruncate(marker_fd, 9);            
            goto cleanup;
        }

        // Append to the file
        (void)write(marker_fd, argv[1], len);
    }
 
    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != marker_fd)
    {
        close(marker_fd);
        marker_fd = -1;
    }
    if (-1 != flag_fd)
    {
        close(flag_fd);
        flag_fd = -1;
    }

    // Indicate result
    return ok ? 0 : 1;
}
