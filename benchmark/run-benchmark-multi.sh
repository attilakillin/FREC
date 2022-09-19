#!/bin/sh

# Runs multi-pattern benchmarks with the specified executable, and with the
# system-supplied grep command. The latter must be present for this to work!
#
# Parameters: $1: The base filename of the custom grep executable.
#             $2: The pattern file to read line-by-line for benchmarking.
#                 Each line can contain a comma-separated list of patterns.
#             $3: The text file to search each pattern line in.
#             $4: The folder where the output files will be placed.

# Check arguments
if [ $# -ne 4 ]; then
    printf "> ERROR: Invalid number of arguments! See source for more help!\n"
    exit 2
fi

exec_base=$1
patterns=$2
text=$3
out_folder=$4

# Benchmark the given executable with the given pattern and text.
# Parameters: $1: The comma-separated list of patterns to use for the search.
#             $2: The base filename where the output will be written.
# Returns: The return value of the compare_outputs command.
benchmark_with() {
    echo "> Running benchmarks... | \"$1\" | $text |"

    # Replace the beginning and the end of the line, and every comma inbetween.
    # Meaning: '^' to '-e "' and '$' to '"' and every ',' to '" -e "'
    queries=`echo $1 | sed 's/^/-e "/;s/$/"/;s/,/" -e "/g'`

    ./time-run.sh "$exec_base $queries \"$text\" | awk '{ print length }'" "$2-custom" "custom"
    ./time-run.sh "grep $queries \"$text\" | awk '{ print length }'" "$2-system" "system"
}

# Execution starts here
echo "> Reading test input file for benchmarks..."

i=1
# Read each pattern from the patterns file
while read -r line; do
    # And run a benchmark with it
    benchmark_with "$line" "$out_folder/out-$i"
    i=$(($i+1))
done < "$patterns"

echo "> Benchmarking finished, outputs written to $out_folder.\n"
