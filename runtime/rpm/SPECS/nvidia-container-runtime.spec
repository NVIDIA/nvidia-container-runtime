Name: nvidia-container-runtime
Version: %{version}
Release: %{release}
Group: Development Tools

Vendor: NVIDIA CORPORATION
Packager: NVIDIA CORPORATION <cudatools@nvidia.com>

Summary: NVIDIA container runtime
URL: https://github.com/NVIDIA/nvidia-container-runtime
# runc NOTICE file: https://github.com/opencontainers/runc/blob/master/NOTICE
License: ASL 2.0

Source0: nvidia-container-runtime
Source1: LICENSE

Obsoletes: nvidia-container-runtime < 3.0.0
Requires: nvidia-container-runtime-hook < 2.0.0
Requires: libseccomp

%description
Provides a modified version of runc allowing users to run GPU enabled
containers.

%prep
cp %{SOURCE0} %{SOURCE1} .

%install
mkdir -p %{buildroot}%{_bindir}
install -m 755 -t %{buildroot}%{_bindir} nvidia-container-runtime

%files
%license LICENSE
%{_bindir}/nvidia-container-runtime

%changelog
