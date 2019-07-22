#! /bin/bash

set -ex


find . \( -path ./.git -o -path ./license \)  -prune -o -print -type f | \
	grep -i -E "license|copying" > /tmp/LICENSES.list

for path in `cat /tmp/LICENSES.list`; do
	# Print the license for logging purposes
	licensee detect ${path} | tee -a /tmp/LICENSE.full

	# Ensure the format is of the form "${path} ${license}"
	license=$(licensee detect ${path} | \
		grep '^License:' | tr -s ' ' | awk '{print $2}')
	confidence=$(licensee detect ${path} | \
		tr -s ' ' | grep -E '^ Confidence:' | awk '{print $2}')

	echo "Inspecting License ${license} (${confidence}) at ${path}" | tee -a /tmp/LICENSE.result
	if grep -Fxq "${license}" ./license/whitelist.txt; then
		continue
	fi

	echo "Found unexpected license at ${path}" | tee -a /tmp/LICENSE.result

	if grep -Fq "${path}" ./license/whitelist.txt; then
		echo -e "There was a manual inspection for this license\n" | tee -a /tmp/LICENSE.result
		continue
	fi

	cat /tmp/LICENSE.result
	exit 1
done

cat /tmp/LICENSE.result
