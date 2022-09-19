#!/bin/sh

# Runs benchmarks with the specified flavors.
#
# Parameters: $1: The base filename of the grep executables (will be extended
#                 by a dash and the name of the flavors,
#                 e.g.: grep -> grep-frec, grep-posix, grep-tre).
#             $2: The pattern file to read line-by-line for benchmarking.
#             $3: The text file to search each pattern line in.
#             $4: The folder where the output files will be placed.
#             $5: The comma-separated list of flavors to benchmark on,
#                 e.g.: frec,posix,tre.

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
#             $3: The flavors to use.
# Returns: The return value of the compare_outputs command.
benchmark_with() {
    echo "> Running benchmarks... | \"$1\" | $text |"

    for var in $3; do
        ./time-run.sh "$exec_base-$var -e \"$1\" \"$text\"" $2 $var

        if [ "$2" != "/dev/null" ]; then
            mv $2 "$2-$var"
        fi
    done
}

# Execution starts here
echo "> Reading test input file for benchmarks..."

i=1
# Read each pattern from the patterns file
while read -r line; do
    # And run a benchmark with it
    benchmark_with "$line" "$out_folder/out-$i" "$flavors"
    i=$(($i+1))
done < "$patterns"

echo "> Benchmarking finished, outputs written to $out_folder.\n"
