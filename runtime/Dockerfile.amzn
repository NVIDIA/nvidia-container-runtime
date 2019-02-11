ARG VERSION_ID
FROM nvidia/base/amzn:${VERSION_ID}

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
