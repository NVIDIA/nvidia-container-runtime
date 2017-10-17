#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "bvlc/caffe time" {
    docker_run --rm --runtime=nvidia bvlc/caffe:gpu caffe time --model=/opt/caffe/models/bvlc_googlenet/deploy.prototxt --gpu=0
    [ "$status" -eq 0 ]
}

@test "nvidia/caffe device_query" {
    docker_run --rm --runtime=nvidia nvidia/caffe caffe device_query --gpu=all
    [ "$status" -eq 0 ]
}
