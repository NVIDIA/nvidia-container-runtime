#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

set -eEux

readonly dind_name="nvidia-container-runtime-installer"

# TODO move rm -rf shared to cleanup
testing::cleanup() {
	docker run -it --privileged -v "${shared_dir}:/shared" alpine:latest chmod -R 777 /shared
	rm -rf "${shared_dir}" || true

	docker kill "${dind_name}" || true &> /dev/null
	docker rm "${dind_name}" || true &> /dev/null

	return
}

testing::setup() {
	mkdir -p "${shared_dir}"
	mkdir -p "${shared_dir}"/etc/docker
	mkdir -p "${shared_dir}"/run/nvidia
	mkdir -p "${shared_dir}"/etc/nvidia-container-runtime
}

testing::main() {
	local image="${1:-"nvidia/container-toolkit:docker19.03"}"

	testing::setup

	# Docker creates /etc/docker when starting
	# by default there isn't any config in this directory (even after the daemon starts)
	docker run --privileged \
		-v "${shared_dir}/etc/docker:/etc/docker" \
		-v "${shared_dir}/run/nvidia:/run/nvidia:shared" \
		--name "${dind_name}" -d docker:stable-dind -H unix://run/nvidia/docker.sock

	# Share the volumes so that we can edit the config file and point to the new runtime
	# Share the pid so that we can ask docker to reload its config
	docker run -it --privileged \
		--volumes-from "${dind_name}" \
		--pid "container:${dind_name}" \
		"${image}" \
		bash -x -c "/work/run.sh /run/nvidia /run/nvidia/docker.sock"

	testing::cleanup
}

readonly shared_dir="${1:-"./shared"}"
shift

trap testing::cleanup ERR

testing::cleanup
testing::main "$@"
