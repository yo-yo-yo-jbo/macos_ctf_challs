/************************************************************************************************************
*                                                                                                           *
*  File:       gimmie.c                                                                                     *
*  Purpose:    The gimmie challenge.                                                                        *
*  Remarks:    - Created by Jonathan Bar Or ("JBO"), https://jonathanbaror.com                              *
*              - Link with: -framework Security -framework CoreFoundation                                   *
*                                                                                                           *
************************************************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_MESSAGE_SIZE                                                                             *
*  Purpose:    Maximum accepted size of the signed message in bytes.                                        *
*                                                                                                           *
************************************************************************************************************/
#define MAX_MESSAGE_SIZE (4096)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_SIGNATURE_SIZE                                                                           *
*  Purpose:    Maximum accepted size of the DER-encoded ECDSA signature in bytes.                           *
*                                                                                                           *
************************************************************************************************************/
#define MAX_SIGNATURE_SIZE (256)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   MAX_FLAG_SIZE                                                                                *
*  Purpose:    Maximum accepted size of the flag file in bytes.                                             *
*                                                                                                           *
************************************************************************************************************/
#define MAX_FLAG_SIZE (256)

/************************************************************************************************************
*                                                                                                           *
*  Constant:   FLAG_PATH                                                                                    *
*  Purpose:    Path to the flag file.                                                                       *
*                                                                                                           *
************************************************************************************************************/
#define FLAG_PATH ("/Users/ctf/flag")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   EXPECTED_MESSAGE                                                                             *
*  Purpose:    The message that must be signed for the flag to be released.                                 *
*                                                                                                           *
************************************************************************************************************/
#define EXPECTED_MESSAGE ("https://jonathanbaror.com")

/************************************************************************************************************
*                                                                                                           *
*  Constant:   PUBLIC_KEY                                                                                   *
*  Purpose:    The embedded NIST P-256 ECDSA public key, in uncompressed X9.63 format (0x04 || X || Y).     *
*                                                                                                           *
************************************************************************************************************/
static const uint8_t PUBLIC_KEY[] = {
    0x04, 0x89, 0x97, 0x05, 0xf3, 0xbc, 0x58, 0xa8, 0xb0, 0x07, 0xdc, 0xfa, 0xe0, 0x4c, 0xd8, 0x99,
    0xe5, 0xfa, 0x5e, 0x2c, 0xc5, 0x5d, 0x8e, 0x55, 0x3d, 0x7e, 0x7e, 0x1a, 0x97, 0xc8, 0xa6, 0x29,
    0xe0, 0x6d, 0x0d, 0xca, 0x8c, 0x94, 0xf5, 0x24, 0xdb, 0xcb, 0xb2, 0x45, 0xe3, 0xa4, 0xf0, 0x8d,
    0x22, 0x1a, 0x76, 0x5c, 0x8a, 0x44, 0x8b, 0xc7, 0x64, 0x82, 0x32, 0xb4, 0x85, 0x69, 0x35, 0x2c,
    0x17
};

/************************************************************************************************************
*                                                                                                           *
*  Function:   read_file                                                                                    *
*  Purpose:    Reads a file fully into a freshly allocated buffer, up to a maximum size.                    *
*  Parameters: - path - the path of the file to read.                                                       *
*              - max_size - the maximum number of bytes to accept.                                          *
*              - out_buffer - gets the allocated buffer in case of success.                                 *
*              - out_size - gets the number of bytes read in case of success.                               *
*  Returns:    True upon success, false otherwise.                                                          *
*  Remarks:    - Free the returned buffer with free.                                                        *
*                                                                                                           *
************************************************************************************************************/
static
bool
read_file(
    const char* path,
    size_t max_size,
    uint8_t** out_buffer,
    size_t* out_size
)
{
    bool ok = false;
    int fd = -1;
    struct stat st = {0};
    uint8_t* buffer = NULL;
    ssize_t bytes_read = 0;
    size_t total = 0;

    // Open the file
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (-1 == fd)
    {
        fprintf(stderr, "gimmie: cannot open %s: %d\n", path, errno);
        goto cleanup;
    }

    // Stat for size
    if (0 != fstat(fd, &st))
    {
        fprintf(stderr, "gimmie: fstat(%s) failed: %d\n", path, errno);
        goto cleanup;
    }
    if ((0 >= st.st_size) || ((size_t)st.st_size > max_size))
    {
        fprintf(stderr, "gimmie: %s has invalid size\n", path);
        goto cleanup;
    }

    // Allocate
    buffer = calloc((size_t)st.st_size, sizeof(*buffer));
    if (NULL == buffer)
    {
        fprintf(stderr, "gimmie: memory error\n");
        goto cleanup;
    }

    // Read fully
    while (total < (size_t)st.st_size)
    {
        bytes_read = read(fd, buffer + total, (size_t)st.st_size - total);
        if (-1 == bytes_read)
        {
            if (EINTR == errno)
            {
                continue;
            }
            fprintf(stderr, "gimmie: read(%s) failed: %d\n", path, errno);
            goto cleanup;
        }
        if (0 == bytes_read)
        {
            break;
        }
        total += (size_t)bytes_read;
    }
    if (total != (size_t)st.st_size)
    {
        fprintf(stderr, "gimmie: short read on %s\n", path);
        goto cleanup;
    }

    // Success
    *out_buffer = buffer;
    *out_size = total;
    buffer = NULL;
    ok = true;

cleanup:

    // Free resources
    if (NULL != buffer)
    {
        free(buffer);
        buffer = NULL;
    }
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
*  Function:   load_public_key                                                                              *
*  Purpose:    Loads the embedded P-256 X9.63 public key into a SecKeyRef.                                  *
*  Parameters: - out_key - gets the public key in case of success.                                          *
*  Returns:    True upon success, false otherwise.                                                          *
*  Remarks:    - Release the returned key with CFRelease.                                                   *
*                                                                                                           *
************************************************************************************************************/
static
bool
load_public_key(
    SecKeyRef* out_key
)
{
    bool ok = false;
    CFDataRef key_data = NULL;
    CFMutableDictionaryRef attrs = NULL;
    SecKeyRef key = NULL;
    CFErrorRef err = NULL;

    // Wrap the raw key bytes
    key_data = CFDataCreate(kCFAllocatorDefault, PUBLIC_KEY, (CFIndex)sizeof(PUBLIC_KEY));
    if (NULL == key_data)
    {
        fprintf(stderr, "gimmie: CFDataCreate failed\n");
        goto cleanup;
    }

    // Build the key attributes
    attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (NULL == attrs)
    {
        fprintf(stderr, "gimmie: CFDictionaryCreateMutable failed\n");
        goto cleanup;
    }
    CFDictionarySetValue(attrs, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
    CFDictionarySetValue(attrs, kSecAttrKeyClass, kSecAttrKeyClassPublic);

    // Create the key
    key = SecKeyCreateWithData(key_data, attrs, &err);
    if (NULL == key)
    {
        fprintf(stderr, "gimmie: SecKeyCreateWithData failed\n");
        goto cleanup;
    }

    // Success
    *out_key = key;
    key = NULL;
    ok = true;

cleanup:

    // Free resources
    if (NULL != key)
    {
        CFRelease(key);
        key = NULL;
    }
    if (NULL != err)
    {
        CFRelease(err);
        err = NULL;
    }
    if (NULL != attrs)
    {
        CFRelease(attrs);
        attrs = NULL;
    }
    if (NULL != key_data)
    {
        CFRelease(key_data);
        key_data = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   verify_signature                                                                             *
*  Purpose:    Verifies a DER-encoded ECDSA-SHA256 signature over the given message.                        *
*  Parameters: - message - the signed message bytes.                                                        *
*              - message_size - the size of the message in bytes.                                           *
*              - signature - the DER-encoded signature bytes.                                               *
*              - signature_size - the size of the signature in bytes.                                       *
*              - is_valid - gets whether the signature verified successfully.                               *
*  Returns:    True upon success, false otherwise.                                                          *
*                                                                                                           *
************************************************************************************************************/
static
bool
verify_signature(
    const uint8_t* message,
    size_t message_size,
    const uint8_t* signature,
    size_t signature_size,
    bool* is_valid
)
{
    bool ok = false;
    SecKeyRef key = NULL;
    CFDataRef message_data = NULL;
    CFDataRef signature_data = NULL;
    CFErrorRef err = NULL;

    // Load the embedded public key
    if (!load_public_key(&key))
    {
        goto cleanup;
    }

    // Make sure the key supports the ECDSA verification algorithm
    if (!SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeVerify, kSecKeyAlgorithmECDSASignatureMessageX962SHA256))
    {
        fprintf(stderr, "gimmie: key does not support ECDSA-SHA256 verification\n");
        goto cleanup;
    }

    // Wrap message and signature
    message_data = CFDataCreate(kCFAllocatorDefault, message, (CFIndex)message_size);
    if (NULL == message_data)
    {
        fprintf(stderr, "gimmie: CFDataCreate(message) failed\n");
        goto cleanup;
    }
    signature_data = CFDataCreate(kCFAllocatorDefault, signature, (CFIndex)signature_size);
    if (NULL == signature_data)
    {
        fprintf(stderr, "gimmie: CFDataCreate(signature) failed\n");
        goto cleanup;
    }

    // Verify
    *is_valid = SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, message_data, signature_data, &err);

    // Success
    ok = true;

cleanup:

    // Free resources
    if (NULL != err)
    {
        CFRelease(err);
        err = NULL;
    }
    if (NULL != signature_data)
    {
        CFRelease(signature_data);
        signature_data = NULL;
    }
    if (NULL != message_data)
    {
        CFRelease(message_data);
        message_data = NULL;
    }
    if (NULL != key)
    {
        CFRelease(key);
        key = NULL;
    }

    // Indicate result
    return ok;
}

/************************************************************************************************************
*                                                                                                           *
*  Function:   main                                                                                         *
*  Purpose:    Main routine. The gimmie binary.                                                             *
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
    uint8_t* message = NULL;
    size_t message_size = 0;
    uint8_t* signature = NULL;
    size_t signature_size = 0;
    uint8_t* flag = NULL;
    size_t flag_size = 0;
    bool is_valid = false;

    // Validate commandline arguments
    if (3 != argc)
    {
        fprintf(stderr, "gimmie: insufficient arguments\n");
        goto cleanup;
    }

    // Read the message
    if (!read_file(argv[1], MAX_MESSAGE_SIZE, &message, &message_size))
    {
        goto cleanup;
    }

    // Read the signature
    if (!read_file(argv[2], MAX_SIGNATURE_SIZE, &signature, &signature_size))
    {
        goto cleanup;
    }

    // Verify the signature
    if (!verify_signature(message, message_size, signature, signature_size, &is_valid))
    {
        goto cleanup;
    }

    // Report the result
    if (!is_valid)
    {
        fprintf(stderr, "gimmie: invalid signature\n");
        goto cleanup;
    }

    // Print the signed message
    printf("gimmie: signed message: ");
    fwrite(message, 1, message_size, stdout);
    printf("\n");
    fflush(stdout);

    // If this is the magic message, release the flag
    if ((strlen(EXPECTED_MESSAGE) == message_size) &&
        (0 == memcmp(message, EXPECTED_MESSAGE, message_size)))
    {
        if (!read_file(FLAG_PATH, MAX_FLAG_SIZE, &flag, &flag_size))
        {
            goto cleanup;
        }
        printf("gimmie: ");
        fwrite(flag, 1, flag_size, stdout);
        if ((0 == flag_size) || ('\n' != flag[flag_size - 1]))
        {
            printf("\n");
        }
        fflush(stdout);
    }

    // Success
    ok = true;

cleanup:

    // Free resources
    if (NULL != flag)
    {
        free(flag);
        flag = NULL;
    }
    if (NULL != signature)
    {
        free(signature);
        signature = NULL;
    }
    if (NULL != message)
    {
        free(message);
        message = NULL;
    }

    // Indicate result
    return ok ? 0 : 1;
}
