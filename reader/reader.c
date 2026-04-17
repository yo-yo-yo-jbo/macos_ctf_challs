/************************************************************************************************************
*                                                                                                           *
*  File:       reader.c                                                                                     *
*  Purpose:    The reader challenge.                                                                        *
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
#define LOCK_FILE_PATH ("/var/run/reader.lock")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   BUFFER_SIZE                                                                                  *
*  Purpose:    Buffer size in bytes.                                                                        *
*                                                                                                           *
************************************************************************************************************/
#define BUFFER_SIZE (8192)

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
            fprintf(stderr, "reader: could not set limit of type %d\n", g_limits[counter].resource);
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
        fprintf(stderr, "reader: failed to open lock file\n");
        goto cleanup;
    }

    // Attempt a non-blocking exclusive lock
    if (-1 == flock(lock_fd_tmp, LOCK_EX | LOCK_NB))
    {
        fprintf(stderr, "reader: another instance is already running\n");
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
        fprintf(stderr, "reader: cannot run as root\n");
        goto cleanup;
    }

    // Spawn the marker process
    if (0 != posix_spawn(&pid, MARKER_PATH, NULL, NULL, marker_args, empty_env))
    {
        fprintf(stderr, "reader: posix_spawn failed: %d\n", errno);
        goto cleanup;
    }

    // Wait for it to exit and capture its exit status
    if (pid != waitpid(pid, &status, 0))
    {
        fprintf(stderr, "reader: waitpid failed: %d\n", errno);
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
        fprintf(stderr, "reader: cannot open flag\n");
        goto cleanup;
    }

    // Read flag
    bytes_done = read(flag_fd, buf, sizeof(buf));
    if (bytes_done < 0)
    {
        fprintf(stderr, "reader: cannot read flag\n");
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
*  Purpose:    Main routine. The reader binary.                                                             *
*  Parameters: - argc - the number of arguments.                                                            *
*              - argv - the commandline arguments.                                                          *
*  Returns:    Zero upon success, non-zero upon failure.                                                    *
*                                                                                                           *
************************************************************************************************************/
int
main(
    int argc,
    char** argv
)
{
    int result = 1;
    int lock_fd = -1;
    char own_path[PATH_MAX] = { 0 };
    uint32_t own_path_size = 0;
    const char* name = NULL;
    bool run_failed = false;
    int arg_fd = -1;
    uint8_t first_byte = 0;

    // Make sure we don't have limitations
    if (!unlimit())
    {
        goto cleanup;
    }

    // Figure own path
    own_path_size = sizeof(own_path);
    if (0 != _NSGetExecutablePath(own_path, &own_path_size))
    {
        fprintf(stderr, "reader: cannot get self-path\n");
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

    // Validate arguments
    if (2 > argc)
    {
        fprintf(stderr, "reader: invalid arguments\n");
        goto cleanup;
    }

    // Read the first byte
    arg_fd = open(argv[1], O_RDWR);
    if (-1 == arg_fd)
    {
        fprintf(stderr, "reader: cannot open path\n");
        goto cleanup;
    }
    if (sizeof(first_byte) != read(arg_fd, &first_byte, sizeof(first_byte)))
    {
        fprintf(stderr, "reader: cannot read first byte of file\n");
        goto cleanup;
    }

    // Print the first byte
    printf("%d\n", first_byte);

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
    if (-1 != arg_fd)
    {
        close(arg_fd);
        arg_fd = -1;
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
