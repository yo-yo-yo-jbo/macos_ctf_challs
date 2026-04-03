/************************************************************************************************************
*                                                                                                           *
*  File:       black_pearl.c                                                                                *
*  Purpose:    The Black Pearl challenge.                                                                   *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

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
*  Function:   main                                                                                         *
*  Purpose:    Main routine. Runs the Black Pearl script.                                                   *
*  Parameters: - argc - the number of arguments.                                                            *
*              - argv - the commandline arguments.                                                          *
*  Returns:    Non-zero in case of failure. Does not return in case of success.                             *
*                                                                                                           *
************************************************************************************************************/
int
main(
    int argc,
    char** argv
)
{
    char* perl_argv[] = { "/usr/bin/perl", "-e", SCRIPT_CONTENTS, NULL };
    char* perl_envp[] = { "PATH=/usr/bin:/bin", NULL };

    // Drop setgid
    if (0 != setgid(getegid()))
    {
        fprintf(stderr, "black_pearl: error setting GID\n");
        goto cleanup;
    }

    // Drop setuid
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
    return 1;
}
