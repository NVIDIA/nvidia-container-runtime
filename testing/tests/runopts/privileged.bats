#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "docker run --privileged" {
    skip_if_userns
    docker_run --privileged --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}
