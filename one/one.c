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
#include <spawn.h>
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
*  Function:   run_command                                                                                  *
*  Purpose:    Spawns a subprocess directly (no shell), waits for it, and returns its exit status.          *
*  Parameters: - args - NULL-terminated argument vector; args[0] must be the absolute executable path.      *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
run_command(
    char* const args[]
)
{
    bool ok = false;
    pid_t pid = -1;
    int status = 0;

    // Spawn child process
    char* const empty_env[] = { NULL };

    if (0 != posix_spawn(&pid, args[0], NULL, NULL, args, empty_env))
    {
        fprintf(stderr, "one: posix_spawn(%s) failed: %s\n", args[0], strerror(errno));
        goto cleanup;
    }

    // Wait for child to exit
    if (pid != waitpid(pid, &status, 0))
    {
        fprintf(stderr, "one: waitpid failed: %s\n", strerror(errno));
        goto cleanup;
    }

    // Check exit code
    if (!WIFEXITED(status) || 0 != WEXITSTATUS(status))
    {
        fprintf(stderr, "one: %s exited with code %d\n", args[0], WEXITSTATUS(status));
        goto cleanup;
    }

    // Success
    ok = true;

cleanup:

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   check                                                                                        *
*  Purpose:    Check for updates.                                                                           *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
check(void)
{
    bool ok = false;
    char* bootstrap_argv[] = { "/bin/launchctl", "bootstrap", "system", UPDATER_PLIST_PATH, NULL };
    char* kickstart_argv[] = { "/bin/launchctl", "kickstart", "-k", "system/jbo.one.updater", NULL };
    
    // Bootstrap
    if (!run_command(bootstrap_argv))
    {
        fprintf(stderr, "one: bootstrapping failed\n");
        goto cleanup;
    }

    // Kickstart
    if (!run_command(kickstart_argv))
    {
        fprintf(stderr, "one: kickstarting failed\n");
        goto cleanup;
    }

    // Success
    ok = true;

cleanup:

    // Indicate result
    return ok;
}

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

    // Optional check
    if (0 == strcmp("check", argv[1]))
    {
        if (!check())
        {        
            goto cleanup;
        }
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

