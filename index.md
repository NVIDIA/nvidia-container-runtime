# Repository configuration

In order to setup the nvidia-container-runtime repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/nvidia-container-runtime/issues/new).

List of supported distributions:

|         | Ubuntu 14.04 | Ubuntu 16.04 | Ubuntu 18.04 | Debian 8 | Debian 9 | Centos 7 | RHEL 7 | Amazon Linux 1 | Amazon Linux 2 |
| ------- | :----------: | :----------: | :----------: | :------: | :------: | :------: | :----: | :------------: | :------------: |
| x86_64  |      X       |      X       |       X      |     X    |    X     |    X     |    X   |        X       |        X       |
| ppc64le |              |      X       |       X      |          |          |    X     |    X   |                |                |

## Debian-based distributions

```bash
curl -s -L https://nvidia.github.io/nvidia-container-runtime/gpgkey | \
  sudo apt-key add -
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-container-runtime/$distribution/nvidia-container-runtime.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-runtime.list
sudo apt-get update
```

## RHEL-based distributions

```bash
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-container-runtime/$distribution/nvidia-container-runtime.repo | \
  sudo tee /etc/yum.repos.d/nvidia-container-runtime.repo
```

# Updating repository keys

In order to update the nvidia-container-runtime repository key for your distribution, follow the instructions below.

## RHEL-based distributions

```bash
DIST=$(sed -n 's/releasever=//p' /etc/yum.conf)
DIST=${DIST:-$(. /etc/os-release; echo $VERSION_ID)}
sudo rpm -e gpg-pubkey-f796ecb0
sudo gpg --homedir /var/lib/yum/repos/$(uname -m)/$DIST/nvidia-container-runtime/gpgdir --delete-key f796ecb0
sudo yum makecache
```

## Debian-based distributions
```bash
curl -s -L https://nvidia.github.io/nvidia-container-runtime/gpgkey | \
  sudo apt-key add -
```
