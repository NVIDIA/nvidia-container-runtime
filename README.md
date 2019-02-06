# libnvidia-container

[![GitHub license](https://img.shields.io/badge/license-New%20BSD-blue.svg?style=flat-square)](https://raw.githubusercontent.com/NVIDIA/libnvidia-container/master/LICENSE)
[![GitHub release](https://img.shields.io/github/release/NVIDIA/libnvidia-container/all.svg?style=flat-square)](https://github.com/NVIDIA/libnvidia-container/releases)
[![Package repository](https://img.shields.io/badge/packages-repository-b956e8.svg?style=flat-square)](https://nvidia.github.io/libnvidia-container)
[![Travis](https://img.shields.io/travis/NVIDIA/libnvidia-container.svg?style=flat-square)](https://travis-ci.org/NVIDIA/libnvidia-container)
[![Coverity Scan](https://img.shields.io/coverity/scan/12444.svg?style=flat-square)](https://scan.coverity.com/projects/nvidia-libnvidia-container)
[![LGTM](https://img.shields.io/lgtm/grade/cpp/g/NVIDIA/libnvidia-container.svg?style=flat-square)](https://lgtm.com/projects/g/NVIDIA/libnvidia-container/alerts/)

This repository provides a library and a simple CLI utility to automatically configure GNU/Linux containers leveraging NVIDIA hardware.\
The implementation relies on kernel primitives and is designed to be agnostic of the container runtime.

## Installing the library
### From packages
Refer to the [repository configuration](https://nvidia.github.io/libnvidia-container/) for your Linux distribution.

### From sources
With Docker:
```bash
# Generate packages for a given distribution in the dist/ directory
make docker-ubuntu:16.04
````

Without Docker:
```bash
make install

# Alternatively in order to customize the installation paths
DESTDIR=/path/to/root make install prefix=/usr
```

## Using the library
### Container runtime example
Refer to the [nvidia-container-runtime](https://github.com/NVIDIA/nvidia-container-runtime) project.

### Command line example

```bash
# Setup a new set of namespaces
cd $(mktemp -d) && mkdir rootfs
sudo unshare --mount --pid --fork

# Setup a rootfs based on Ubuntu 16.04 inside the new namespaces
curl http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04-core-amd64.tar.gz | tar -C rootfs -xz
useradd -R $(realpath rootfs) -U -u 1000 -s /bin/bash nvidia
mount --bind rootfs rootfs
mount --make-private rootfs
cd rootfs

# Mount standard filesystems
mount -t proc none proc
mount -t sysfs none sys
mount -t tmpfs none tmp
mount -t tmpfs none run

# Isolate the first GPU device along with basic utilities
nvidia-container-cli --load-kmods configure --ldconfig=@/sbin/ldconfig.real --no-cgroups --utility --device 0 $(pwd)

# Change into the new rootfs
pivot_root . mnt
umount -l mnt
exec chroot --userspec 1000:1000 . env -i bash

# Run nvidia-smi from within the container
nvidia-smi -L
```

## Copyright and License

This project is released under the [BSD 3-clause license](https://github.com/NVIDIA/libnvidia-container/blob/master/LICENSE).

Additionally, this project can be dynamically linked with libelf from the elfutils package (https://sourceware.org/elfutils), in which case additional terms apply.\
Refer to [NOTICE](https://github.com/NVIDIA/libnvidia-container/blob/master/NOTICE) for more information.

## Issues and Contributing

A signed copy of the [Contributor License Agreement](https://raw.githubusercontent.com/NVIDIA/libnvidia-container/master/CLA) needs to be provided to <a href="mailto:digits@nvidia.com">digits@nvidia.com</a> before any change can be accepted.

* Please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new)
* You can contribute by opening a [pull request](https://help.github.com/articles/using-pull-requests/)
