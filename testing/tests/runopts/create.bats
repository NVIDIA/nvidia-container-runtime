#!/usr/bin/env bats

load ../helpers

NAME="qa-create"

function setup() {
    check_runtime
}

function teardown() {
    docker rm -f "$NAME" || true
}

@test "docker start" {
    docker create --name "$NAME" --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi
    docker start "$NAME"
    exitcode=$(docker inspect -f '{{ .State.ExitCode }}' "$NAME")
    [ "$exitcode" -eq 0 ]
}
