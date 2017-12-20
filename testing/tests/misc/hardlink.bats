#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

# Test for https://github.com/NVIDIA/libnvidia-container/issues/9
@test "hard link: /usr/bin/find -> /usr/myfind" {
    docker_run --rm --runtime=nvidia nvidia/cuda:9.0-base ln /usr/bin/find /usr/myfind
    [ "$status" -eq 0 ]
}
