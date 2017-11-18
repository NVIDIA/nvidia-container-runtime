# Repository configuration

In order to setup the nvidia-container-runtime repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/nvidia-container-runtime/issues/new).

## Ubuntu distributions (Xenial x86_64)

```bash
curl -s -L https://nvidia.github.io/nvidia-container-runtime/gpgkey | \
  sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-container-runtime/ubuntu16.04/amd64/nvidia-container-runtime.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-runtime.list
sudo apt-get update
```

## Debian distributions (Stretch x86_64)
```bash
curl -s -L https://nvidia.github.io/nvidia-container-runtime/gpgkey | \
  sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-container-runtime/debian9/amd64/nvidia-container-runtime.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-runtime.list
sudo apt-get update
```

## CentOS distributions (RHEL7 x86_64)

```bash
curl -s -L https://nvidia.github.io/nvidia-container-runtime/centos7/x86_64/nvidia-container-runtime.repo | \
  sudo tee /etc/yum.repos.d/nvidia-container-runtime.repo
```

