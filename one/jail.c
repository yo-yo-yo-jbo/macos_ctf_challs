#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <sandbox.h>
#include <stdio.h>
#include <unistd.h>

// Sandbox profile
static
const
char
PROFILE[] =
    "(version 1)\n"
    "(deny default)\n"

    /* Allow zsh to be exec'd */
    "(allow process-exec\n"
    "    (literal \"/bin/zsh\")\n"
    ")\n"

    /* Allow one to be exec'd without sandbox */
    "(allow process-exec\n"
    "    (literal \"/Users/ctf/one\")\n"
    "    (with no-sandbox)\n"
    ")\n"

    /* Forking is allowed (needed for pipelines, subshells, job control) */
    "(allow process-fork)\n"

    /* Signals */
    "(allow signal)\n"

    /* zsh needs to read its binary, dylibs, etc configs, zsh function paths */
    "(allow file-read*\n"
    "    (subpath \"/usr/lib\")\n"
    "    (subpath \"/System/Library\")\n"
    "    (subpath \"/dev\")\n"
    "    (literal \"/\")\n"
    "    (literal \"/bin/zsh\")\n"
    ")\n"

    // Pgrp
    "(allow file-ioctl\n"
    "    (subpath \"/dev\")\n"
    ")\n"

    /* Terminal I/O: /dev/tty, /dev/pts, /dev/null, /dev/zero */
    "(allow file-write* (regex #\"^/dev/\"))\n"

    /* Mach IPC - required by libSystem, dyld, launchd plumbing */
    "(allow mach-lookup)\n"
    "(allow mach-register)\n"

    /* sysctl reads - dyld, stack-guard init */
    "(allow sysctl-read)\n"

    /* IOKit - terminal driver queries */
    "(allow iokit-open)\n"

    /* socket() syscall for loopback mach plumbing */
    "(allow system-socket)\n";

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
        sandbox_free_error(err);
        return 1;
    }

    // Should not return
    execve("/bin/zsh", argv, envp);

    // Error out
    fprintf(stderr, "jail: execve failed\n");
    return 1;
}

