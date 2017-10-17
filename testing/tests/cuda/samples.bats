#!/usr/bin/env bats

load ../helpers

image="nvidia/cuda:8.0-devel-ubuntu16.04"

function setup() {
    check_runtime
    docker pull $image >/dev/null 2>&1 || true
}

@test "deviceQuery" {
    cat <<EOF > Dockerfile
    FROM $image

    RUN apt-get update && apt-get install -y cuda-samples-8-0
    WORKDIR /usr/local/cuda/samples/1_Utilities/deviceQuery

    RUN make

    CMD ./deviceQuery
EOF
    docker build --pull -t "cuda-samples" .

    docker_run --rm --runtime=nvidia cuda-samples
    [ "$status" -eq 0 ]
}


@test "rmmod nvidia_uvm" {
    skip_if_nonroot

    cat <<EOF > Dockerfile
    FROM $image

    RUN apt-get update && apt-get install -y cuda-samples-8-0
    WORKDIR /usr/local/cuda/samples/1_Utilities/deviceQuery

    RUN make

    CMD ./deviceQuery
EOF
    docker build --pull -t "cuda-samples" .

    grep -q "nvidia_uvm" /proc/modules && rmmod nvidia_uvm
    docker_run --rm --runtime=nvidia cuda-samples
    modprobe nvidia_uvm
    [ "$status" -eq 0 ]
}
