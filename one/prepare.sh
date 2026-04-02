#!/bin/zsh

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./one ./one.c
if [[ $? != 0 ]]; then
    echo Compilation error: one
    exit 1
fi

# Compile jail
gcc -o ./jail ./jail.c
if [[ $? != 0 ]]; then
    rm -f ./one
    echo Compilation error: jail
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/jail 2>/dev/null
rm -f /Users/ctf/one 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./one /Users/ctf/
cp ./jail /Users/ctf
rm -f ./one
rm -f ./jail
chown root:wheel /Users/ctf/one
if [[ $? != 0 ]]; then
    echo Error setting owner: one
    exit 1
fi
chown root:wheel /Users/ctf/jail
if [[ $? != 0 ]]; then
    echo Error setting owner: jail
    exit 1
fi
chmod +s /Users/ctf/jail
if [[ $? != 0 ]]; then
    echo Error setting SUID
    exit 1
fi

