#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

docker::info() {
	local -r docker_socket="${1:-unix:///var/run/docker.socket}"

	# Docker in Docker has a startup race
	for i in $(seq 1 5); do
		# Calling in a subshell so that we can recover from a failure
		if [[ ! $(docker -H "${docker_socket}" info -f '{{json .Runtimes}}') ]]; then
			sleep 2
			continue
		fi

		docker -H "${docker_socket}" info -f '{{json .Runtimes}}' | jq '.nvidia.path'
		return
	done
	exit 1
}

# Echo an empty config if the config file doesn't exist
docker::daemon_config() {
	local -r daemon_file="${1:-"/etc/docker/daemon.json"}"
	([[ -f "${daemon_file}" ]] && cat "${daemon_file}") || echo {}
}

docker::refresh_configuration() {
	log INFO "Refreshing the docker daemon configuration"
	pkill -SIGHUP dockerd
}

docker::update_config_file() {
	local -r destination="${1:-/run/nvidia}"
	local -r nvcr="${destination}/nvidia-container-runtime"

	local config_json
	IFS='' read -r config_json

	echo "${config_json}" | \
		jq -r ".runtimes += {\"nvidia\": {\"path\": \"${nvcr}\"}}" | \
		jq -r '. += {"default-runtime": "nvidia"}'
}

docker::ensure_prerequisites() {
	# Ensure that the docker config path exists
	if [[ ! -d "/etc/docker" ]]; then
		log ERROR "Docker directory doesn't exist in container"
		log ERROR "Ensure that you have correctly mounted the docker directoy"
		exit 1
	fi

	mount | grep /etc/docker
	if [[ ! $? ]]; then
		log ERROR "Docker directory isn't mounted in container"
		log ERROR "Ensure that you have correctly mounted the docker directoy"
		exit 1
	fi
}

docker::setup() {
	local -r destination="${1:-/run/nvidia}"
	log INFO "Setting up the configuration for the docker daemon"

	docker::ensure_prerequisites

	log INFO "current config: $(docker::daemon_config)"

	# Append the nvidia runtime to the docker daemon's configuration
	# We use sponge here because the input file is the output file
	config=$(docker::daemon_config | docker::update_config_file "${destination}")
	echo "${config}" > /etc/docker/daemon.json

	log INFO "after: $(docker::daemon_config | jq .)"
	docker::refresh_configuration
}
