#!/bin/sh

# Generates a large file. Concatenates the content of each file that matches
# the given pattern in the second parameter.
# Optionally, specific lines can be injected with an optional fourth text file
# argument.
#
# Parameters: $1: The number of rounds to run (each file will be copied this
#                 many times)
#             $2: A pattern that may match multiple files.
#             $3: The final file to write the output into.
#             $4: Optional, a single file with lines to inject into the output.

# Check arguments
if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    printf "> ERROR: Invalid number of arguments! See source for more help!\n"
    exit 2
fi

# Give names to numbered arguments
rounds=$1
pattern=$2
output=$3
extras=$4 # Optional

printf "> Generating input text file..."

# Clear output file
> $output


if [ $# -eq 4 ]; then
    j=1
    lines=`wc -l < $extras`
fi

# Exactly $round times:
i=0
while [ $i -le $rounds ]; do
    # Concatenate each file
    for file in `find $pattern`; do
        cat $file >> $output

        # Inject line from optional argument
        if [ $# -eq 4 ]; then
            sed -n "${j}p" $extras >> $output
            j=$(( $j % $lines + 1 ))
        fi
    done
    # Increase loop variable
    i=$(($i+1))
done

printf " Done.\n"
