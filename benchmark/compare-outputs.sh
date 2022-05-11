#!/bin/sh

# Compares output files generated by run-benchmark.sh.
#
# Parameters: $1 The folder that contains the output files.
#             $2 The original pattern file that was used for benchmarking.
#             $3 The comma-separated list of flavors to compare.


# Check arguments
if [ $# -ne 3 ]; then
    printf "> ERROR: Invalid number of arguments! See source for more help!\n"
    exit 2
fi

out_folder=$1
patterns=$2
flavors=`echo $3 | sed "s/,/ /g"`

printf "> Comparing outputs...\n"

i=1
success=0

# Read each pattern from the patterns file
while read -r line; do
    files=`echo $flavors | awk '{ for(j=1;j<=NF;j++) $j="./work/out-'$i'-"$j; print }'`
    diff -q --from-file $files 1> /dev/null

    # Maintain number of successes
    if [ $? -eq 0 ]; then
        success=$(($success+1))
    else
        printf "    Difference found for pattern: \"$line\" (index: $i)\n"
    fi

    i=$(($i+1))
done < "$patterns"

printf "  Done. Outputs matched in $success/$(($i-1)) benchmark runs!\n"