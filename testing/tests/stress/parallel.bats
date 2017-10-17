#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

COUNT=100

@test "${COUNT}x centos:7" {
    for i in `seq 1 $COUNT`; do echo $i; done | xargs -I{} -n1 -P8 docker run --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all centos:7 nvidia-smi >/dev/null
}

@test "${COUNT}x ubuntu:16.04" {
    for i in `seq 1 $COUNT`; do echo $i; done | xargs -I{} -n1 -P8 docker run --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all ubuntu:16.04 nvidia-smi >/dev/null
}
