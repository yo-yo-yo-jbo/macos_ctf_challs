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
#include <errno.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <unistd.h>

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
#define MARKER_PATH ("/private/var/tmp/marker/marker")

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
#define MARKER_MAX_SIZE (100000)

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The appender binary.                                                           *
*  Returns:    Non-zero in case of failure, zero in case of success.                                        *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
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
        // Fail unless it's file-not-found
        if (ENOENT != errno)
        {
            fprintf(stderr, "appender: marker file could not be opened\n");
            goto cleanup;
        }

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
        // Set the file to be quarantined
        if (0 != fsetxattr(marker_fd, "com.apple.quarantine", "0083;67fc1a3b;JBO;00000000-0000-0000-0000-000000000000", 54, 0, 0))
        {
            fprintf(stderr, "appender: cannot set quarnatine attributes\n");
            goto cleanup;
        }

        // Set the file to be readable and executable only
        if (0 != fchmod(marker_fd, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
        {
            fprintf(stderr, "appender: cannot set file mode\n");
            goto cleanup;
        }

        // Seek to the end of the marker file
        offset = lseek(marker_fd, 0, SEEK_END);
        if (-1 == offset)
        {
            fprintf(stderr, "appender: cannot seek to the end of the file\n");
            goto cleanup;
        }

        // Read from standard input and append to the marker file
        len = offset;
        for (;;)
        {
            bytes_done = read(STDIN_FILENO, buf, sizeof(buf));
            if (0 > bytes_done)
            {
                fprintf(stderr, "appender: cannot read from stdin\n");
                goto cleanup;
            }
            if (0 == bytes_done)
            {
                break;
            }
            if (write(marker_fd, buf, bytes_done) != bytes_done)
            {
                fprintf(stderr, "appender: cannot write to marker file\n");
                goto cleanup;
            }
            len += (size_t)bytes_done;
        }

        // Check for maximum size
        if (MARKER_MAX_SIZE < len)
        {
            fprintf(stderr, "appender: marker file is too big - resetting it\n");
            (void)lseek(marker_fd, 0, SEEK_SET);
            (void)ftruncate(marker_fd, 0);            
            goto cleanup;
        }
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
