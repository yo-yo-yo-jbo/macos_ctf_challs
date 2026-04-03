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

/************************************************************************************************************
*                                                                                                           *
*  Constant:   BLOCK_SIZE                                                                                   *
*  Purpose:    Block size in bytes.                                                                         *
*                                                                                                           *
************************************************************************************************************/
#define BLOCK_SIZE (8192)

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

    // Validate commandline arguments
    if (3 != argc)
    {
        fprintf(stderr, "extractor: invalid arguments\n");
        goto cleanup;
    }

    // Extract
    if (!extract_archive(argv[1], argv[2]))
    {
        goto cleanup;
    }

    // Success
    ok = true;

cleanup:

    // Indicate result
    return ok ? 0 : 1;
}

