#!/bin/bash
yum -y update
yum -y groupinstall "GNOME Desktop" "Development Tools"
yum -y install kernel-devel epel-release dkms wget git

wget http://us.download.nvidia.com/tesla/384.81/nvidia-diag-driver-local-repo-rhel7-384.81-1.0-1.x86_64.rpm
rpm -i nvidia-diag-driver-local-repo-rhel7-384.81-1.0-1.x86_64.rpm
yum clean all
yum -y install cuda-drivers
