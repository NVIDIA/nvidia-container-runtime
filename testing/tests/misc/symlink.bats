#!/usr/bin/env bats

load ../helpers

function setup() {
    check_runtime
}

@test "relative symlink: /usr/bin -> ../tmp" {
    cat <<EOF > Dockerfile
    FROM ubuntu:16.04

    RUN mv -T /usr/bin /tmp && ln -s ../tmp /usr/bin
EOF
    docker build -t "qa-symlink:relative" .
    # Check the container works without our runtime
    docker run --rm "qa-symlink:relative" /usr/bin/diff --version
    run_nvidia_smi "qa-symlink:relative"
}

@test "absolute symlink: /usr/bin -> /tmp" {
    cat <<EOF > Dockerfile
    FROM ubuntu:16.04

    RUN mv -T /usr/bin /tmp && ln -s /tmp /usr/bin
EOF
    docker build -t "qa-symlink:absolute" .
    # Check the container works without our runtime
    docker run --rm "qa-symlink:absolute" /usr/bin/diff --version
    run_nvidia_smi "qa-symlink:absolute"
}
