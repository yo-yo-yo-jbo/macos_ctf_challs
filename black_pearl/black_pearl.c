/************************************************************************************************************
*                                                                                                           *
*  File:       black_pearl.c                                                                                *
*  Purpose:    The Black Pearl challenge.                                                                   *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmpx.h>
#include <stdbool.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   SCRIPT_CONTENTS                                                                              *
*  Purpose:    The Black Pearl script contents.                                                             *
*                                                                                                           *
************************************************************************************************************/
#define SCRIPT_CONTENTS         "print \"Welcome to the Black Pearl!\\n\";"                                     \
                                "open(my $fh, \"-|\", \"/usr/bin/who\") or die \"Cannot run who: $!\\n\";"      \
                                "while (my $line = <$fh>) {"                                                    \
                                "    chomp $line;"                                                              \
                                "    if ($line =~ /console/) {"                                                 \
                                "        my ($user) = split(' ', $line);"                                       \
                                "        my $uid = `/usr/bin/id -u $user`;"                                     \
                                "        chomp $uid;"                                                           \
                                "        $uid = int($uid);"                                                     \
                                "        if ($uid == 666) { print \"You win!\\n\"; }"                           \
                                "        else { print \"You lose.\\n\"; }"                                      \
                                "    }"                                                                         \
                                "}"

/************************************************************************************************************
*                                                                                                           *
*  Function:   run_script                                                                                   *
*  Purpose:    Main routine. Runs the Black Pearl script.                                                   *
*  Returns:    False in case of failure. Does not return in case of success.                                *
*                                                                                                           *
************************************************************************************************************/
static
bool
run_script(void)
{
    char* perl_argv[] = { "/usr/bin/perl", "-e", SCRIPT_CONTENTS, NULL };
    char* perl_envp[] = { "PATH=/usr/bin:/bin", NULL };

    // Change privileges
    if (0 != setgid(getegid()))
    {
        fprintf(stderr, "black_pearl: error setting GID\n");
        goto cleanup;
    }
    if (0 != setuid(geteuid()))
    {
        fprintf(stderr, "black_pearl: error setting UID\n");
        goto cleanup;
    }

    // Execute
    execve("/usr/bin/perl", perl_argv, perl_envp);
    fprintf(stderr, "black_pearl: error running perl\n");

cleanup:

    // Indicate an error
    return false;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   log_entry                                                                                    *
*  Purpose:    Logs an entry.                                                                               *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
log_entry(void)
{
    bool ok = false;
    struct utmpx entry = { 0 };
    const char* user = NULL;

    // Write utmpx entry
    printf("black_pearl: logging...\n");
    entry.ut_type = USER_PROCESS;
    entry.ut_pid = getpid();
    (void)strncpy(entry.ut_id, "perl", sizeof(entry.ut_id) - 1);
    user = getenv("USER");
    if (NULL != user)
    {
        (void)strncpy(entry.ut_user, user, sizeof(entry.ut_user) - 1);
    }
    else
    {
        (void)strncpy(entry.ut_user, "(unknown)", sizeof(entry.ut_user) - 1);
    }

    // Commit entry
    if (NULL == pututxline(&entry))
    {
        fprintf(stderr, "black_pearl: error logging entry\n");
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
*  Purpose:    Main routine. Runs the Black Pearl script.                                                   *
*  Returns:    Non-zero in case of failure. Does not return in case of success.                             *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    // Logs an entry
    if (!log_entry())
    {
        goto cleanup;
    }

    // Execute script
    (void)run_script();

cleanup:

    // Indicate an error
    return 1;
}
