#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Just in case - install libarchive for the headers
brew install libarchive 2>/dev/null

# Compile main challenge file
gcc -o ./extractor ./extractor.c -I/opt/homebrew/opt/libarchive/include -larchive
if [[ $? != 0 ]]; then
    echo Compilation error: extractor
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/extractor 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./extractor /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -f ./extractor
chown root:wheel /Users/ctf/extractor
if [[ $? != 0 ]]; then
    echo Error setting owner: extractor
    exit 1
fi
chmod +s /Users/ctf/extractor
if [[ $? != 0 ]]; then
    echo Error setting SUID
    exit 1
fi
chown root:wheel /Users/ctf/flag
if [[ $? != 0 ]]; then
    echo Error setting owner: flag
    exit 1
fi
chmod 0400 /Users/ctf/flag
if [[ $? != 0 ]]; then
    echo Error setting read only: flag
    exit 1
fi

# Create the extractor temporary directory
mkdir /Users/ctf/extractor_tmp 2>/dev/null
chown root:wheel /Users/ctf/extractor_tmp
if [[ $? != 0 ]]; then
    echo Error setting owner: extractor_tmp
    exit 1
fi
chmod 0600 /Users/ctf/extractor_tmp
if [[ $? != 0 ]]; then
    echo Error setting read-write only: extractor_tmp
    exit 1
fi

