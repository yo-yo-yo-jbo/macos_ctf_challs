/************************************************************************************************************
*                                                                                                           *
*  File:       jail.c                                                                                       *
*  Purpose:    Runs /bin/zsh interactively in a minimal jail.                                               *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <sandbox.h>
#include <stdio.h>
#include <unistd.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   PROFILE                                                                                      *
*  Purpose:    Minimal SBPL profile for running /bin/zsh.                                                   *
*                                                                                                           *
************************************************************************************************************/
static
const
char
PROFILE[] =
    "(version 1)\n"
    "(deny default)\n"

    // Self-execution
    "(allow process-exec\n"
    "    (literal \"/bin/zsh\")\n"
    ")\n"

    // The challenge can be run without a sandbox
    "(allow process-exec\n"
    "    (literal \"/Users/ctf/one\")\n"
    "    (with no-sandbox)\n"
    ")\n"

    // Forking is allowed
    "(allow process-fork)\n"

    // Signals are allowed
    "(allow signal)\n"

    // Reading own library and some devices
    "(allow file-read*\n"
    "    (subpath \"/usr/lib/zsh\")\n"
    "    (regex #\"^/dev/tty.*$\")\n"
    "    (regex #\"^/dev/pty.*$\")\n"
    "    (literal \"/dev/null\")\n"
    "    (literal \"/\")\n"
    "    (literal \"/bin/zsh\")\n"
    ")\n"

    // Performing terminal IOCTL calls
    "(allow file-ioctl\n"
    "    (regex #\"^/dev/tty.*$\")\n"
    "    (regex #\"^/dev/pty.*$\")\n"
    "    (literal \"/dev/null\")\n"
    ")\n"

    // Terminal I/O writes
    "(allow file-write*\n"
    "    (regex #\"^/dev/tty.*$\")\n"
    "    (regex #\"^/dev/pty.*$\")\n"
    "    (literal \"/dev/null\")\n"
    ")\n"

    // Performing sysctl reads for dyld and such
    "(allow sysctl-read)\n"
;

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. Runs the zsh binary in a minimal jail.                                         *
*  Returns:    Non-zero in case of failure. Successful execution does not return due to execve.             *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    char* err = NULL;
    char* argv[] = { "/bin/zsh", "-f", NULL };
    char* envp[] = {
        "TERM=xterm-256color",
        "HOME=/Users/ctf",
        "PATH=/Users/ctf",
        "SHELL=/bin/zsh",
        NULL
    };

    // Apply sandbox
    if (0 != sandbox_init(PROFILE, 0, &err))
    {
        fprintf(stderr, "jail: sandbox_init failed: %s\n", err ? err : "(unknown)");
        goto cleanup;
    }

    // Execute and indicate an error if execve fails
    execve("/bin/zsh", argv, envp);
    fprintf(stderr, "jail: execve failed\n");

cleanup:

    // Free resources
    if (NULL != err)
    {
        sandbox_free_error(err);
        err = NULL;
    }

    // Always indicate failure here
    return 1;
}

