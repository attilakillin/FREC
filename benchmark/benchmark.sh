#!/bin/sh

# Runs make clean and make all in the specified folder.
# Parameters: $1: The folder to run make in.
# Returns: The return value of the make all command.
make_executables() {
    printf "> Cleaning and rebuilding executables..."

    (cd $1 && make clean && make all) 1> /dev/null 2> /dev/null
    ret=$?

    if [ $ret -eq 0 ]; then printf " Done.\n"; else printf " FAILED!\n"; fi

    return $ret
}

# Generates a large file. Concatenates the content of each file that matches
# the given pattern in the first parameter, and inserts a line from the second
# parameter file after each file's content.
# Parameters: $1: A pattern that may match multiple files.
#             $2: A single file with lines to insert into the output file.
#             $3: The number of rounds to run (each file from $1 will be copied
#                 this many times)
#             $4: The final file to write the output into.
generate_file() {
    printf "> Generating input text file..."
    > $4

    j=1
    lines=`wc -l < $2`

    i=0
    end=$3
    while [ $i -le $end ]; do
        for f in `find $1`; do
            cat $f >> $4
            sed -n "${j}p" $2 >> $4
        done
        i=$(($i+1))
        j=$(( $j % $lines + 1 ))
    done

    printf " Done.\n"
}

# Runs the specified command and prints its run time in minutes and seconds.
# Parameters: $1: The command(s) to run.
#             $2: The file to write the output to.
#             $3: The prefix string to write in front of the run time.
# Returns: The return value of the command.
time_run() {
    st_date=`date +"%s"`

    eval $1 1> $2 2> $2
    ret=$?

    en_date=`date +"%s"`

    diff=$(($en_date-$st_date))
	printf "    %-8s - %2dm%02ds\n" $3 $(($diff / 60)) $(($diff % 60))

    return $ret
}

# Compares the output of the frec variant to the other two.
# The given filename will be extended by -frec, -posix, or -tre
# depending on the variant.
# Parameters: $1: The base filename of the output files.
# Returns: The return value of the cmp commands.
compare_outputs() {
    printf "  Comparing outputs..."
    
    cmp "$1" "$2" || cmp "$1" "$3"
    ret=$?

    if [ $ret -eq 0 ]; then printf " Done.\n"; else printf " FAILED!\n"; fi

    return $ret
}

# Benchmark all three variants with the given pattern and text.
# Two base filenames have to supplied, these will be extended by -frec,
# -posix, or -tre depending on the variant.
# Parameters: $1: The base filename of the grep executables.
#             $2: The pattern to use for the search.
#             $3: The file to search in.
#             $4: The base filename where the output will be written.
# Returns: The return value of the compare_outputs command.
benchmark_with() {
    echo "> Running benchmarks... | \"$2\" | $3 |"

    for var in frec posix tre; do
        time_run "$1-$var -e \"$2\" \"$3\"" $4 $var

        if [ "$4" != "/dev/null" ]; then
            mv $4 "$4-$var"
        fi
    done

    compare_outputs "$4-frec" "$4-posix" "$4-tre"
}

# Benchmark all three variants with patterns read from the given file.
# The base executable filename will be extended by -frec, -posix, or -tre
# depending on the variant.
# Parameters: $1: The base filename of the grep executables.
#             $2: The pattern file to read line-by-line for benchmarking.
#             $3: The text file to search in.
#             $4: The folder where the output files will be placed.
benchmark_from_file() {
    echo "> Reading test input file for benchmarks..."

    i=0
    success=0

    while read -r line; do
        benchmark_with "$1" "$line" "$3" "$4/out-$i"

        if [ $? -eq 0 ]; then
            success=$(($success+1))
        fi

        i=$(($i+1))
    done < "$2"

    echo "> Benchmarking finished, outputs matched for $success/$i tests."
}



while getopts "gm" opt; do
    case $opt in
        g)
            # Concatenate the source files of the library 500 times.
            generate_file "../libfrec/lib/*.c" ./extra-patterns.txt 500 ./generated
            ;;
        m)
            # Rebuild variants in the wrappers folder.
            make_executables ./wrappers
            ;;
    esac
done

# Run each variant with specific patterns on the above file.
benchmark_from_file ./wrappers/grep ./test-inputs.txt ./generated ./work
