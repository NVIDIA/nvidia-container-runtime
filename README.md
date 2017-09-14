# nvidia-container-runtime

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

## Copyright and License

This project is released under the [BSD 3-clause license](https://github.com/NVIDIA/nvidia-container-runtime/blob/master/LICENSE).
