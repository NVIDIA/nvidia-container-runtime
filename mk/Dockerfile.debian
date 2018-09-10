ARG IMAGESPEC=debian:9.2
FROM ${IMAGESPEC}
ARG IMAGESPEC=debian:9.2

RUN apt-get update && apt-get install -y --no-install-recommends \
        apt-utils \
        bmake \
        build-essential \
        bzip2 \
        ca-certificates \
        curl \
        devscripts \
        dh-make \
        fakeroot \
        git \
        gnupg2 \
        libcap-dev \
        libelf-dev \
        libseccomp-dev \
        lintian \
        lsb-release \
        m4 \
        xz-utils && \
    rm -rf /var/lib/apt/lists/*

ENV GPG_TTY /dev/console

ARG USERSPEC=0:0

WORKDIR /tmp/libnvidia-container
COPY . .
RUN chown -R $USERSPEC $PWD
USER $USERSPEC

ARG WITH_LIBELF=no
ARG WITH_TIRPC=no
ARG WITH_SECCOMP=yes
ENV WITH_LIBELF=${WITH_LIBELF}
ENV WITH_TIRPC=${WITH_TIRPC}
ENV WITH_SECCOMP=${WITH_SECCOMP}

RUN make distclean && make -j"$(nproc)"

ENV DIST_DIR /mnt
VOLUME $DIST_DIR
CMD make dist && make deb
