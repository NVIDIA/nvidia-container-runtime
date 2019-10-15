#! /bin/bash
# Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euxo pipefail
shopt -s lastpipe

readonly RUN_DIR="/run/nvidia"
readonly TOOLKIT_DIR="${RUN_DIR}/toolkit"
readonly PID_FILE="${RUN_DIR}/toolkit.pid"

readonly basedir="$(dirname "$(realpath "$0")")"

source "${basedir}/common.sh"
source "${basedir}/toolkit.sh"
source "${basedir}/docker.sh"

UNINSTALL=0
DAEMON=0

_shutdown() {
	rm -f ${PID_FILE}
}

main() {
	local -r destination="${1:-"${RUN_DIR}"}/toolkit"
	local -r docker_socket="${2:-"/var/run/docker.socket"}"

	shift 2

	while [ $# -gt 0 ]; do
		case "$1" in
		--no-uninstall)
			UNINSTALL=1
			shift
			;;
		--no-daemon)
			DAEMON=1
			shift
			;;
		*)
			echo "Unknown argument $@"
			exit 1
		esac
	done

	log INFO "=================Starting the NVIDIA Container Toolkit================="

	# Todo this probably just needs us to wait
	exec 3> ${PID_FILE}
	if ! flock -n 3; then
		echo "An instance of the NVIDIA toolkit Container is already running, aborting"
		exit 1
	fi
	echo $$ >&3

	trap "_shutdown" EXIT
	toolkit::uninstall "${destination}" || exit 1

	toolkit::setup "${destination}"
	docker::setup "${destination}" "${docker_socket}"
	echo "docker info: $(docker::info "${docker_socket}")"

	# Unexpose the toolkit from the host
	# Restart docker so that it picks up the new config
	local uninstall_instructions="docker::uninstall ${docker_socket}"
	if [[ "${UNINSTALL}" -ne 0 ]]; then
		uninstall_instructions="true"
	fi

	if [[ "$DAEMON" -ne 0 ]]; then
		# clear other handlers
		trap - EXIT
		eval "${uninstall_instructions}"
		exit 0
	fi

	echo "Done, now waiting for signal"
	sleep infinity &

	# shellcheck disable=SC2064
	# We want the expand to happen now rather than at trap time
	# Setup a new signal handler and reset the EXIT signal handler
	trap "echo 'Caught signal'; ${uninstall_instructions} && _shutdown && { kill $!; exit 0; }" HUP INT QUIT PIPE TERM
	trap - EXIT
	while true; do wait $! || continue; done
	exit 0
}

main "$@"
