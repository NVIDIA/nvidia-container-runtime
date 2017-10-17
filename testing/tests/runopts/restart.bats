#!/usr/bin/env bats

load ../helpers

image="qa-restart"

function setup() {
    check_runtime
}

function teardown() {
    docker rm -f "$image" || true
}

@test "docker restart" {
    docker run --name "$image" -d --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
    exitcode=$(docker inspect -f '{{ .State.ExitCode }}' "$image")
    [ "$exitcode" -eq 0 ]
    docker restart "$image"
    exitcode=$(docker inspect -f '{{ .State.ExitCode }}' "$image")
    [ "$exitcode" -eq 0 ]
}
