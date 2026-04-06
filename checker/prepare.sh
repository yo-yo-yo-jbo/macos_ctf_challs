#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./checker ./checker.c -lsqlite3
if [[ $? != 0 ]]; then
    echo Compilation error: checker
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/checker 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./checker /Users/ctf/
sqlite3 /Users/ctf/flag "CREATE TABLE secret(flag TEXT); INSERT INTO secret(flag) VALUES('$FLAG');"
rm -f ./checker
chown root:wheel /Users/ctf/checker
if [[ $? != 0 ]]; then
    echo Error setting owner: checker
    exit 1
fi
chmod +s /Users/ctf/checker
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

