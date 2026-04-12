/************************************************************************************************************
*                                                                                                           *
*  File:       gates_bill.c                                                                                 *
*  Purpose:    The gates_bill challenge.                                                                    *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <archive.h>
#include <archive_entry.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <CoreFoundation/CoreFoundation.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   BLOCK_SIZE                                                                                   *
*  Purpose:    Block size in bytes used when reading archive data.                                          *
*                                                                                                           *
************************************************************************************************************/
#define BLOCK_SIZE (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_DEPTH                                                                                    *
*  Purpose:    Maximum directory depth for nftw traversal.                                                  *
*                                                                                                           *
************************************************************************************************************/
#define MAX_DEPTH (64)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_XATTR_VALUE_LEN                                                                          *
*  Purpose:    Maximum extended attribute value length.                                                     *
*                                                                                                           *
************************************************************************************************************/
#define MAX_XATTR_VALUE_LEN (256)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_PATH_LEN                                                                                 *
*  Purpose:    Maximum path length.                                                                         *
*                                                                                                           *
************************************************************************************************************/
#define MAX_PATH_LEN (4096)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   LOCK_FILE_PATH                                                                               *
*  Purpose:    Path to the advisory lock file used for single-instance enforcement.                         *
*                                                                                                           *
************************************************************************************************************/
#define LOCK_FILE_PATH ("/var/run/gates_bill.lock")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   IMAGE_BASE_PATH                                                                              *
*  Purpose:    Output path passed to hdiutil create (extension is appended automatically).                  *
*                                                                                                           *
************************************************************************************************************/
#define IMAGE_BASE_PATH ("/private/var/gates_bill")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   IMAGE_PATH                                                                                   *
*  Purpose:    Full path of the disk image file created by hdiutil.                                         *
*                                                                                                           *
************************************************************************************************************/
#define IMAGE_PATH ("/private/var/gates_bill.sparseimage")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MOUNT_POINT                                                                                  *
*  Purpose:    Filesystem path at which the volume is mounted.                                              *
*                                                                                                           *
************************************************************************************************************/
#define MOUNT_POINT ("/Users/ctf/mnt")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   ENTRY_PATH                                                                                   *
*  Purpose:    Path to the entry executable.                                                                *
*                                                                                                           *
************************************************************************************************************/
#define ENTRY_PATH ("/Users/ctf/mnt/entry")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   IMAGE_SIZE                                                                                   *
*  Purpose:    Absolute quota / maximum size of the disk image.                                             *
*                                                                                                           *
************************************************************************************************************/
#define IMAGE_SIZE ("1g")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   VOLUME_NAME                                                                                  *
*  Purpose:    Display name of the APFS volume created inside the disk image.                               *
*                                                                                                           *
************************************************************************************************************/
#define VOLUME_NAME ("GatesBill")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   QUARANTINE_XATTR                                                                             *
*  Purpose:    Extended attribute name used by Gatekeeper to quarantine files.                              *
*                                                                                                           *
************************************************************************************************************/
#define QUARANTINE_XATTR ("com.apple.quarantine")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   QUARANTINE_FLAGS                                                                             *
*  Purpose:    Quarantine flags: download agent (0x01) | hard quarantine (0x04).                            *
*                                                                                                           *
************************************************************************************************************/
#define QUARANTINE_FLAGS ("0005")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   QUARANTINE_AGENT                                                                             *
*  Purpose:    Agent name embedded in the quarantine extended attribute value.                              *
*                                                                                                           *
************************************************************************************************************/
#define QUARANTINE_AGENT ("gates_bill")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   CF_EPOCH_OFFSET                                                                              *
*  Purpose:    Seconds between the Unix epoch (1970-01-01) and the CoreFoundation epoch (2001-01-01).       *
*                                                                                                           *
************************************************************************************************************/
#define CF_EPOCH_OFFSET (978307200)

/************************************************************************************************************
*                                                                                                           *
*  Function:   check_qtn                                                                                    *
*  Purpose:    Checks the quarantine status of the given path.                                              *
*  Parameters: - path - the path to check.                                                                  *
*              - is_qtn - indicates whether the file is quarantined.                                        *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
check_qtn(
    char* path,
    bool* is_qtn
)
{
    bool ok = false;
    CFStringRef path_cf = NULL;
    CFURLRef url_cf = NULL;
    CFDictionaryRef qtn_cf = NULL;
    Boolean res = false;

    // Build the URL representing the path
    path_cf = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (NULL == path_cf)
    {
        goto cleanup;
    }
    url_cf = CFURLCreateWithFileSystemPath(NULL, path_cf, kCFURLPOSIXPathStyle, false);
    if (NULL == url_cf)
    {
        goto cleanup;
    }

    // Get the quarantine property
    res = CFURLCopyResourcePropertyForKey(url_cf, kCFURLQuarantinePropertiesKey, &qtn_cf, NULL);

    // Success
    *is_qtn = (res && (NULL != qtn_cf));
    ok = true;

cleanup:

    // Free resources
    if (NULL != path_cf)
    {
        CFRelease(path_cf);
        path_cf = NULL;
    }
    if (NULL != url_cf)
    {
        CFRelease(url_cf);
        url_cf = NULL;
    }
    if (NULL != qtn_cf)
    {
        CFRelease(qtn_cf);
        qtn_cf = NULL;
    }

    // Indicate result
    return ok;
}

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
        fprintf(stderr, "gates_bill: posix_spawn(%s) failed: %s\n", args[0], strerror(errno));
        goto cleanup;
    }

    // Wait for child to exit
    if (pid != waitpid(pid, &status, 0))
    {
        fprintf(stderr, "gates_bill: waitpid failed: %s\n", strerror(errno));
        goto cleanup;
    }

    // Check exit code
    if (!WIFEXITED(status) || 0 != WEXITSTATUS(status))
    {
        fprintf(stderr, "gates_bill: %s exited with code %d\n", args[0], WEXITSTATUS(status));
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
*  Function:   copy_data                                                                                    *
*  Purpose:    Copies archive data blocks from a reader archive to a writer archive.                        *
*  Parameters: - ar - the source archive to read from.                                                      *
*              - aw - the destination archive to write to.                                                  *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
copy_data(
    struct archive* ar,
    struct archive* aw
)
{
    bool ok = false;
    const void* buff = NULL;
    size_t size = 0;
    la_int64_t offset = 0;
    int err = 0;

    // Iterate all blocks
    while (true)
    {
        // Read next block and bail-out if we're done
        err = archive_read_data_block(ar, &buff, &size, &offset);
        if (ARCHIVE_EOF == err)
        {
            break;
        }
        if (ARCHIVE_OK != err)
        {
            fprintf(stderr, "gates_bill: archive_read_data_block failed: %d\n", err);
            goto cleanup;
        }

        // Write to target archive
        err = archive_write_data_block(aw, buff, size, offset);
        if (ARCHIVE_OK != err)
        {
            fprintf(stderr, "gates_bill: archive_write_data_block failed: %d\n", err);
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
*  Function:   apply_quarantine_xattr                                                                       *
*  Purpose:    Stamps a filesystem entry with com.apple.quarantine data mimicking Gatekeeper.               *
*  Parameters: - path - absolute path of the file or directory to annotate.                                 *
*                                                                                                           *
************************************************************************************************************/
static
void
apply_quarantine_xattr(
    const char* path
)
{
    char value[MAX_XATTR_VALUE_LEN] = { 0 };
    uint32_t cf_ts = 0;
    uint32_t uuid[4] = { 0 };

    // Compute CoreFoundation timestamp (seconds since 2001-01-01)
    cf_ts = (uint32_t)((uint64_t)time(NULL) - CF_EPOCH_OFFSET);

    // Generate a random UUID
    uuid[0] = arc4random();
    uuid[1] = arc4random();
    uuid[2] = arc4random();
    uuid[3] = arc4random();

    // Build quarantine string: FLAGS;TIMESTAMP;AGENT;UUID
    snprintf(value, sizeof(value),
        "%s;%08x;%s;%08x-%04x-%04x-%04x-%04x%08x",
        QUARANTINE_FLAGS,
        cf_ts,
        QUARANTINE_AGENT,
        uuid[0],
        (uuid[1] >> 16) & 0xFFFF,
        uuid[1] & 0xFFFF,
        (uuid[2] >> 16) & 0xFFFF,
        uuid[2] & 0xFFFF,
        uuid[3]);

    // Apply the attribute
    (void)setxattr(path, QUARANTINE_XATTR, value, strlen(value), 0, XATTR_NOFOLLOW);
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   remove_entry                                                                                 *
*  Purpose:    An nftw(3) callback that removes a single file or directory entry.                           *
*  Parameters: - path - entry path.                                                                         *
*              - sb - stat buffer (unused).                                                                 *
*              - typeflag - entry type flag.                                                                *
*              - ftwbuf - walk buffer (unused).                                                             *
*  Returns:    0 on success, -1 on failure.                                                                 *
*                                                                                                           *
************************************************************************************************************/
static
int
remove_entry(
    const char* path,
    const struct stat* sb,
    int typeflag,
    struct FTW* ftwbuf
)
{
    int result = -1;

    // Unused arguments
    (void)sb;
    (void)ftwbuf;

    // Delete directories or files
    if (FTW_DP == typeflag)
    {
        result = rmdir(path);
    }
    else
    {
        result = unlink(path);
    }

    // Indicate result
    return 0 == result ? 0 : -1;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   quarantine_entry                                                                             *
*  Purpose:    An nftw(3) callback that applies the quarantine extended attribute to a single entry.        *
*  Parameters: - path - entry path.                                                                         *
*              - sb - stat buffer (unused).                                                                 *
*              - typeflag - entry type flag (unused).                                                       *
*              - ftwbuf - walk buffer (unused).                                                             *
*  Returns:    Always 0 to continue the walk.                                                               *
*                                                                                                           *
************************************************************************************************************/
static
int
quarantine_entry(
    const char* path,
    const struct stat* sb,
    int typeflag,
    struct FTW* ftwbuf
)
{
    // Unused arguments
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;

    // Stamp entry; result is intentionally ignored
    apply_quarantine_xattr(path);

    // Indicate result
    return 0;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   quarantine_volume_contents                                                                   *
*  Purpose:    Walks MOUNT_POINT and applies the quarantine extended attribute to every entry.              *
*                                                                                                           *
************************************************************************************************************/
static
void
quarantine_volume_contents(void)
{
    nftw(MOUNT_POINT, quarantine_entry, MAX_DEPTH, FTW_PHYS);
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
        fprintf(stderr, "gates_bill: failed to open lock file\n");
        goto cleanup;
    }

    // Attempt a non-blocking exclusive lock
    if (-1 == flock(lock_fd_tmp, LOCK_EX | LOCK_NB))
    {
        fprintf(stderr, "gates_bill: another instance is already running\n");
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
*  Function:   create_and_mount_volume                                                                      *
*  Purpose:    Creates an APFS disk image and mounts it at MOUNT_POINT.                                     *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
create_and_mount_volume(void)
{
    bool ok = false;
    struct stat st = { 0 };
    char* const create_args[] = {
        "/usr/bin/hdiutil",
        "create",
        "-size",
        IMAGE_SIZE,
        "-type",
        "SPARSE",
        "-fs",
        "APFS",
        "-volname",
        VOLUME_NAME,
        IMAGE_BASE_PATH,
        NULL
    };
    char* const attach_args[] = {
        "/usr/bin/hdiutil",
        "attach",
        IMAGE_PATH,
        "-mountpoint",
        MOUNT_POINT,
        "-nobrowse",
        NULL
    };

    // Create the mount point directory if it does not yet exist
    if (0 != stat(MOUNT_POINT, &st))
    {
        if (0 != mkdir(MOUNT_POINT, 0755))
        {
            fprintf(stderr, "gates_bill: mkdir failed\n");
            goto cleanup;
        }
    }

    // Create an APFS disk image capped at IMAGE_SIZE
    if (!run_command(create_args))
    {
        goto cleanup;
    }

    // Attach the image and mount it at the designated mount point
    if (!run_command(attach_args))
    {
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
*  Function:   extract_archive                                                                              *
*  Purpose:    Extracts the given zip file into MOUNT_POINT without invoking a shell.                       *
*              Stamps every extracted entry with Gatekeeper quarantine metadata.                            *
*  Parameters: - filepath - path to the zip archive to extract.                                             *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
extract_archive(
    const char* filepath
)
{
    bool ok = false;
    struct archive* ar = NULL;
    struct archive* ext = NULL;
    struct archive_entry* entry = NULL;
    int err = 0;
    const char* current = NULL;
    char fullpath[MAX_PATH_LEN] = { 0 };

    // Prepare archive reader
    ar = archive_read_new();
    archive_read_support_format_zip(ar);

    // Prepare disk writer
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_SYMLINKS);
    archive_write_disk_set_standard_lookup(ext);

    // Open archive for reading
    if (archive_read_open_filename(ar, filepath, BLOCK_SIZE))
    {
        fprintf(stderr, "gates_bill: archive_read_open_filename failed\n");
        goto cleanup;
    }

    // Iterate all headers
    while (ARCHIVE_OK == (err = archive_read_next_header(ar, &entry)))
    {
        // Get the pathname and skip any leading slashes
        current = archive_entry_pathname(entry);
        while (NULL != current && '/' == *current)
        {
            current++;
        }

        // Skip empty entries
        if (NULL == current || '\0' == *current)
        {
            archive_read_data_skip(ar);
            continue;
        }

        // Reject symlinks unconditionally
        if (AE_IFLNK == archive_entry_filetype(entry))
        {
            fprintf(stderr, "gates_bill: symlinks are not permitted in the archive\n");
            archive_read_data_skip(ar);
            goto cleanup;
        }


        // Prevent path traversal
        if (NULL != strstr(current, ".."))
        {
            fprintf(stderr, "gates_bill: suspicious path found: %s\n", current);
            archive_read_data_skip(ar);
            goto cleanup;
        }

        // Rewrite path under the mount point
        snprintf(fullpath, sizeof(fullpath), "%s/%s", MOUNT_POINT, current);
        archive_entry_set_pathname(entry, fullpath);

        // Write entry header
        err = archive_write_header(ext, entry);
        if (ARCHIVE_OK != err)
        {
            fprintf(stderr, "gates_bill: archive_write_header failed: %d\n", err);
            goto cleanup;
        }

        // Handle data
        if (0 < archive_entry_size(entry))
        {
            if (!copy_data(ar, ext))
            {
                goto cleanup;
            }
        }

        // Finish current entry
        archive_write_finish_entry(ext);
    }

    // Success
    ok = true;

cleanup:

    // Free resources
    if (NULL != ar)
    {
        archive_read_close(ar);
        archive_read_free(ar);
        ar = NULL;
    }
    if (NULL != ext)
    {
        archive_write_close(ext);
        archive_write_free(ext);
        ext = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   unmount_and_destroy_volume                                                                   *
*  Purpose:    Force-detaches the mounted volume and removes the backing disk image.                        *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
unmount_and_destroy_volume(void)
{
    bool ok = false;
    char* const detach_args[] = {
        "/usr/bin/hdiutil",
        "detach",
        MOUNT_POINT,
        "-force",
        NULL
    };


    // Force-detach the volume
    if (!run_command(detach_args))
    {
        goto cleanup;
    }

    // Remove the backing image
    if (0 != unlink(IMAGE_PATH))
    {
        fprintf(stderr, "gates_bill: unlink failed\n");
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
*  Function:   run_entry                                                                                    *
*  Purpose:    Attempts to run the extracted entry - if it exists and not quarantined.                      *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
run_entry(void)
{
    bool ok = false;
    bool is_qtn = false;
    char* const entry_args[] = { ENTRY_PATH, NULL };

    // Check the quarantine state
    if (!check_qtn(ENTRY_PATH, &is_qtn))
    {
        goto cleanup;
    }

    // Avoid if quarnatined
    if (is_qtn)
    {
        fprintf(stderr, "gates_bill: you shall not pass\n");
        goto cleanup;
    }

    // Make executable
    if (0 != chmod(ENTRY_PATH, S_IRWXU))
    {
        fprintf(stderr, "gates_bill: unlink failed\n");
        goto cleanup;
    }

    // Run the entrypoint
    if (!run_command(entry_args))
    {
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
*  Purpose:    Main routine. The gates_bill binary.                                                         *
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
    bool volume_created = false;
    int lock_fd = -1;

    // Validate commandline arguments
    if (2 != argc)
    {
        fprintf(stderr, "gates_bill: invalid usage\n");
        goto cleanup;
    }

    // Ensure only one instance runs at a time
    if (!ensure_single_instance(&lock_fd))
    {
        goto cleanup;
    }

    // Create and mount the volume
    if (!create_and_mount_volume())
    {
        goto cleanup;
    }
    volume_created = true;

    // Extract the zip into the mounted volume
    if (!extract_archive(argv[1]))
    {
        goto cleanup;
    }

    // Stamp every extracted entry with Gatekeeper quarantine metadata
    quarantine_volume_contents();

    // Launch the entry
    if (!run_entry())
    {
        goto cleanup;
    }

    // Success
    ok = true;

cleanup:

    // Delete the extracted tree before detaching
    if (volume_created)
    {
        nftw(MOUNT_POINT, remove_entry, MAX_DEPTH, FTW_DEPTH | FTW_PHYS);
        (void)unmount_and_destroy_volume();
        volume_created = false;
    }

    // Release the single-instance lock
    if (-1 != lock_fd)
    {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        lock_fd = -1;
    }

    // Indicate result
    return ok ? 0 : 1;
}
