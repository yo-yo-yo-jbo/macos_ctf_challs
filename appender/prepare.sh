#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./appender ./appender.c
if [[ $? != 0 ]]; then
    echo Compilation error: appender
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/appender 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./appender /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -rf /private/var/tmp/marker
mkdir -p /private/var/tmp/marker
if [[ $? != 0 ]]; then
    echo Error creating marker directory
    exit 1
fi
cp ./appender /private/var/tmp/marker/marker
rm -f ./appender
chown root:wheel /Users/ctf/appender
if [[ $? != 0 ]]; then
    echo Error setting owner: appender
    exit 1
fi
chmod +s /Users/ctf/appender
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

