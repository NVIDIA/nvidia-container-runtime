#!/bin/bash

set -xe

CUDA_PKG="cuda-repo-rhel7-10.1.168-1.x86_64.rpm"

yum -y update
yum -y install kernel-devel epel-release dkms wget git

wget https://developer.download.nvidia.com/compute/cuda/repos/rhel7/x86_64/"${CUDA_PKG}"
rpm -i "${CUDA_PKG}"
yum clean all
yum -y install cuda-drivers
