ARG VERSION_ID
FROM nvidia/base/ubuntu:${VERSION_ID}

# runc dependencies
RUN apt-get update && \
    if [ "$(lsb_release -cs)" = "trusty" ]; then \
        apt-get install -t "trusty-backports" -y libseccomp-dev; \
    else \
        apt-get install -y libseccomp-dev; \
    fi && \
    apt-get install -y \
        pkg-config \
        libapparmor-dev \
        libselinux1-dev && \
    rm -rf /var/lib/apt/lists/*

RUN go get github.com/LK4D4/vndr

# runc
WORKDIR $GOPATH/src/github.com/opencontainers/runc

RUN git clone https://github.com/opencontainers/runc.git .

# packaging
ARG PKG_VERS
ARG PKG_REV

ENV DEBFULLNAME "NVIDIA CORPORATION"
ENV DEBEMAIL "cudatools@nvidia.com"
ENV REVISION "$PKG_VERS-$PKG_REV"
ENV SECTION ""

# output directory
ENV DIST_DIR=/tmp/nvidia-container-runtime-$PKG_VERS
RUN mkdir -p $DIST_DIR /dist

ARG RUNC_COMMIT
COPY runc/$RUNC_COMMIT/ /tmp/patches/runc

RUN git checkout $RUNC_COMMIT && \
    git apply /tmp/patches/runc/* && \
    if [ -f vendor.conf ]; then vndr; fi && \
    make BUILDTAGS="seccomp apparmor selinux" && \
    mv runc $DIST_DIR/nvidia-container-runtime

WORKDIR $DIST_DIR
COPY debian ./debian

RUN sed -i "s;@VERSION@;${REVISION#*+};" debian/changelog && \
    if [ "$REVISION" != "$(dpkg-parsechangelog --show-field=Version)" ]; then exit 1; fi

CMD export DISTRIB="$(lsb_release -cs)" && \
    debuild -eREVISION -eDISTRIB -eSECTION --dpkg-buildpackage-hook='sh debian/prepare' -i -us -uc -b && \
    mv /tmp/*.deb /dist
