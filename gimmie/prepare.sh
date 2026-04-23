#!/bin/zsh

# Flag (fine-tunable)
FLAG='JBO{this_is_my_flag}'

# Check for root
if [[ $(id -u) != 0 ]]; then
    echo Must run as root
    exit 1
fi

# Compile main challenge file
gcc -o ./gimmie ./gimmie.c -framework CoreFoundation -framework Security
if [[ $? != 0 ]]; then
    echo Compilation error: gimmie
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/gimmie 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./gimmie /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -f ./gimmie
chown root:wheel /Users/ctf/gimmie
if [[ $? != 0 ]]; then
    echo Error setting owner: gimmie
    exit 1
fi
chmod +s /Users/ctf/gimmie
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

# Copy quotes
cp -R ./quotes /Users/ctf/quotes
chown -R root:wheel /Users/ctf/quotes
