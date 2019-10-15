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

readonly basedir="$(dirname "$(realpath "$0")")"
readonly dind_name="nvidia-container-runtime-dind"

source "${basedir}/../src/common.sh"

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

testing::dind() {
	# Docker creates /etc/docker when starting
	# by default there isn't any config in this directory (even after the daemon starts)
	docker run --privileged \
		-v "${shared_dir}/etc/docker:/etc/docker" \
		-v "${shared_dir}/run/nvidia:/run/nvidia:shared" \
		--name "${dind_name}" -d docker:stable-dind -H unix://run/nvidia/docker.sock
}

testing::dind::alpine() {
	docker exec -it "${dind_name}" sh -c "$*"
}

testing::toolkit() {
	# Share the volumes so that we can edit the config file and point to the new runtime
	# Share the pid so that we can ask docker to reload its config
	docker run -it --privileged \
		--volumes-from "${dind_name}" \
		--pid "container:${dind_name}" \
		"${tool_image}" \
		bash -x -c "/work/run.sh /run/nvidia /run/nvidia/docker.sock $*"
}

testing::main() {
	local -r tool_image="${1:-"nvidia/container-toolkit:docker19.03"}"

	testing::setup

	testing::dind
	testing::toolkit --no-uninstall --no-daemon

	# Ensure that we haven't broken non GPU containers
	testing::dind::alpine docker run -it alpine echo foo

	# Uninstall
	testing::toolkit --no-daemon
	testing::dind::alpine test ! -f /etc/docker/daemon.json

	toolkit_files="$(ls -A "${shared_dir}"/run/nvidia/toolkit)"
	test ! -z "${toolkit_files}"

	testing::cleanup
}

readonly shared_dir="${1:-"./shared"}"
shift 1

trap testing::cleanup ERR

testing::cleanup
testing::main "$@"
