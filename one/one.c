#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UPDATER_PLIST_CONTENTS ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n	<key>Label</key>\n	<string>jbo.one.updater</string>\n	<key>Program</key>\n	<string>/Users/ctf/one</string>\n	<key>ProgramArguments</key>\n	<array>\n		<string>update</string>\n	</array>\n</dict>\n</plist>\n")

int
main(
    int argc,
    char** argv
)
{
    int fd = -1;

    // Validate commandline arguments
    if (2 != argc)
    {
        fprintf(stderr, "Usage: ./one <message>\n");
        goto cleanup;
    }

    // Optional update
    if (0 == strcmp("update", argv[1]))
    {
        fprintf(stderr, "Not yet implemented\n");
        goto cleanup;
    }

    // Create updater LaunchDaemon
    fd = open("/Library/LaunchDaemons/jbo.one.updater.plist", O_CLOEXEC | O_RDWR | O_CREAT, 0644);
    if (-1 != fd)
    {
        write(fd, UPDATER_PLIST_CONTENTS, strlen(UPDATER_PLIST_CONTENTS));
        lseek(fd, 0, SEEK_SET);
        fprintf(stderr, "DBG: %d\n", fd);
    }

    // Echo the message back
    printf("%s\n", argv[1]);
    fflush(stdout);

cleanup:

    // Free resources
    if (-1 != fd)
    {
        close(fd);
        fd = -1;
    }

    // Finish
    return 0;
}

