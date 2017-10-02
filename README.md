# nvidia-container-runtime
[![GitHub license](https://img.shields.io/badge/license-New%20BSD-blue.svg?style=flat-square)](https://raw.githubusercontent.com/NVIDIA/nvidia-container-runtime/master/LICENSE)
[![Package repository](https://img.shields.io/badge/packages-repository-b956e8.svg?style=flat-square)](https://nvidia.github.io/nvidia-container-runtime)

**Warning: This project is an alpha release, it is not intended to be used in production systems.**

A modified version of [runc](https://github.com/opencontainers/runc) adding a custom [pre-start hook](https://github.com/opencontainers/runtime-spec/blob/master/config.md#prestart) to all containers.  
If environment variable `NVIDIA_VISIBLE_DEVICES` is set in the OCI spec, the hook will configure GPU access for the container by leveraging `nvidia-container-cli` from project [libnvidia-container](https://github.com/NVIDIA/libnvidia-container).

## Usage example

```sh
# Setup a rootfs based on Ubuntu 16.04
cd $(mktemp -d) && mkdir rootfs
curl -sS http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04-core-amd64.tar.gz | tar --exclude 'dev/*' -C rootfs -xz

# Create an OCI runtime spec
nvidia-container-runtime spec
sed -i 's;"sh";"nvidia-smi";' config.json
sed -i 's;\("TERM=xterm"\);\1, "NVIDIA_VISIBLE_DEVICES=0";' config.json

# Run the container
sudo nvidia-container-runtime run nvidia_smi
```

## Environment variables (OCI spec)

Each environment variable maps to an command-line argument for `nvidia-container-cli` from [libnvidia-container](https://github.com/NVIDIA/libnvidia-container).  
These variables are already set in our [official CUDA images](https://hub.docker.com/r/nvidia/cuda/).

### `NVIDIA_VISIBLE_DEVICES`
This variable controls which GPUs will be made accessible inside the container.  

#### Possible values
* `0,1,2`, `GPU-fef8089b` …: a comma-separated list of GPU UUID(s) or index(es),
* `all`: all GPUs will be accessible, this is the default value in our container images,
* `none`: no GPU will be accessible, but driver capabilities will be enabled.
* *empty*: `nvidia-container-runtime` will have the same behavior as `runc`.

### `NVIDIA_DRIVER_CAPABILITIES`
This option controls which driver libraries/binaries will be mounted inside the container.

#### Possible values
* `compute,video`, `graphics,utility` …: a comma-separated list of driver features the container needs,
* `all`: enable all available driver capabilities.
* *empty*: use default driver capabilities, determined by `nvidia-container-cli`.

#### Supported driver capabilities
* `compute`: required for CUDA and OpenCL applications,
* `compat32`: required for running 32-bit applications,
* `graphics`: required for running OpenGL and Vulkan applications,
* `utility`: required for using `nvidia-smi` and NVML,
* `video`: required for using the Video Codec SDK.

### `NVIDIA_CUDA_VERSION`
The version of the CUDA toolkit used by the container.  
If the version of the NVIDIA driver is insufficient to run this version of CUDA, the container will not be started.

#### Possible values
* `7.5`, `8.0`, `9.0` …: any valid CUDA version in the form `major.minor`.

### `CUDA_VERSION`
Similar to `NVIDIA_CUDA_VERSION`, for legacy CUDA images.  
In addition, if `NVIDIA_CUDA_VERSION` is not set, `NVIDIA_VISIBLE_DEVICES` and `NVIDIA_DRIVER_CAPABILITIES` will default to `all`.

## Copyright and License

This project is released under the [BSD 3-clause license](https://github.com/NVIDIA/nvidia-container-runtime/blob/master/LICENSE).
