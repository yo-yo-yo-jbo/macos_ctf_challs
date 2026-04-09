#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./swear_machine ./swear_machine.c
if [[ $? != 0 ]]; then
    echo Compilation error: swear_machine
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/swear_machine 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./swear_machine /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -f ./swear_machine
chown root:wheel /Users/ctf/swear_machine
if [[ $? != 0 ]]; then
    echo Error setting owner: swear_machine
    exit 1
fi
chmod +s /Users/ctf/swear_machine
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

