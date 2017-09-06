Name: nvidia-container-runtime
Version: %{version}
Release: %{release}
Group: Development Tools

Vendor: NVIDIA CORPORATION
Packager: NVIDIA CORPORATION <cudatools@nvidia.com>

Summary: NVIDIA container runtime
URL: https://github.com/NVIDIA/nvidia-container-runtime
# The entire source code is BSD except runc/* which is ASL 2.0
# runc NOTICE file: https://github.com/opencontainers/runc/blob/master/NOTICE
License: BSD and ASL 2.0

Source0: nvidia-container-runtime
Source1: nvidia-container-runtime-hook
Source2: config.toml
Source3: LICENSE

Requires: libnvidia-container-tools >= 0.1.0, libnvidia-container-tools < 2.0.0
Requires: libseccomp

%description
Provides a modified version of runc allowing users to run GPU enabled
containers.

%prep
cp %{SOURCE0} %{SOURCE1} %{SOURCE2} %{SOURCE3} .

%install
mkdir -p %{buildroot}%{_bindir}
install -m 755 -t %{buildroot}%{_bindir} nvidia-container-runtime
install -m 755 -t %{buildroot}%{_bindir} nvidia-container-runtime-hook
mkdir -p %{buildroot}/etc/nvidia-container-runtime
install -m 644 -t %{buildroot}/etc/nvidia-container-runtime config.toml

%files
%license LICENSE
%{_bindir}/nvidia-container-runtime-hook
%{_bindir}/nvidia-container-runtime
/etc/nvidia-container-runtime/config.toml

%changelog
