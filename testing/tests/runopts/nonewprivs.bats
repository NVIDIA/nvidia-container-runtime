#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "docker run --security-opt=no-new-privileges" {
    docker_run --security-opt=no-new-privileges --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}
