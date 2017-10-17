#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "pid namespace: host" {
    skip_if_userns
    docker_run --pid=host --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}

@test "net namespace: host" {
    skip_if_userns
    docker_run --network=host --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}

@test "net namespace: none" {
    docker_run --network=none --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}
