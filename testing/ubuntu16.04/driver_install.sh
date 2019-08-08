#!/bin/bash

set -xe

CUDA_PKG="cuda-repo-ubuntu1604_10.1.168-1_amd64.deb"

apt-get install -y wget
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1604/x86_64/"${CUDA_PKG}"
dpkg -i "${CUDA_PKG}"
apt-key adv --fetch-keys http://developer.download.nvidia.com/compute/cuda/repos/ubuntu1604/x86_64/7fa2af80.pub
apt-get update
apt-get install -y cuda-drivers
