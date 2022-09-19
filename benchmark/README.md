# Benchmarking guide

This folder contains a few scripts and tools that can be used to benchmark the
performance of different regular expression matcher implementations.

The three flavors that can be compared are the following:

- The standard POSIX implementation available in `<regex.h>`.
- The TRE approximate regex matching library, available from
  [here](https://laurikari.net/tre/).
- This implementation, that was written with certain specific
  optimizations in mind.

## Building executables for each flavor

The `wrappers` folder contains a single C source file and a Makefile.
This C file extensively uses macros in order to work with all three flavors
specified above. The Makefile can build every flavor simultaneously using 
compile-time flags.

The `make-executables.sh` script can be used to build all three flavors using
the following command:

    ./make-executables.sh ./wrappers

The only prerequisites are downloading the TRE library and building the FREC
library from the root folder beforehand.

## Generating or finding sample texts

Sample texts can be found online, but for ease of access, a local script is
also provided that can generate large text files.

### Generating a sample text file

The `generate-text.sh` script can be used to concatenate files matching a given
pattern a fixed number of times. With a high enough number of rounds, the output
file is large enough to be used for benchmarking tests.

The script can be used with 3 or 4 arguments. With 3 arguments, the number of
rounds, the input file pattern and the output file must be specified.
For example:

    ./generate-text.sh 5000 "../libfrec/lib/*.c" "./texts/generated"

This concatenates every C source file of the library 5000 times, which amounts
to a few hundred megabytes of plain text.

An optional 4th argument can be used to specify an "extras" file. After each
concatenation, a line from this file is read and inserted into the output file.

### Finding a large sample text file online

The easiest way to find large files is to look for data dumps. For example, the
[following page](https://dumps.wikimedia.org/enwiki/) contains download dumps
for the entire content of Wikipedia.

For a quicker, smaller download, the index files can be used instead of the
content of each page (such as this:
[enwiki-20220201-pages-articles-multistream-index.txt.bz2](https://dumps.wikimedia.org/enwiki/20220201/enwiki-20220201-pages-articles-multistream-index.txt.bz2)).

## Running benchmarks

The `run-benchmark.sh` script can be used to test specific patterns on specific
text files. The script has five arguments, and can be used as follows:

    ./run-benchmark.sh ./wrappers/grep ./inputs/inputs-generated.txt ./texts/generated ./work frec,tre

The arguments in order:

- The base executable path and name of the executable flavors. Will be extended
  by a dash and the flavor (so that `grep` becomes `grep-frec` or `grep-tre`).
- A pattern file. Each line in this file will be used as a pattern to search for
  in the text file.
- A text file to search in.
- A folder where the output files will be written.
- The comma-separated list of flavors to benchmark.

The output files will contain the start and end offset of each match the flavor
has found, as well as the number of matches that were found.
To compare these automatically, use `compare-outputs.sh`, as seen below.

## Comparing benchmark outputs

For a full benchmark, the outputs can be compared for any differences. If this
implementation works correctly, no difference should occur.

The `compare-outputs.sh` script can be used with three arguments:
- The output folder where the benchmark output was written.
- The original pattern file that was used for benchmarking
  (used for identifying the mismatched pattern, should such pattern exist).
- The comma-separated list of flavors to compare.

Example usage:

    ./compare-outputs.sh ./work ./inputs/inputs-generated.txt frec,tre

Cleaning up the output folder is the user's responsibility, these scripts don't
remove anything during execution. In the above example, this means running
`rm ./work/*`.

## Benchmarking the multi-pattern matcher

The `run-benchmark-multi.sh` script can be used in conjunction with the previous scripts to test the multi-pattern matching algorithm. The script works a bit differently as the single-pattern matcher, as it has no specific flavors. A single run will execute the query on the system-supplied `grep` command, as well as on the executable supplied by the argument.

The `bin` folder in the project root contains an executable built around the multi-pattern matcher, which can be used for testing. An example call:

    ./run-benchmark-multi.sh ../bin/grep/grep ./inputs/inputs-enwiki-multi.txt ./texts/enwiki ./work

The arguments in order:

- The custom executable to test multi-pattern matching on.
- A pattern file. Each line in this file will be used as a pattern to search for
  in the text file.
- A text file to search in.
- A folder where the output files will be written.

The pattern file must be somewhat different. Instead of a single pattern per line, multiple patterns can be supplied, separated by the `,` character.

The output files will contain the start and end offset of each match the executable has found, as well as the number of matches that were found.
To compare these automatically, use `compare-outputs.sh`, as seen above.
