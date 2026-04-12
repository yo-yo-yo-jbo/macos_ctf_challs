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
gcc -o ./gates_bill ./gates_bill.c -I/opt/homebrew/opt/libarchive/include -larchive -framework CoreFoundation
if [[ $? != 0 ]]; then
    echo Compilation error: gates_bill
    exit 1
fi

# Install binaries to /Users/ctf
rm -f /Users/ctf/gates_bill 2>/dev/null
rm -f /Users/ctf/flag 2>/dev/null
mkdir /Users/ctf 2>/dev/null
cp ./gates_bill /Users/ctf/
echo "$FLAG" > /Users/ctf/flag
rm -f ./gates_bill
chown root:wheel /Users/ctf/gates_bill
if [[ $? != 0 ]]; then
    echo Error setting owner: gates_bill
    exit 1
fi
chmod +s /Users/ctf/gates_bill
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

