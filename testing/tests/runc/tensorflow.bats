#!/usr/bin/env bats

load ../helpers

function setup() {
    if [[ "$(id -u)" -ne 0 ]]; then
        return
    fi

    run rm -rf containers/tensorflow

    mkdir -p containers/tensorflow/rootfs
    cd containers/tensorflow
    docker export $(docker create tensorflow/tensorflow:1.2.1-devel-gpu) | tar --exclude 'dev/*' -C rootfs -xf -
    nvidia-container-runtime spec
    sed -i 's;"sh";"nvidia-smi", "-L";' config.json
    sed -i 's;\("TERM=xterm"\);\1, "CUDA_VERSION=8.0.61";' config.json
}

function teardown() {
    if [[ "$(id -u)" -ne 0 ]]; then
        return
    fi

    run rm -rf containers/tensorflow
}

@test "nvidia-container-runtime tensorflow nvidia-smi" {
    skip_if_nonroot

    nvidia_container_runtime_run test_tf
    [ "$status" -eq 0 ]
}
