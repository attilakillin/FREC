#!/bin/sh

# Runs make clean and make all in the specified folder.
#
# Parameters: $1: The folder to run make in.
# Returns: The return value of the make all command.

# Check arguments
if [ $# -ne 1 ]; then
    printf "> ERROR: Specify a folder to build executables in!\n"
    exit 2
fi

# Give names to numbered arguments
folder=$1

printf "> Cleaning and rebuilding executables..."

# Execute make commands
(cd $folder && make clean && make all) 1> /dev/null 2> /dev/null
ret=$?

# Finish message based on return value
if [ $ret -eq 0 ]; then
    printf " Done.\n"
else
    printf " FAILED!\n"
fi

return $ret
