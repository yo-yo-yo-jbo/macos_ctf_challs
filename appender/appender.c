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
#include <sys/resource.h>

/************************************************************************************************************
*                                                                                                           *
*  Global:     g_limits                                                                                     *
*  Purpose:    The limit types and corresponding values.                                                    *
*                                                                                                           *
************************************************************************************************************/
static
const
struct {
    int resource;
    uint64_t limit;
}
g_limits[] = {
    { RLIMIT_CORE, RLIM_INFINITY },
    { RLIMIT_DATA, RLIM_INFINITY },
    { RLIMIT_FSIZE, RLIM_INFINITY },
    { RLIMIT_NOFILE, RLIM_INFINITY },
    { RLIMIT_STACK, 0x3FFC000 },
    { RLIMIT_CPU, RLIM_INFINITY },
    { RLIMIT_AS, RLIM_INFINITY },
    { RLIMIT_NPROC, 4000 },
    { RLIMIT_MEMLOCK, RLIM_INFINITY },
};

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
*  Constant:   MARKER_EXIT_CODE                                                                             *
*  Purpose:    Exit code returned by the marker when it runs successfully as the marker.                    *
*                                                                                                           *
************************************************************************************************************/
#define MARKER_EXIT_CODE (42)

/************************************************************************************************************
*                                                                                                           *
*  Function:   unlimit                                                                                      *
*  Purpose:    Makes sure our own processes does not have any resource limits.                              *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
unlimit(void)
{
    bool ok = false;
    size_t counter = 0;
    struct rlimit rl = { RLIM_INFINITY, RLIM_INFINITY };

    // Iterate all limits
    for (counter = 0; counter < sizeof(g_limits) / sizeof(*g_limits); counter++)
    {
        // Set the limit
        rl.rlim_cur = g_limits[counter].limit;
        rl.rlim_max = g_limits[counter].limit;
        if (0 != setrlimit(g_limits[counter].resource, &rl))
        {
            fprintf(stderr, "appender: could not set limit of type %d\n", g_limits[counter].resource);
            goto cleanup;
        }
    }

    // Success
    ok = true;

cleanup:

    // Indicate result
    return ok;
}

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
    int status = 0;

    // Run as root
    if (0 != setuid(0))
    {
        fprintf(stderr, "appender: cannot run as root\n");
        goto cleanup;
    }

    // Spawn the marker process
    if (0 != posix_spawn(&pid, MARKER_PATH, NULL, NULL, marker_args, empty_env))
    {
        fprintf(stderr, "appender: posix_spawn failed: %d\n", errno);
        goto cleanup;
    }

    // Wait for it to exit and capture its exit status
    if (pid != waitpid(pid, &status, 0))
    {
        fprintf(stderr, "appender: waitpid failed: %d\n", errno);
        goto cleanup;
    }

    // Compare exit code
    *run_failed = !(WIFEXITED(status) && (MARKER_EXIT_CODE == WEXITSTATUS(status)));

    // Success
    ok = true;

cleanup:

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
    uint32_t own_path_size = 0;
    char buf[BUFFER_SIZE] = { 0 };
    ssize_t bytes_done = 0;
    size_t len = 0;
    off_t offset = -1;
    const char* name = NULL;
    bool run_failed = false;

    // Make sure we don't have limitations
    if (!unlimit())
    {
        goto cleanup;
    }

    // Figure own path
    own_path_size = sizeof(own_path);
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
        result = MARKER_EXIT_CODE;
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
