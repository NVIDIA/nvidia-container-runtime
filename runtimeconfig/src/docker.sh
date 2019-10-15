#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

readonly DOCKER_CONFIG="/etc/docker/daemon.json"

docker::info() {
	local -r docker_socket="${1:-/var/run/docker.sock}"
	curl --unix-socket "${docker_socket}" 'http://v1.40/info' | jq -r '.Runtimes.nvidia.path'
}

docker::ensure::mounted() {
	mount | grep /etc/docker
	if [[ ! $? ]]; then
		log ERROR "Docker directory isn't mounted in container"
		log ERROR "Ensure that you have correctly mounted the docker directoy"
		exit 1
	fi
}

docker::ensure::config_dir() {
	# Ensure that the docker config path exists
	if [[ ! -d "/etc/docker" ]]; then
		log ERROR "Docker directory doesn't exist in container"
		log ERROR "Ensure that you have correctly mounted the docker directoy"
		exit 1
	fi

}

docker::config::backup() {
	if [[ -f "${DOCKER_CONFIG}" ]]; then
		mv "${DOCKER_CONFIG}" "${DOCKER_CONFIG}.bak"
	fi
}

docker::config::restore() {
	if [[ -f "${DOCKER_CONFIG}.bak" ]]; then
		mv "${DOCKER_CONFIG}.bak" "${DOCKER_CONFIG}"
	else
		if [[ -f "${DOCKER_CONFIG}" ]]; then
			rm "${DOCKER_CONFIG}"
		fi
	fi
}

docker::config::add_runtime() {
	local -r destination="${1:-/run/nvidia}"
	local -r nvcr="${destination}/nvidia-container-runtime"

	cat - | \
		jq -r ".runtimes = {}" | \
		jq -r ".runtimes += {\"nvidia\": {\"path\": \"${nvcr}\"}}" | \
		jq -r '. += {"default-runtime": "nvidia"}'
}

docker::config() {
	([[ -f "${DOCKER_CONFIG}" ]] && cat "${DOCKER_CONFIG}") || echo {}
}

docker::config::refresh() {
	log INFO "Refreshing the docker daemon configuration"
	pkill -SIGHUP dockerd
}

docker::config::restart() {
	log INFO "restarting the docker daemon"
	pkill -SIGTERM dockerd
}

docker::config::get_nvidia_runtime() {
	cat - | jq -r '.runtimes | keys[0]'
}

docker::setup() {
	docker::ensure::mounted
	docker::ensure::config_dir

	log INFO "Setting up the configuration for the docker daemon"

	local -r destination="${1:-/run/nvidia}"
	local -r docker_socket="${2:-"/var/run/docker.socket"}"

	local config=$(docker::config)
	log INFO "current config: ${config}"

	local -r nvidia_runtime="$(with_retry 5 2s docker::info "${docker_socket}")"

	# This is a no-op
	if [[ "${nvidia_runtime}" = "${destination}/nvidia-container-runtime" ]]; then
		log INFO "Noop, docker is arlready setup with the runtime container"
		return
	fi

	# Append the nvidia runtime to the docker daemon's configuration
	local updated_config=$(echo "${config}" | docker::config::add_runtime "${destination}")
	local -r config_runtime=$(echo "${updated_config}" | docker::config::get_nvidia_runtime)

	# If there was an error while parsing the file catch it here
	if [[ "${config_runtime}" != "nvidia" ]]; then
		config=$(echo "{}" | docker::config::add_runtime "${destination}")
	fi

	docker::config::backup
	echo "${updated_config}" > /etc/docker/daemon.json

	log INFO "after: $(docker::config | jq .)"
	docker::config::refresh
}

docker::uninstall() {
	local -r docker_socket="${1:-"/var/run/docker.socket"}"
	docker::config::restore
}
