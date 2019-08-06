#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

set -euxo pipefail
shopt -s lastpipe

source "common.sh"
source "docker.sh"

install_nvidia_container_runtime_toolkit() {
	log INFO "Installing the NVIDIA Container Runtime Toolkit"

	local -r destination="${1:-/run/nvidia}"
	local -a packages=("/usr/bin/nvidia-container-runtime" \
		"/usr/bin/nvidia-container-toolkit" \
		"/usr/bin/nvidia-container-cli" \
		"/etc/nvidia-container-runtime/config.toml" \
		"/usr/lib/x86_64-linux-gnu/libnvidia-container.so.1")

	# TODO workaround until we fix the runtime requiring this
	# directory and file to exist at that location
	cp ./config.toml /etc/nvidia-container-runtime

	# Bash variables starts at 0
	# ZSH variables starts at 1
	for ((i=0; i < ${#packages[@]}; i++)); do
		packages[$i]=$(readlink -f ${packages[$i]})
	done

	if [[ ! -d "${destination}" ]]; then
		log ERROR "Destination directory doesn't exist in container"
		log ERROR "Ensure that you have correctly mounted the destination directoy"
		exit 1
	fi

	cp "${packages[@]}" "${destination}"

	# Setup links to the real binaries to ensure that variables and configs
	# are pointing to the right path
	mv "${destination}/nvidia-container-toolkit" \
		"${destination}/nvidia-container-toolkit.real"
	mv "${destination}/nvidia-container-runtime" \
		"${destination}/nvidia-container-runtime.real"


	# Setup aliases so as to ensure that the path is correctly set
	cat <<- EOF > ${destination}/nvidia-container-toolkit
		#! /bin/sh
		LD_LIBRARY_PATH="${destination}" \
		PATH="\$PATH:${destination}" \
		${destination}/nvidia-container-toolkit.real \
			-config "${destination}/config.toml" \
			\$@
	EOF

	cat <<- EOF > ${destination}/nvidia-container-runtime
		#! /bin/sh
		LD_LIBRARY_PATH="${destination}" \
		PATH="\$PATH:${destination}" \
		${destination}/nvidia-container-runtime.real \
			\$@
	EOF

	# Make sure that the alias files are executable
	chmod +x "${destination}/nvidia-container-toolkit"
	chmod +x "${destination}/nvidia-container-runtime"
}

main() {
	local -r destination="${1:-/run/nvidia}"
	local -r docker_socket="${2:-/var/run/docker.socket}"
	local -r nvidia_runtime="$(docker::info ${docker_socket})"

	if [[ "${nvidia_runtime}" = "${destination}/nvidia-container-runtime" ]]; then
		exit 0
	fi

	install_nvidia_container_runtime_toolkit "${destination}"
	docker::setup "${destination}"
	echo "docker info: $(docker::info ${docker_socket})"
}

main "$@"
