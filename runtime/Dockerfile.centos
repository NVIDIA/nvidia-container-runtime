ARG VERSION_ID
FROM nvidia/base/centos:${VERSION_ID}

# Install a more version of git (for vndr)
RUN yum install -y \
        gcc \
        make \
        gettext-devel \
        openssl-devel \
        perl-CPAN \
        perl-devel \
        zlib-devel \
        curl-devel && \
    rm -rf /var/cache/yum/*

RUN GIT_DOWNLOAD_SUM=e19d450648d6d100eb93abaa5d06ffbc778394fb502354b7026d73e9bcbc3160 && \
    curl -fsSL https://www.kernel.org/pub/software/scm/git/git-2.13.2.tar.gz -O && \
    echo "$GIT_DOWNLOAD_SUM  git-2.13.2.tar.gz" | sha256sum -c --strict - && \
    tar --no-same-owner -xzf git-2.13.2.tar.gz -C /tmp && \
    cd /tmp/git-2.13.2 && \
    ./configure && make -j"$(nproc)" install

# runc dependencies
RUN yum install -y \
        pkgconfig \
        gcc \
        libseccomp-devel \
        libselinux-devel && \
    rm -rf /var/cache/yum/*

RUN go get github.com/LK4D4/vndr

# runc
WORKDIR $GOPATH/src/github.com/opencontainers/runc

RUN git clone https://github.com/opencontainers/runc.git .

# packaging
ARG PKG_VERS
ARG PKG_REV

ENV VERSION $PKG_VERS
ENV RELEASE $PKG_REV

# output directory
ENV DIST_DIR=/tmp/nvidia-container-runtime-$PKG_VERS/SOURCES
RUN mkdir -p $DIST_DIR /dist

ARG RUNC_COMMIT
COPY runc/$RUNC_COMMIT/ /tmp/patches/runc

RUN git checkout $RUNC_COMMIT && \
    git apply /tmp/patches/runc/* && \
    if [ -f vendor.conf ]; then vndr; fi && \
    make BUILDTAGS="seccomp selinux" && \
    mv runc $DIST_DIR/nvidia-container-runtime

WORKDIR $DIST_DIR/..
COPY rpm .

CMD arch=$(uname -m) && \
    rpmbuild --clean --target=$arch -bb \
             -D "_topdir $PWD" \
             -D "version $VERSION" \
             -D "release $RELEASE" \
             SPECS/nvidia-container-runtime.spec && \
    mv RPMS/$arch/*.rpm /dist
