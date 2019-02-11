ARG VERSION_ID
FROM nvidia/base/ubuntu:${VERSION_ID}

# packaging
ARG PKG_VERS
ARG PKG_REV

ENV DEBFULLNAME "NVIDIA CORPORATION"
ENV DEBEMAIL "cudatools@nvidia.com"
ENV REVISION "$PKG_VERS-$PKG_REV"
ENV SECTION ""

# output directory
ENV DIST_DIR=/tmp/nvidia-container-runtime-hook-$PKG_VERS
RUN mkdir -p $DIST_DIR /dist

# nvidia-container-runtime-hook
COPY nvidia-container-runtime-hook/ $GOPATH/src/nvidia-container-runtime-hook

RUN go get -ldflags "-s -w" -v nvidia-container-runtime-hook && \
    mv $GOPATH/bin/nvidia-container-runtime-hook $DIST_DIR/nvidia-container-runtime-hook

COPY config.toml.ubuntu $DIST_DIR/config.toml

WORKDIR $DIST_DIR
COPY debian ./debian

RUN if [ "$REVISION" != "$(dpkg-parsechangelog --show-field=Version)" ]; then exit 1; fi

CMD export DISTRIB="$(lsb_release -cs)" && \
    debuild -eDISTRIB -eSECTION --dpkg-buildpackage-hook='sh debian/prepare' -i -us -uc -b && \
    mv /tmp/*.deb /dist
