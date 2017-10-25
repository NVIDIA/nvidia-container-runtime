#!/usr/bin/env bats

load ../helpers

image="ubuntu:16.04"
envRequireCUDA="NVIDIA_REQUIRE_CUDA"
envNVGPU="NVIDIA_VISIBLE_DEVICES"

function setup() {
	 check_runtime
}

@test "NVIDIA_CUDA_VERSION=10.5" {
      docker_run --runtime=nvidia -e $envRequireCUDA="cuda>=10.5" -e $envNVGPU=0 $image
      [ "$status" -ne 0 ]
}

@test "NVIDIA_CUDA_VERSION=8.0" {
      docker_run --runtime=nvidia -e $envRequireCUDA="cuda>=8.0" -e $envNVGPU=0 $image nvidia-smi
      [ "$status" -eq 0 ]
}
