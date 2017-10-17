#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "ubuntu:12.04 nvidia-smi" {
    run_nvidia_smi "ubuntu:12.04"
}

@test "ubuntu:14.04 nvidia-smi" {
    run_nvidia_smi "ubuntu:14.04"
}

@test "ubuntu:16.04 nvidia-smi" {
    run_nvidia_smi "ubuntu:16.04"
}

@test "ubuntu:16.10 nvidia-smi" {
    run_nvidia_smi "ubuntu:16.10"
}

@test "ubuntu:17.04 nvidia-smi" {
    run_nvidia_smi "ubuntu:17.04"
}
