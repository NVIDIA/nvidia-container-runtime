#!/usr/bin/env bats

load ../helpers

image="ubuntu:16.04"
envCUDAVersion="NVIDIA_CUDA_VERSION"
envNVGPU="NVIDIA_VISIBLE_DEVICES"

function setup() {
	 check_runtime
}

@test "NVIDIA_CUDA_VERSION=10.5" {
      docker_run --runtime=nvidia -e $envCUDAVersion=10.5 -e $envNVGPU=0 $image
      [ "$status" -ne 0 ]
}

@test "NVIDIA_CUDA_VERSION=8.0" {
      docker_run --runtime=nvidia -e $envCUDAVersion=8.0 -e $envNVGPU=0 $image nvidia-smi
      [ "$status" -eq 0 ]
}
