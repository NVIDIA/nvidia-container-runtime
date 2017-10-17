#!/bin/bash

function check_runtime() {
    docker info | grep 'Runtimes:' | grep -q 'nvidia'
}

# Taken from runc tests
function docker_run() {
    run docker run "$@"

    echo "docker run $@ (status=$status):" >&2
    echo "$output" >&2
}

function nvidia_container_runtime_run() {
    run nvidia-container-runtime run "$@"

    echo "nvidia-container-runtime run $@ (status=$status):" >&2
    echo "$output" >&2
}

function run_nvidia_smi() {
    local image=$1
    docker pull $image >/dev/null 2>&1 || true
    docker_run --rm --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all $image nvidia-smi
    [ "$status" -eq 0 ]
}

function skip_if_userns() {
    run sh -c "docker info -f  '{{ .SecurityOptions }}' | grep -q userns"
    if [[ "$status" -eq 0 ]]; then
        skip "user NS enabled."
    fi
}

function skip_if_nouserns() {
    run sh -c "docker info -f  '{{ .SecurityOptions }}' | grep -q userns"
    if [[ "$status" -eq 1 ]]; then
        skip "user NS not enabled."
    fi
}

function skip_if_headless() {
    run pidof X Xorg
    if [[ "$status" -eq 1 ]]; then
        skip "no X server is running"
    fi
}

function skip_if_nonroot() {
    if [[ "$(id -u)" -ne 0 ]]; then
        skip "running as non-root"
    fi
}
