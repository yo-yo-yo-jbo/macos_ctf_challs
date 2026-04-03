/************************************************************************************************************
*                                                                                                           *
*  File:       one.c                                                                                        *
*  Purpose:    The one challenge.                                                                           *
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

/************************************************************************************************************
*                                                                                                           *
*  Constant:   UPDATER_PLIST_PATH                                                                           *
*  Purpose:    The updater LaunchDaemon path.                                                               *
*                                                                                                           *
************************************************************************************************************/
#define UPDATER_PLIST_PATH ("/Library/LaunchDaemons/jbo.one.updater.plist")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   UPDATER_PLIST_CONTENTS                                                                       *
*  Purpose:    The updater LaunchDaemon contents.                                                           *
*                                                                                                           *
************************************************************************************************************/
#define UPDATER_PLIST_CONTENTS ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n	<key>Label</key>\n	<string>jbo.one.updater</string>\n	<key>Program</key>\n	<string>/Users/ctf/one</string>\n	<key>ProgramArguments</key>\n	<array>\n		<string>update</string>\n	</array>\n</dict>\n</plist>\n")

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The one binary.                                                                *
*  Parameters: - argc - the number of arguments.                                                            *
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
    int updater_fd = -1;

    // Validate commandline arguments
    if (2 != argc)
    {
        fprintf(stderr, "one: invalid arguments\n");
        goto cleanup;
    }

    // Optional update
    if (0 == strcmp("update", argv[1]))
    {
        fprintf(stderr, "Not yet implemented\n");
        goto cleanup;
    }

    // Create updater LaunchDaemon
    updater_fd = open(UPDATER_PLIST_PATH, O_CLOEXEC | O_RDWR | O_CREAT, 0644);
    if (-1 != updater_fd)
    {
        write(updater_fd, UPDATER_PLIST_CONTENTS, strlen(UPDATER_PLIST_CONTENTS));
        lseek(updater_fd, 0, SEEK_SET);
    }

    // Echo the message back and quit
    printf("%s\n", argv[1]);
    fflush(stdout);

    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != updater_fd)
    {
        close(updater_fd);
        updater_fd = -1;
    }

    // Indicate result
    return ok ? 0 : 1;
}

