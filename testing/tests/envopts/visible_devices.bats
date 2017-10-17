#!/usr/bin/env bats

load ../helpers

image="ubuntu:16.04"
envNVGPU="NVIDIA_VISIBLE_DEVICES"

function setup() {
    check_runtime
    docker pull $image >/dev/null 2>&1 || true
}

@test "no device " {
      docker_run --rm --runtime=nvidia -e $envNVGPU=none $image nvidia-smi
      [ "$status" -ne 0 ]
}

@test "device 0" {
      docker_run --rm --runtime=nvidia -e $envNVGPU=0 $image nvidia-smi -L
      [ "$status" -eq 0 ]
}

@test "devices all" {
      docker_run --rm --runtime=nvidia -e $envNVGPU=all $image nvidia-smi
      [ "$status" -eq 0 ]
}

@test "no envNVGPU" {
      docker_run --rm --runtime=nvidia $image nvidia-smi
      [ "$status" -ne 1 ]
}
