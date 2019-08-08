#!/bin/bash

# exit code 0 if all test cases pass
exit_code=0

# run tests one by one in the remote tests directory.
for dir in tests/*; do
    if [ -d "$dir" ]; then
	for file in "$dir"/*; do
	    /usr/local/bin/bats "$file"
	    exit_code=$(( $?>0 ? $? : exit_code))
	done
	find "$dir" -name "Dockerfile" -exec rm -f {} \;
    fi
done

exit ${exit_code}
