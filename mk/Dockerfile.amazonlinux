ARG IMAGESPEC=amazonlinux:2
FROM ${IMAGESPEC}
ARG IMAGESPEC=amazonlinux:2

RUN yum install -y \
        bzip2 \
        createrepo \
        elfutils-libelf-devel \
        gcc \
        git \
        libcap-devel \
        libseccomp-devel \
        m4 \
        make \
        redhat-lsb-core \
        rpm-build \
        rpm-sign \
        rpmlint \
        which && \
    rm -rf /var/cache/yum/*

ARG WITH_LIBELF=no
ARG WITH_TIRPC=no
ARG WITH_SECCOMP=yes
ENV WITH_LIBELF=${WITH_LIBELF}
ENV WITH_TIRPC=${WITH_TIRPC}
ENV WITH_SECCOMP=${WITH_SECCOMP}

RUN if [ "$WITH_LIBELF" = "no" ]; then \
        arch=$(uname -m) && \
        cd $(mktemp -d) && \
        curl -fsSL -O https://mirrors.kernel.org/mageia/distrib/6/${arch}/media/core/release/pmake-1.45-16.mga6.${arch}.rpm && \
        curl -fsSL -O https://mirrors.kernel.org/mageia/distrib/6/${arch}/media/core/release/bmake-20161212-1.mga6.${arch}.rpm && \
        rpm -i *.rpm && \
        rm -rf $PWD \
    ; fi

ARG USERSPEC=0:0

WORKDIR /tmp/libnvidia-container
COPY . .
RUN chown -R $USERSPEC $PWD
USER $USERSPEC

RUN make distclean && make -j"$(nproc)"

ENV DIST_DIR /mnt
VOLUME $DIST_DIR
CMD make dist && make rpm
