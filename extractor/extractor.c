/************************************************************************************************************
*                                                                                                           *
*  File:       extractor.c                                                                                  *
*  Purpose:    An extractor utility.                                                                        *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*                                                                                                           *
************************************************************************************************************/
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <ftw.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   BLOCK_SIZE                                                                                   *
*  Purpose:    Block size in bytes.                                                                         *
*                                                                                                           *
************************************************************************************************************/
#define BLOCK_SIZE (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_MAX_SIZE                                                                                *
*  Purpose:    Maximum flag size in bytes.                                                                  *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_MAX_SIZE (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_PATH_LEN                                                                                 *
*  Purpose:    Maximum path length.                                                                         *
*                                                                                                           *
************************************************************************************************************/
#define MAX_PATH_LEN (8192)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_DEPTH                                                                                    *
*  Purpose:    Maximum directory depth.                                                                     *
*                                                                                                           *
************************************************************************************************************/
#define MAX_DEPTH (64)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   EXTRACTOR_TMP_PATH                                                                           *
*  Purpose:    Base directory for extraction.                                                               *
*                                                                                                           *
************************************************************************************************************/
#define EXTRACTOR_TMP_PATH ("/Users/ctf/extractor_tmp")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   SYSTEM_FLAG_PATH                                                                             *
*  Purpose:    Path of the system flag file.                                                                *
*                                                                                                           *
************************************************************************************************************/
#define SYSTEM_FLAG_PATH ("/Users/ctf/flag")

/************************************************************************************************************
*                                                                                                           *
*  Function:   copy_data                                                                                    *
*  Purpose:    Copy data from one archive data block to the other archive.                                  *
*  Parameters: - ar - the archive to read from.                                                             *
*              - aw - the archive to write to.                                                              *
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
            fprintf(stderr, "extractor: archive_read_data_block failed: %d\n", err);
            goto cleanup;
        }

        // Write to target archive
        err = archive_write_data_block(aw, buff, size, offset);
        if (ARCHIVE_OK != err)
        {
            fprintf(stderr, "extractor: archive_write_data_block failed: %d\n", err);
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
*  Function:   validate_basename                                                                            *
*  Purpose:    Validates that a name is non-empty and purely alphanumeric.                                  *
*  Parameters: - name - the name to validate.                                                               *
*  Returns:    True if valid, false otherwise.                                                              *
*                                                                                                           *
************************************************************************************************************/
static
bool
validate_basename(
    const char* name
)
{
    const char* curr = NULL;
    bool ok = false;

    // Validates against an empty string
    if (NULL == name || '\0' == *name)
    {
        goto cleanup;
    }

    // Check all characters
    for (curr = name; '\0' != *curr; curr++)
    {
        if (!isalnum((unsigned char)*curr))
        {
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
*  Function:   create_dest_dir                                                                              *
*  Purpose:    Creates the destination directory with owner-only read/write permissions.                    *
*  Parameters: - path - the directory path to create.                                                       *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
create_dest_dir(
    const char* path
)
{
    bool ok = false;
    int fd = -1;
    struct stat st = { 0 };

    // Create directory
    if (0 != mkdir(path, 0700))
    {
        fprintf(stderr, "extractor: mkdir failed: %s\n", path);
        goto cleanup;
    }

    // Open with O_NOFOLLOW to guard against a symlink race on the final component
    fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (-1 == fd)
    {
        fprintf(stderr, "extractor: open directory failed: %s\n", path);
        goto cleanup;
    }

    // Verify via file descriptor that it is a real directory and not a symlink
    if (0 != fstat(fd, &st))
    {
        fprintf(stderr, "extractor: fstat failed\n");
        goto cleanup;
    }
    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "extractor: path is not a directory: %s\n", path);
        goto cleanup;
    }

    // Owner read/write only
    if (0 != fchmod(fd, 0600))
    {
        fprintf(stderr, "extractor: fchmod failed\n");
        goto cleanup;
    }

    // Success
    ok = true;

cleanup:

    // Free resources
    if (-1 != fd)
    {
        close(fd);
        fd = -1;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   remove_entry                                                                                 *
*  Purpose:    An nftw(3) callback that removes a single file or directory entry.                           *
*  Parameters: - path - entry path.                                                                         *
*              - sb - stat buffer (unused).                                                                 *
*              - typeflag - entry type.                                                                     *
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
    return result == 0 ? 0 : -1;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   remove_dest_dir                                                                              *
*  Purpose:    Recursively removes a directory tree without following symlinks.                             *
*  Parameters: - path - the root of the tree to remove.                                                     *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
remove_dest_dir(
    const char* path
)
{
    // Run with post-order (required for rmdir) and without following symlinks
    return 0 == nftw(path, remove_entry, MAX_DEPTH, FTW_DEPTH | FTW_PHYS);
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   extract_archive                                                                              *
*  Purpose:    Extracts the given filepath into the destination path.                                       *
*  Parameters: - filepath - the archive filepath.                                                           *
*              - dest - the destination path.                                                               *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
extract_archive(
    const char* filepath,
    const char* dest
)
{
    bool ok = false;
    struct archive* ar = NULL;
    struct archive* ext = NULL;
    struct archive_entry* entry = NULL;
    int err = 0;
    const char* current = NULL;
    char fullpath[4096] = { 0 };

    // Prepare archive
    ar = archive_read_new();
    archive_read_support_format_all(ar);
    archive_read_support_filter_all(ar);

    // Prepare extraction
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(ext);

    // Open archive for reading
    if (archive_read_open_filename(ar, filepath, BLOCK_SIZE))
    {
        fprintf(stderr, "extractor: archive_read_open_filename failed\n");
        goto cleanup;
    }

    // Iterate all headers
    while (ARCHIVE_OK == (err = archive_read_next_header(ar, &entry)))
    {
        // Get the pathname
        current = archive_entry_pathname(entry);

        // Prevent path traversal
        if (NULL != strstr(current, ".."))
        {
            fprintf(stderr, "extractor: suspicious path found: %s\n", current);
            archive_read_data_skip(ar);
            goto cleanup;
        }

        // Rewrite path to destination directory
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dest, current);
        archive_entry_set_pathname(entry, fullpath);

        // Write entry
        err = archive_write_header(ext, entry);
        if (ARCHIVE_OK != err)
        {
            fprintf(stderr, "extractor: archive_write_header failed: %d\n", err);
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
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   compare_flag                                                                                 *
*  Purpose:    Compares the extracted flag against the system flag using byte-by-byte XOR.                  *
*  Parameters: - extracted_flag_path - path to the extracted flag file.                                     *
*  Returns:    True if they match, false otherwise.                                                         *
*                                                                                                           *
************************************************************************************************************/
static
bool
compare_flag(
    const char* extracted_flag_path
)
{
    bool win = false;
    int fd1 = -1;
    int fd2 = -1;
    uint8_t buf1[FLAG_MAX_SIZE] = { 0 };
    uint8_t buf2[FLAG_MAX_SIZE] = { 0 };
    ssize_t n1 = 0;
    ssize_t n2 = 0;
    uint8_t diff = 0;
    ssize_t counter = 0;

    // Open extracted flag with O_NOFOLLOW to prevent symlink attacks
    fd1 = open(extracted_flag_path, O_RDONLY | O_NOFOLLOW);
    if (-1 == fd1)
    {
        fprintf(stderr, "extractor: cannot open extracted flag\n");
        goto cleanup;
    }

    // Open system flag
    fd2 = open(SYSTEM_FLAG_PATH, O_RDONLY | O_NOFOLLOW);
    if (-1 == fd2)
    {
        fprintf(stderr, "extractor: cannot open system flag\n");
        goto cleanup;
    }

    // Read both flags
    n1 = read(fd1, buf1, sizeof(buf1));
    n2 = read(fd2, buf2, sizeof(buf2));
    if (n1 < 0 || n2 < 0 || n1 != n2)
    {
        goto cleanup;
    }

    // Byte-by-byte XOR comparison - no short-circuit to avoid timing side-channels
    for (counter = 0; counter < n1; counter++)
    {
        diff |= buf1[counter] ^ buf2[counter];
    }

    // Set result
    win = (0 == diff);

cleanup:

    // Free resources
    if (-1 != fd1)
    {
        close(fd1);
        fd1 = -1;
    }
    if (-1 != fd2)
    {
        close(fd2);
        fd2 = -1;
    }

    // Indicate result
    return win;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. Extracts archives.                                                             *
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
    bool dest_created = false;
    const char* filepath = NULL;
    const char* last_slash = NULL;
    const char* base = NULL;
    char dest[MAX_PATH_LEN] = { 0 };
    char flag_path[MAX_PATH_LEN] = { 0 };

    // Validate commandline arguments
    if (2 != argc)
    {
        fprintf(stderr, "extractor: invalid arguments\n");
        goto cleanup;
    }
    filepath = argv[1];

    // Derive basename without modifying the argument
    last_slash = strrchr(filepath, '/');
    base = (NULL != last_slash) ? last_slash + 1 : filepath;

    // Validate basename is non-empty and alphanumeric
    if (!validate_basename(base))
    {
        fprintf(stderr, "extractor: invalid archive name: %s\n", base);
        goto cleanup;
    }

    // Build destination path
    snprintf(dest, sizeof(dest), "%s/%s", EXTRACTOR_TMP_PATH, base);

    // Create destination directory securely (no symlink follow, owner r/w only)
    if (!create_dest_dir(dest))
    {
        goto cleanup;
    }
    dest_created = true;

    // Extract archive
    if (!extract_archive(filepath, dest))
    {
        goto cleanup;
    }

    // Check if a flag file was extracted and compare it
    snprintf(flag_path, sizeof(flag_path), "%s/flag", dest);
    if (0 == access(flag_path, F_OK))
    {
        if (compare_flag(flag_path))
        {
            printf("you win!\n");
        }
        else
        {
            printf("you lose.\n");
        }
    }

    // Success
    ok = true;

cleanup:

    // Always remove the temporary extraction directory
    if (dest_created)
    {
        remove_dest_dir(dest);
    }

    // Indicate result
    return ok ? 0 : 1;
}
