#!/bin/bash

# HACK: Wait for /var/lib/lock acquired by another apt process
sleep 180

# install nvidia-docker2
curl -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
tee /etc/apt/sources.list.d/nvidia-docker.list <<< \
"deb https://nvidia.github.io/libnvidia-container/ubuntu16.04/amd64 /
deb https://nvidia.github.io/nvidia-container-runtime/ubuntu16.04/amd64 /
deb https://nvidia.github.io/nvidia-docker/ubuntu16.04/amd64 /"
apt-get update -y
apt-get install -y nvidia-docker2
pkill -SIGHUP dockerd
apt-get update -y
