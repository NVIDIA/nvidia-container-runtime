#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "tensorflow MNIST" {
    docker_run --rm --runtime=nvidia tensorflow/tensorflow:1.2.1-devel-gpu python /tensorflow/tensorflow/examples/tutorials/mnist/mnist_with_summaries.py
    [ "$status" -eq 0 ]

    echo "$output" | grep -E -q "/gpu:0"
}
