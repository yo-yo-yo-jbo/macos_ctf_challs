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
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <mach-o/dyld.h>
#include <sys/syslimits.h>

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
*  Constant:   LOCK_FILE_PATH                                                                               *
*  Purpose:    Path to the advisory lock file used for single-instance enforcement.                         *
*                                                                                                           *
************************************************************************************************************/
#define LOCK_FILE_PATH ("/var/run/appender.lock")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   BUFFER_SIZE                                                                                  *
*  Purpose:    Buffer size in bytes.                                                                        *
*                                                                                                           *
************************************************************************************************************/
#define BUFFER_SIZE (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MARKER_MAX_SIZE                                                                              *
*  Purpose:    Maximum marker size in bytes.                                                                *
*                                                                                                           *
************************************************************************************************************/
#define MARKER_MAX_SIZE (100000)

/************************************************************************************************************
*                                                                                                           *
*  Function:   ensure_single_instance                                                                       *
*  Purpose:    Acquires an exclusive non-blocking advisory lock to prevent concurrent instances.            *
*  Parameters: - lock_fd - receives the open lock file descriptor on success.                               *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
ensure_single_instance(
    int* lock_fd
)
{
    bool ok = false;
    int lock_fd_tmp = -1;

    // Open or create the lock file
    lock_fd_tmp = open(LOCK_FILE_PATH, O_CLOEXEC | O_RDWR | O_CREAT, 0600);
    if (-1 == lock_fd_tmp)
    {
        fprintf(stderr, "appender: failed to open lock file\n");
        goto cleanup;
    }

    // Attempt a non-blocking exclusive lock
    if (-1 == flock(lock_fd_tmp, LOCK_EX | LOCK_NB))
    {
        fprintf(stderr, "appender: another instance is already running\n");
        goto cleanup;
    }

    // Success
    *lock_fd = lock_fd_tmp;
    lock_fd_tmp = -1;
    ok = true;

cleanup:

    // Free resources
    if (-1 != lock_fd_tmp)
    {
        close(lock_fd_tmp);
        lock_fd_tmp = -1;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   reset_marker                                                                                 *
*  Purpose:    Resets the marker file to contain the exact bytes of the appender binary.                    *
*  Parameters: - marker_fd - open file descriptor to the marker file.                                       *
*              - own_path  - path to the appender binary.                                                   *
*  Returns:    True in case of success, false otherwise.                                                    *
*                                                                                                           *
************************************************************************************************************/
static
bool
reset_marker(
    int marker_fd,
    const char* own_path
)
{
    bool ok = false;
    int self_fd = -1;
    char buf[BUFFER_SIZE] = { 0 };
    ssize_t bytes_done = 0;

    // Seek to the beginning and truncate
    if (-1 == lseek(marker_fd, 0, SEEK_SET))
    {
        fprintf(stderr, "appender: cannot seek to the beginning of the file\n");
        goto cleanup;
    }
    if (0 != ftruncate(marker_fd, 0))
    {
        fprintf(stderr, "appender: cannot truncate marker file\n");
        goto cleanup;
    }

    // Open own binary
    self_fd = open(own_path, O_RDONLY | O_NOFOLLOW);
    if (-1 == self_fd)
    {
        fprintf(stderr, "appender: cannot open self\n");
        goto cleanup;
    }

    // Copy own bytes into the marker file
    for (;;)
    {
        bytes_done = read(self_fd, buf, sizeof(buf));
        if (0 > bytes_done)
        {
            fprintf(stderr, "appender: cannot read self\n");
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
    }

    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != self_fd)
    {
        close(self_fd);
        self_fd = -1;
    }

    // Return result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   run_marker                                                                                   *
*  Purpose:    Runs the marker file.                                                                        *
*  Parameters: - run_failed - indicates whether running the marker file failed.                             *
*  Returns:    True in case of success, false otherwise.                                                    *
*                                                                                                           *
************************************************************************************************************/
static
bool
run_marker(
    bool* run_failed
)
{
    bool ok = false;
    pid_t pid = -1;
    char* const marker_args[] = { MARKER_PATH, NULL };
    char* const empty_env[] = { NULL };
    int pipe_fds[2] = { -1, -1 };
    posix_spawn_file_actions_t file_actions;
    bool file_actions_initialized = false;
    char output[PATH_MAX] = { 0 };
    ssize_t bytes_read = 0;

    // Create pipe to capture marker stdout
    if (0 != pipe(pipe_fds))
    {
        fprintf(stderr, "appender: pipe failed: %d\n", errno);
        goto cleanup;
    }

    // Initialize file actions
    if (0 != posix_spawn_file_actions_init(&file_actions))
    {
        fprintf(stderr, "appender: posix_spawn_file_actions_init failed\n");
        goto cleanup;
    }
    file_actions_initialized = true;

    // Redirect child stdout to the write end of the pipe
    if (0 != posix_spawn_file_actions_adddup2(&file_actions, pipe_fds[1], STDOUT_FILENO))
    {
        fprintf(stderr, "appender: posix_spawn_file_actions_adddup2 failed\n");
        goto cleanup;
    }

    // Close both pipe ends in the child (they are no longer needed after dup2)
    if (0 != posix_spawn_file_actions_addclose(&file_actions, pipe_fds[0]))
    {
        fprintf(stderr, "appender: posix_spawn_file_actions_addclose failed\n");
        goto cleanup;
    }
    if (0 != posix_spawn_file_actions_addclose(&file_actions, pipe_fds[1]))
    {
        fprintf(stderr, "appender: posix_spawn_file_actions_addclose failed\n");
        goto cleanup;
    }

    // Spawn the marker process
    if (0 != posix_spawn(&pid, MARKER_PATH, &file_actions, NULL, marker_args, empty_env))
    {
        fprintf(stderr, "appender: posix_spawn failed: %d\n", errno);
        goto cleanup;
    }

    // Close write end in parent
    close(pipe_fds[1]);
    pipe_fds[1] = -1;

    // Wait for it to exit
    if (pid != waitpid(pid, NULL, 0))
    {
        fprintf(stderr, "appender: waitpid failed: %d\n", errno);
        goto cleanup;
    }

    // Read marker stdout output
    bytes_read = read(pipe_fds[0], output, sizeof(output) - 1);
    if (0 < bytes_read)
    {
        // Strip trailing newline
        if ('\n' == output[bytes_read - 1])
        {
            output[bytes_read - 1] = '\0';
        }
    }

    // Compare output to expected path
    *run_failed = (0 != strcmp(output, MARKER_PATH));
    printf("OUTPUT: %s\n", output);

    // Success
    ok = true;

cleanup:

    // Free resources
    if (file_actions_initialized)
    {
        posix_spawn_file_actions_destroy(&file_actions);
        file_actions_initialized = false;
    }
    if (-1 != pipe_fds[0])
    {
        close(pipe_fds[0]);
        pipe_fds[0] = -1;
    }
    if (-1 != pipe_fds[1])
    {
        close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   dump_flag                                                                                    *
*  Purpose:    Dumps the flag.                                                                              *
*  Returns:    True in case of success, false otherwise.                                                    *
*                                                                                                           *
************************************************************************************************************/
static
bool
dump_flag(void)
{
    bool ok = false;
    int flag_fd = -1;
    char buf[BUFFER_SIZE] = { 0 };
    ssize_t bytes_done = 0;

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

    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != flag_fd)
    {
        close(flag_fd);
        flag_fd = -1;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The appender binary.                                                           *
*  Returns:    Zero upon success, non-zero upon failure.                                                    *
*                                                                                                           *
************************************************************************************************************/
int
main(void)
{
    int result = 1;
    int lock_fd = -1;
    int marker_fd = -1;
    char own_path[PATH_MAX] = { 0 };
    uint32_t own_path_size = sizeof(own_path);
    char buf[BUFFER_SIZE] = { 0 };
    ssize_t bytes_done = 0;
    size_t len = 0;
    off_t offset = -1;
    const char* name = NULL;
    bool run_failed = false;

    // Figure own path
    if (0 != _NSGetExecutablePath(own_path, &own_path_size))
    {
        fprintf(stderr, "appender: cannot get self-path\n");
        goto cleanup;
    }

    // If our name is "marker" return immediately
    name = strrchr(own_path, '/');
    name = (NULL != name) ? (name + 1) : own_path;
    if (0 == strcmp(name, "marker"))
    {
        printf("%s\n", own_path);
        result = 0;
        goto cleanup;
    }

    // Ensure only one instance runs at a time
    if (!ensure_single_instance(&lock_fd))
    {
        goto cleanup;
    }

    // Open the marker file
    marker_fd = open(MARKER_PATH, O_RDWR);
    if (-1 == marker_fd)
    {
        fprintf(stderr, "appender: marker file could not be opened\n");
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
        if (!reset_marker(marker_fd, own_path))
        {
            fprintf(stderr, "appender: cannot reset market file\n");
            goto cleanup;
        }
    }

    // Close marker file
    close(marker_fd);
    marker_fd = -1;

    // Try to execute marker
    if (!run_marker(&run_failed))
    {
        goto cleanup;
    }
    
    // Dump the file if the run failed
    if (run_failed)
    {
        if (!dump_flag())
        {
            goto cleanup;
        }
    }

    // Success
    result = 0;

cleanup:

    // Free resources
    if (-1 != marker_fd)
    {
        close(marker_fd);
        marker_fd = -1;
    }
    if (-1 != lock_fd)
    {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        lock_fd = -1;
    }

    // Indicate result
    return result;
}
