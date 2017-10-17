#!/bin/bash

# run tests one by one in the remote tests directory.

for dir in tests/*; do
    if [ -d "$dir" ]; then
	for file in "$dir"/*; do
	    /usr/local/bin/bats "$file"
	done
	find "$dir" -name "Dockerfile" -exec rm -f {} \;
    fi
done
