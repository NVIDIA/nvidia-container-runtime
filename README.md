# nvidia-container-runtime

**Warning: This project is an alpha release, it is not intended to be used in production systems.**

A modified version of [runc](https://github.com/opencontainers/runc) adding a custom [pre-start hook](https://github.com/opencontainers/runtime-spec/blob/master/config.md#prestart) to all containers.  
If environment variable `NVIDIA_VISIBLE_DEVICES` is set in the OCI spec, the hook will configure GPU access for the container by leveraging `nvidia-container-cli` from project [libnvidia-container](https://github.com/NVIDIA/libnvidia-container).
