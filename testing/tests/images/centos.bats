#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "centos:5 nvidia-smi" {
    run_nvidia_smi "centos:5"
}

@test "centos:6 nvidia-smi" {
    run_nvidia_smi "centos:6"
}

@test "centos:7 nvidia-smi" {
    run_nvidia_smi "centos:7"
}
