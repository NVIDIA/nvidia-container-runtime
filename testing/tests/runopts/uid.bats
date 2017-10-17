#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "docker run -u 1000:1000" {
    docker_run -u 1000:1000 --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
}
