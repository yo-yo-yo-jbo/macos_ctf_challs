#!/bin/bash
#
#  Solution
#  --------
#  1. The SUID binary appends to a "marker" file which is its own copy initially.
#  2. If the marker cannot execute from any reason - it gives out the flag.
#  3. Solution - make XProtect prevent it from running.
#  4. Note we use base64 to make sure our own script is not remediated.
#

# Append malicious string
echo QTdrOXZRcjVaOGQyZkczeUx3NHhNMXNUajZQcTBuWHo1Umo4dVZ3MlloSzNlQnE5RmNMcDFEZzdIbjBaeTZYYzRUajgK | base64 -d | /Users/ctf/appender

