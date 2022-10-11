#!/bin/sh

# Runs the specified command and prints its run time in minutes and seconds.
# Parameters: $1: The command(s) to run.
#             $2: The file to write the output to.
#             $3: The prefix string to write in front of the run time.
# Returns: The return value of the command.

st_date=`date +"%s"`

eval $1 1> $2
ret=$?

en_date=`date +"%s"`

diff=$(($en_date-$st_date))
count=`wc -l < $2`
    
printf "    %-8s - %2dm%02ds - %8d matches\n" "$3" $(($diff / 60)) $(($diff % 60)) $count

return $ret
