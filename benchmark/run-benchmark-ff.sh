#!/bin/sh

# Runs benchmarks with the specified flavors. Perfect for testing the "ff",
# i.e. the "find first" flavors.
#
# Parameters: $1: The base filename of the grep executables (will be extended
#                 by a dash and the name of the flavors,
#                 e.g.: grep -> grep-frec, grep-posix, grep-tre).
#             $2: The pattern file to read line-by-line for benchmarking.
#                 Each line can contain a comma-separated list of patterns.
#             $3: The text file to search each pattern line in.
#             $4: The folder where the output files will be placed.
#             $5: The flavors to use for multi-pattern matching, e.g.:
#                 frec-multi, posix, tre. Separate by comma.

# Check arguments
if [ $# -ne 5 ]; then
    printf "> ERROR: Invalid number of arguments! See source for more help!\n"
    exit 2
fi

exec_base=$1
patterns=$2
text=$3
out_folder=$4
flavors=`echo $5 | sed "s/,/ /g"`

# Benchmark specific flavors with the given pattern and text.
# Two base filenames have to supplied, these will be extended by -frec,
# -posix, or -tre depending on the variant.
# Parameters: $1: The pattern to use for the search.
#             $2: The base filename where the output will be written.
#             $3: The multi-pattern flavors to use.
# Returns: The return value of the compare_outputs command.
benchmark_with() {
    echo "> Running benchmarks... | \"$1\" | $text |"

    # Multi pattern matching:

    # Replace the beginning and the end of the line, and every comma inbetween.
    # Meaning: '^' to '-e "' and '$' to '"' and every ',' to '" -e "'
    patterns_template=`echo $1 | sed 's/^/-e "/;s/$/"/;s/,/" -e "/g'`

    for var in $3; do
        ./time-run.sh "$exec_base-$var $patterns_template \"$text\"" $2 "$var"
    done
}

# Execution starts here
echo "> Reading test input file for benchmarks..."

i=1
# Read each pattern from the patterns file
while read -r line; do
    # And run a benchmark with it
    benchmark_with "$line" "$out_folder/out-$i" "$flavors" "$flavors"
    i=$(($i+1))
done < "$patterns"

echo "> Benchmarking finished, outputs written to $out_folder.\n"
