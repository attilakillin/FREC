#!/bin/sh

# Runs the specified command and prints its run time in minutes and seconds.
# Parameters: $1: The command(s) to run.
#             $2: The file to write the output to.
#             $3: The prefix string to write in front of the run time.
# Returns: The return value of the command.

st_date=$(($(date +"%s%N")/1000000))

eval $1 1> $2
ret=$?

en_date=$(($(date +"%s%N")/1000000))

diff_ns=$(($en_date-$st_date))
diff_s=$((diff_ns/1000))
count=`wc -l < $2`
    
printf "    %-8s - %2dm%02ds%03dms - %8d matches\n" "$3" $(($diff_s / 60)) $(($diff_s % 60)) $((diff_ns % 1000)) $count

return $ret
