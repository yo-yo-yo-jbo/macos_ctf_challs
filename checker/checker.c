/************************************************************************************************************
*                                                                                                           *
*  File:       checker.c                                                                                    *
*  Purpose:    The checker challenge.                                                                       *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sqlite3.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_PATH                                                                                    *
*  Purpose:    Path to the flag file.                                                                       *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_PATH ("/Users/ctf/flag")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_FLAG_SIZE                                                                                *
*  Purpose:    Maximum flag size in bytes.                                                                  *
*                                                                                                           *
************************************************************************************************************/
#define MAX_FLAG_SIZE (256)

/************************************************************************************************************
*                                                                                                           *
*  Global:     environ                                                                                      *
*  Purpose:    Contains the environment variables for this program.                                         *
*                                                                                                           *
************************************************************************************************************/
extern char** environ;

/************************************************************************************************************
*                                                                                                           *
*  Function:   drop_env                                                                                     *
*  Purpose:    Drops all environment variables besides PATH.                                                *
*  Returns:    True in case of success, false in case of a failure.                                         *
*                                                                                                           *
************************************************************************************************************/
static
bool
drop_env(void)
{
    bool ok = false;
    static char* empty_env[] = { NULL };
    char* path_val = NULL;
    char* path_saved = NULL;

    // Get the path value and duplicate it
    path_val = getenv("PATH");
    if (NULL != path_val)
    {
        path_saved = strdup(path_val);
        if (NULL == path_saved)
        {
            fprintf(stderr, "checker: memory error\n");
            goto cleanup;
        }
    }

    // Clean all environment variables
    environ = empty_env;

    // Set the PATH
    if (NULL != path_saved)
    {
        if (0 != setenv("PATH", path_saved, 1))
        {
            fprintf(stderr, "checker: error setting PATH\n");
            goto cleanup;
        }
    }

    // Success
    ok = true;

cleanup:

    // Free resources
    if (NULL != path_saved)
    {
        free(path_saved);
        path_saved = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   check_flag                                                                                   *
*  Purpose:    Checks the flag.                                                                             *
*  Parameters: - path - the candidate path.                                                                 *
*              - is_match - whether there is a match or not.                                                *
*  Returns:    True in case of success, false in case of a failure.                                         *
*                                                                                                           *
************************************************************************************************************/
static
bool
check_flag(
    char* path,
    bool* is_match
)
{
    bool ok = false;
    int fd = -1;
    char candidate[MAX_FLAG_SIZE + 1] = {0};
    ssize_t bytes_read = 0;
    sqlite3* db = NULL;
    sqlite3_stmt* stmt = NULL;
    const char* flag = NULL;
    size_t flag_len = 0;
    size_t candidate_len = 0;
    uint8_t diff = 0;
    size_t i = 0;

    // Open and read the candidate file
    fd = open(path, O_RDONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "checker: cannot open candidate file\n");
        goto cleanup;
    }
    bytes_read = read(fd, candidate, MAX_FLAG_SIZE);
    if (-1 == bytes_read)
    {
        fprintf(stderr, "checker: read error\n");
        goto cleanup;
    }

    // Open the flag SQLite DB and read the flag value
    if (SQLITE_OK != sqlite3_open_v2(FLAG_PATH, &db, SQLITE_OPEN_READONLY, NULL))
    {
        fprintf(stderr, "checker: cannot open flag DB\n");
        goto cleanup;
    }
    if (SQLITE_OK != sqlite3_prepare_v2(db, "SELECT flag FROM secret LIMIT 1", -1, &stmt, NULL))
    {
        fprintf(stderr, "checker: cannot prepare query\n");
        goto cleanup;
    }
    if (SQLITE_ROW != sqlite3_step(stmt))
    {
        fprintf(stderr, "checker: no flag row\n");
        goto cleanup;
    }
    flag = (const char*)sqlite3_column_text(stmt, 0);
    if (NULL == flag)
    {
        fprintf(stderr, "checker: null flag\n");
        goto cleanup;
    }

    // Safe (constant-time) XOR comparison
    flag_len = strlen(flag);
    candidate_len = strlen(candidate);
    diff = (flag_len != candidate_len) ? 1 : 0;
    for (i = 0; i < flag_len && i < candidate_len; i++)
    {
        diff |= (uint8_t)((unsigned char)flag[i] ^ (unsigned char)candidate[i]);
    }
    *is_match = (0 == diff);

    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != fd)
    {
        close(fd);
        fd = -1;
    }
    if (NULL != stmt)
    {
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (NULL != db)
    {
        sqlite3_close(db);
        db = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   find_in_path                                                                                 *
*  Purpose:    Finds a relative path in PATH.                                                               *
*  Parameters: - relative - the relative path.                                                              *
*              - absolute - gets the absolute path in the case of success.                                  *
*  Returns:    True in case of success, false in case of a failure.                                         *
*  Remarks:    - Free the returned absolute path with free.                                                 *
*                                                                                                           *
************************************************************************************************************/
static
bool
find_in_path(
    char* relative,
    char** absolute
)
{
    bool ok = false;
    size_t relative_len = 0;
    char* path_env = NULL;
    char* path_env_dup = NULL;
    char* token = NULL;
    char* full_path = NULL;
    size_t token_len = 0;
    int fd = -1;

    // Calculate the relative path length
    relative_len = strlen(relative);

    // Get the path environment variable
    path_env = getenv("PATH");
    if (NULL == path_env)
    {
        fprintf(stderr, "checker: missing PATH\n");
        goto cleanup;
    }

    // Duplicate
    path_env_dup = strdup(path_env);
    if (NULL == path_env_dup)
    {
        fprintf(stderr, "checker: memory error\n");
        goto cleanup;
    }

    // Tokenize
    token = strtok(path_env_dup, ":");
    while (NULL != token)
    {
        // Calculate sizes
        token_len = strlen(token);

        // Create the full path
        full_path = calloc(token_len + relative_len + 2, sizeof(*full_path));
        if (NULL == full_path)
        {
            fprintf(stderr, "checker: memory error\n");
            goto cleanup;
        }

        // Build the full path
        if (token_len + relative_len + 1 != snprintf(full_path, token_len + relative_len + 2, "%s/%s", token, relative))
        {
            fprintf(stderr, "checker: error building absolute path\n");
            goto cleanup;
        }

        // Check for existence
        fd = open(full_path, O_RDONLY);
        if (-1 != fd)
        {
            close(fd);
            break;
        }

        // Free iteration resources
        free(full_path);
        full_path = NULL;

        // Move on to next token
        token = strtok(NULL, ":");
    }

    // Fail if not found
    if (NULL == full_path)
    {
        fprintf(stderr, "checker: cannot find in PATH\n");
        goto cleanup;
    }

    // Success
    *absolute = full_path;
    full_path = NULL;
    ok = true;

cleanup:

    // Free resources
    if (NULL != full_path)
    {
        free(full_path);
        full_path = NULL;
    }
    if (NULL != path_env_dup)
    {
        free(path_env_dup);
        path_env_dup = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. Runs the checker challenge.                                                    *
*  Returns:    Zero in case of success or non-zero in case of a failure.                                    *
*                                                                                                           *
************************************************************************************************************/
int
main(
    int argc,
    char** argv
)
{
    bool ok = false;
    bool verbose = false;
    size_t index = 0;
    char* path = NULL;
    bool flag_match = false;

    // Drop all environment variables besides PATH
    if (!drop_env())
    {
        goto cleanup;
    }

    // Parse arguments
    for (index = 1; index < argc; index++)
    {
        // Show usage
        if (0 == strcmp("--usage", argv[index]))
        {
            printf("usage: %s: [--verbose] [path]\n", argv[0]);
            ok = true;
            goto cleanup;
        }

        // Verbose flag
        if (0 == strcmp("--verbose", argv[index]))
        {
            verbose = true;
            continue;
        }

        // All other cases mean we have the path now
        break;
    }

    // Now we should have the path
    if (NULL == argv[index])
    {
        fprintf(stderr, "checker: insufficient arguments\n");
        goto cleanup;
    }

    // Find the path
    if ('/' != argv[index][0])
    {
        if (!find_in_path(argv[index], &path))
        {
            goto cleanup;
        }
        argv[index] = path;   
    }
    else
    {
        path = strdup(argv[index]);
        if (NULL == path)
        {
            fprintf(stderr, "checker: memory error\n");
            goto cleanup;
        }
    }

    // Check the flag 
    if (!check_flag(path, &flag_match))
    {
        goto cleanup;
    }

    // Show comparison result
    if (verbose)
    {
        printf("Path: %s\n", path);
    }
    printf(flag_match ? "Match!\n" : "No match!\n");

    // Success
    ok = true;

cleanup:

    // Free resources
    if (NULL != path)
    {
        free(path);
        path = NULL;
    }

    // Indicate result
    return ok ? 0 : 1;
}

