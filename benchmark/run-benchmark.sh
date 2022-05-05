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

# Runs the specified command and prints its run time in minutes and seconds.
# Parameters: $1: The command(s) to run.
#             $2: The file to write the output to.
#             $3: The prefix string to write in front of the run time.
# Returns: The return value of the command.
time_run() {
    st_date=`date +"%s"`

    eval $1 1> $2
    ret=$?

    en_date=`date +"%s"`

    diff=$(($en_date-$st_date))
	printf "    %-8s - %2dm%02ds\n" $3 $(($diff / 60)) $(($diff % 60))

    return $ret
}

# Benchmark specific flavors with the given pattern and text.
# Two base filenames have to supplied, these will be extended by -frec,
# -posix, or -tre depending on the variant.
# Parameters: $1: The base filename of the grep executables.
#             $2: The pattern to use for the search.
#             $3: The file to search in.
#             $4: The base filename where the output will be written.
#             $5: The flavors to use.
# Returns: The return value of the compare_outputs command.
benchmark_with() {
    echo "> Running benchmarks... | \"$2\" | $3 |"

    for var in $5; do
        time_run "$1-$var -e \"$2\" \"$3\"" $4 $var

        if [ "$4" != "/dev/null" ]; then
            mv $4 "$4-$var"
        fi
    done
}

# Execution starts here
echo "> Reading test input file for benchmarks..."

i=1
# Read each pattern from the patterns file
while read -r line; do
    # And run a benchmark with it
    benchmark_with "$exec_base" "$line" "$text" "$out_folder/out-$i" "$flavors"
    i=$(($i+1))
done < "$patterns"

echo "> Benchmarking finished, outputs written to $out_folder.\n"
