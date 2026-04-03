#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./black_pearl ./black_pearl.c
if [[ $? != 0 ]]; then
    echo Compilation error: black_pearl
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/black_pearl 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./black_pearl /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -f ./black_pearl
chown root:wheel /Users/ctf/black_pearl
if [[ $? != 0 ]]; then
    echo Error setting owner: black_pearl
    exit 1
fi
chmod +s /Users/ctf/black_pearl
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

