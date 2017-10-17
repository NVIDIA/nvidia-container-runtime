#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "nginx nvidia-smi" {
    run_nvidia_smi "nginx"
}

@test "redis nvidia-smi" {
    run_nvidia_smi "redis"
}

@test "mysql nvidia-smi" {
    run_nvidia_smi "mysql"
}

@test "logstash nvidia-smi" {
    run_nvidia_smi "logstash"
}
