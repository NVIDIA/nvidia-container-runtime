#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "debian:wheezy nvidia-smi" {
    run_nvidia_smi "debian:wheezy"
}

@test "debian:jessie nvidia-smi" {
    run_nvidia_smi "debian:jessie"
}

@test "debian:stretch nvidia-smi" {
    run_nvidia_smi "debian:stretch"
}
