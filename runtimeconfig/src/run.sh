#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

set -euxo pipefail
shopt -s lastpipe

readonly RUN_DIR="/run/nvidia"
readonly TOOLKIT_DIR="${RUN_DIR}/toolkit"

readonly basedir="$(dirname "$(realpath "$0")")"

source "${basedir}/common.sh"
source "${basedir}/toolkit.sh"
source "${basedir}/docker.sh"

main() {
	local -r destination="${1:-"${RUN_DIR}"}/toolkit"
	local -r docker_socket="${2:-"/var/run/docker.socket"}"

	toolkit::setup "${destination}"
	docker::setup "${destination}" "${docker_socket}"
	echo "docker info: $(docker::info "${docker_socket}")"

	echo "Done, now waiting for signal"
	sleep infinity &

	# shellcheck disable=SC2064
	# We want the expand to happen now rather than at trap time
	# Setup a new signal handler and reset the EXIT signal handler
	trap "echo 'Caught signal'; toolkit::uninstall && { kill $!; exit 0; }" HUP INT QUIT PIPE TERM
	trap - EXIT
	while true; do wait $! || continue; done
	exit 0
}

main "$@"
