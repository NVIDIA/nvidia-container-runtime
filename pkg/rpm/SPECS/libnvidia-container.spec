Name: libnvidia-container
License: BSD
Vendor: NVIDIA CORPORATION
Packager: NVIDIA CORPORATION <cudatools@nvidia.com>
URL: https://github.com/NVIDIA/libnvidia-container
BuildRequires: make bmake
Version: %{_version}
Release: 0.1%{?_tag:.%_tag}
Summary: NVIDIA container runtime library
%description
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

%install
DESTDIR=%{buildroot} %{__make} install prefix=%{_prefix} exec_prefix=%{_exec_prefix} bindir=%{_bindir} libdir=%{_libdir} includedir=%{_includedir}
install -d -m 755 %{buildroot}%{_licensedir}/%{name}-%{version}
install -m 644 LICENSE %{buildroot}%{_licensedir}/%{name}-%{version}

%package -n %{name}%{_major}
Summary: NVIDIA container runtime library
%description -n %{name}%{_major}
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package requires the NVIDIA driver (>= 340.29) to be installed separately.
%post -n %{name}%{_major} -p /sbin/ldconfig
%postun -n %{name}%{_major} -p /sbin/ldconfig
%files -n %{name}%{_major}
%license %{_licensedir}/*
%{_libdir}/lib*.so.*

%package devel
Requires: %{name}%{_major}%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (development files)
%description devel
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains the files required to compile programs with the library.
%files devel
%license %{_licensedir}/*
%{_includedir}/*.h
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*.pc

%package static
Requires: %{name}-devel%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (static library)
%description static
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package requires the NVIDIA driver (>= 340.29) to be installed separately.
%files static
%license %{_licensedir}/*
%{_libdir}/lib*.a

%define debug_package %{nil}
%package -n %{name}%{_major}-debuginfo
Requires: %{name}%{_major}%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (debugging symbols)
%description -n %{name}%{_major}-debuginfo
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains the debugging symbols for the library.
%files -n %{name}%{_major}-debuginfo
%license %{_licensedir}/*
%{_prefix}/lib/debug%{_libdir}/lib*.so.*

%package tools
Requires: %{name}%{_major}%{?_isa} >= %{version}-%{release}
Summary: NVIDIA container runtime library (command-line tools)
%description tools
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains command-line tools that facilitate using the library.
%files tools
%license %{_licensedir}/*
%{_bindir}/*

%changelog
* Mon Oct 30 2017 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.alpha.2
- b80e4b6 Relax some requirement constraints
- 3cd1bb6 Handle 32-bit PCI domains
- 6c67a19 Add support for device architecture requirement
- 7584e96 Filter NVRM proc filesystem based on visible devices
- 93c46e1 Prevent the driver process from triggering MPS
- fe4925e Reject invalid device identifier "GPU-"
- dabef1c Do not change bind mount attributes on top-level directories

* Tue Sep 05 2017 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.alpha.1
- Initial release
