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

Source0: LICENSE

Obsoletes: nvidia-container-runtime < 2.0.0
Requires: nvidia-container-toolkit > %{toolkit_version}, nvidia-container-toolkit < 2.0.0

%description
Provides a modified version of runc allowing users to run GPU enabled
containers.

%prep
cp %{SOURCE0} .

%install

%files
%license LICENSE

%changelog
* Thu Apr 29 2021 NVIDIA CORPORATION <cudatools@nvidia.com> 3.5.1-0.1.rc.1
- [BUILD] Allow tag to be specified instead of revision
- Convert to meta package that depends on nvidia-container-toolkit
- Remove libseccomp2 dependency

* Thu Apr 29 2021 NVIDIA CORPORATION <cudatools@nvidia.com> 3.5.0-1
- Add dependence on nvidia-container-toolkit >= 1.5.0
- Refactor the whole project for easier extensibility in the future
- Move main to cmd/nvidia-container-runtime to make it go-installable
- Switch to logrus for logging and refactor how logging is done
- Update to Golang 1.16.3
- Improvements to build and CI system
- Reorganize the code structure and format it correctly

* Fri Feb 05 2021 NVIDIA CORPORATION <cudatools@nvidia.com> 3.4.2-1
- Add dependence on nvidia-container-toolkit >= 1.4.2

* Mon Jan 25 2021 NVIDIA CORPORATION <cudatools@nvidia.com> 3.4.1-1
- Update README to list 'compute' as part of the default capabilities
- Switch to gomod for vendoring
- Update to Go 1.15.6 for builds
- Add dependence on nvidia-container-toolkit >= 1.4.1

* Wed Sep 16 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 3.4.0-1
- Bump version to v3.4.0
- Add dependence on nvidia-container-toolkit >= 1.3.0

* Wed Jul 08 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 3.3.0-1
- e550cb15 Update package license to match source license
- f02eef53 Update project License
- c0fe8aae Update dependence on nvidia-container-toolkit to 1.2.0

* Fri May 15 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 3.2.0-1
- e486a70e Update build system to support multi-arch builds
- 854f4c48 Require new MIG changes
