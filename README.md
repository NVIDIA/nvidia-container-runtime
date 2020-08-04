# nvidia-container-runtime
[![GitHub license](https://img.shields.io/badge/License-Apache%202.0-blue.svg?style=flat-square)](https://raw.githubusercontent.com/NVIDIA/nvidia-container-runtime/master/LICENSE)
[![Package repository](https://img.shields.io/badge/packages-repository-b956e8.svg?style=flat-square)](https://nvidia.github.io/nvidia-container-runtime)

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

## Installation
#### Ubuntu distributions

1. Install the repository for your distribution by following the instructions [here](http://nvidia.github.io/nvidia-container-runtime/).
2. Install the `nvidia-container-runtime` package:
```
sudo apt-get install nvidia-container-runtime
```

#### CentOS distributions
1. Install the repository for your distribution by following the instructions [here](http://nvidia.github.io/nvidia-container-runtime/).
2. Install the `nvidia-container-runtime` package:
```
sudo yum install nvidia-container-runtime
```

## Docker Engine setup

**Do not follow this section if you installed the `nvidia-docker2` package, it already registers the runtime.**

To register the `nvidia` runtime, use the method below that is best suited to your environment.  
You might need to merge the new argument with your existing configuration.

#### Systemd drop-in file
```bash
sudo mkdir -p /etc/systemd/system/docker.service.d
sudo tee /etc/systemd/system/docker.service.d/override.conf <<EOF
[Service]
ExecStart=
ExecStart=/usr/bin/dockerd --host=fd:// --add-runtime=nvidia=/usr/bin/nvidia-container-runtime
EOF
sudo systemctl daemon-reload
sudo systemctl restart docker
```

#### Daemon configuration file
```bash
sudo tee /etc/docker/daemon.json <<EOF
{
    "runtimes": {
        "nvidia": {
            "path": "/usr/bin/nvidia-container-runtime",
            "runtimeArgs": []
        }
    }
}
EOF
sudo pkill -SIGHUP dockerd
```

You can optionally reconfigure the default runtime by adding the following to `/etc/docker/daemon.json`:
```
"default-runtime": "nvidia"
```


#### Command line
```bash
sudo dockerd --add-runtime=nvidia=/usr/bin/nvidia-container-runtime [...]
```

## Containerd setup

This section shows how to add the container runtime into Containerd's configuration file.

**Do NOT follow this section if you just use docker or if you don't understand what is it.**

1. Generate default config file **if you do not have one**.
```bash
containerd config default > /etc/containerd/config.toml
```

2. Edit config file and add runtime describe text.

    You can use any editor you like to edit your config file, for example `vim /etc/containerd/config.toml`.
    
    The whole toml file is too long, so text below shows part of it. 

```toml
      # Please find this title in your config file
      [plugins."io.containerd.grpc.v1.cri".containerd.runtimes]

        # This section is added by system, we can just ignore it.
        [plugins."io.containerd.grpc.v1.cri".containerd.runtimes.runc]
          runtime_type = "io.containerd.runc.v2"
          runtime_engine = ""
          runtime_root = ""
          privileged_without_host_devices = false
          base_runtime_spec = ""
          [plugins."io.containerd.grpc.v1.cri".containerd.runtimes.runc.options]

        # This section is not exist in default file.
        # Here is the example for nvidia container runtime.
        # Please add this section into your config file so that you can use nvidia-runtime.
        [plugins."io.containerd.grpc.v1.cri".containerd.runtimes.nvidia]
          runtime_type = "io.containerd.runc.v2"
          runtime_engine = ""
          runtime_root = ""
          privileged_without_host_devices = false
          base_runtime_spec = ""
          [plugins."io.containerd.grpc.v1.cri".containerd.runtimes.nvidia.options]
            BinaryName = "nvidia-container-runtime"
```

3. Change the default runtime name

    If you want to use nvidia runtime as your default runtime, you can overwrite `default_runtime_name`. However, 
    this step is optional, you may use other methods to choose runtime in dynamic way (For example, add a runtime claim in pod's spec).

```toml
    # At first, you need to find this title.
    [plugins."io.containerd.grpc.v1.cri".containerd]
      snapshotter = "overlayfs"
      # Change this from "runc" to "nvidia"
      default_runtime_name = "nvidia"
```

4. Restart containerd service

    Don't forget to restart containerd service to apply your changes.

```bash
systemctl restart containerd
```


## Environment variables (OCI spec)

Each environment variable maps to an command-line argument for `nvidia-container-cli` from [libnvidia-container](https://github.com/NVIDIA/libnvidia-container).  
These variables are already set in our [official CUDA images](https://hub.docker.com/r/nvidia/cuda/).

### `NVIDIA_VISIBLE_DEVICES`
This variable controls which GPUs will be made accessible inside the container.  

#### Possible values
* `0,1,2`, `GPU-fef8089b` …: a comma-separated list of GPU UUID(s) or index(es).
* `all`: all GPUs will be accessible, this is the default value in our container images.
* `none`: no GPU will be accessible, but driver capabilities will be enabled.
* `void` or *empty* or *unset*: `nvidia-container-runtime` will have the same behavior as `runc`.

**Note**: When running on a MIG capable device, the following values will also be available:
* `0:0,0:1,1:0`, `MIG-GPU-fef8089b/0/1` …: a comma-separated list of MIG Device UUID(s) or index(es).

Where the MIG device indices have the form `<GPU Device Index>:<MIG Device Index>` as seen in the example output:
```
$ nvidia-smi -L
GPU 0: Graphics Device (UUID: GPU-b8ea3855-276c-c9cb-b366-c6fa655957c5)
  MIG Device 0: (UUID: MIG-GPU-b8ea3855-276c-c9cb-b366-c6fa655957c5/1/0)
  MIG Device 1: (UUID: MIG-GPU-b8ea3855-276c-c9cb-b366-c6fa655957c5/1/1)
  MIG Device 2: (UUID: MIG-GPU-b8ea3855-276c-c9cb-b366-c6fa655957c5/11/0)
```

### `NVIDIA_MIG_CONFIG_DEVICES`
This variable controls which of the visible GPUs can have their MIG
configuration managed from within the container. This includes enabling and
disabling MIG mode, creating and destroying GPU Instances and Compute
Instances, etc.

#### Possible values
* `all`: Allow all MIG-capable GPUs in the visible device list to have their
  MIG configurations managed.

**Note**:
* This feature is only available on MIG capable devices (e.g. the A100).
* To use this feature, the container must be started with `CAP_SYS_ADMIN` privileges.
* When not running as `root`, the container user must have read access to the
  `/proc/driver/nvidia/capabilities/mig/config` file on the host.

### `NVIDIA_MIG_MONITOR_DEVICES`
This variable controls which of the visible GPUs can have aggregate information
about all of their MIG devices monitored from within the container. This
includes inspecting the aggregate memory usage, listing the aggregate running
processes, etc.

#### Possible values
* `all`: Allow all MIG-capable GPUs in the visible device list to have their
  MIG devices monitored.

**Note**:
* This feature is only available on MIG capable devices (e.g. the A100).
* To use this feature, the container must be started with `CAP_SYS_ADMIN` privileges.
* When not running as `root`, the container user must have read access to the
  `/proc/driver/nvidia/capabilities/mig/monitor` file on the host.

### `NVIDIA_DRIVER_CAPABILITIES`
This option controls which driver libraries/binaries will be mounted inside the container.

#### Possible values
* `compute,video`, `graphics,utility` …: a comma-separated list of driver features the container needs.
* `all`: enable all available driver capabilities.
* *empty* or *unset*: use default driver capability: `utility`.

#### Supported driver capabilities
* `compute`: required for CUDA and OpenCL applications.
* `compat32`: required for running 32-bit applications.
* `graphics`: required for running OpenGL and Vulkan applications.
* `utility`: required for using `nvidia-smi` and NVML.
* `video`: required for using the Video Codec SDK.
* `display`: required for leveraging X11 display.

### `NVIDIA_REQUIRE_*`
A logical expression to define constraints on the configurations supported by the container.  

#### Supported constraints
* `cuda`: constraint on the CUDA driver version.
* `driver`: constraint on the driver version.
* `arch`: constraint on the compute architectures of the selected GPUs.
* `brand`: constraint on the brand of the selected GPUs (e.g. GeForce, Tesla, GRID).

#### Expressions
Multiple constraints can be expressed in a single environment variable: space-separated constraints are ORed, comma-separated constraints are ANDed.  
Multiple environment variables of the form `NVIDIA_REQUIRE_*` are ANDed together.

### `NVIDIA_DISABLE_REQUIRE`
Single switch to disable all the constraints of the form `NVIDIA_REQUIRE_*`.

### `NVIDIA_REQUIRE_CUDA`

The version of the CUDA toolkit used by the container. It is an instance of the generic `NVIDIA_REQUIRE_*` case and it is set by official CUDA images.
If the version of the NVIDIA driver is insufficient to run this version of CUDA, the container will not be started.

#### Possible values
* `cuda>=7.5`, `cuda>=8.0`, `cuda>=9.0` …: any valid CUDA version in the form `major.minor`.

### `CUDA_VERSION`
Similar to `NVIDIA_REQUIRE_CUDA`, for legacy CUDA images.  
In addition, if `NVIDIA_REQUIRE_CUDA` is not set, `NVIDIA_VISIBLE_DEVICES` and `NVIDIA_DRIVER_CAPABILITIES` will default to `all`.

## Issues and Contributing

[Checkout the Contributing document!](CONTRIBUTING.md)

* Please let us know by [filing a new issue](https://github.com/NVIDIA/nvidia-docker/issues/new)
* You can contribute by opening a [pull request](https://help.github.com/articles/using-pull-requests/)
