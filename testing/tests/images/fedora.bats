#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "fedora:20 nvidia-smi" {
    run_nvidia_smi "fedora:20"
}

@test "fedora:21 nvidia-smi" {
    run_nvidia_smi "fedora:21"
}

@test "fedora:22 nvidia-smi" {
    run_nvidia_smi "fedora:22"
}

@test "fedora:23 nvidia-smi" {
    run_nvidia_smi "fedora:23"
}

@test "fedora:24 nvidia-smi" {
    run_nvidia_smi "fedora:24"
}

@test "fedora:rawhide nvidia-smi" {
    run_nvidia_smi "fedora:rawhide"
}
