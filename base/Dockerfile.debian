ARG VERSION_ID
FROM debian:${VERSION_ID}

RUN apt-get update && apt-get install -y --no-install-recommends \
        wget \
        ca-certificates \
        git \
        build-essential \
        dh-make \
        fakeroot \
        devscripts \
        lsb-release && \
    rm -rf /var/lib/apt/lists/*

RUN echo "deb http://ftp.debian.org/debian $(lsb_release -cs)-backports main" > /etc/apt/sources.list.d/backports.list

ARG GOLANG_VERSION=0.0.0
RUN set -eux; \
    \
    arch="$(uname -m)"; \
    case "${arch##*-}" in \
        x86_64 | amd64) ARCH='amd64' ;; \
        ppc64el | ppc64le) ARCH='ppc64le' ;; \
        *) echo "unsupported architecture" ; exit 1 ;; \
    esac; \
    wget -nv -O - https://storage.googleapis.com/golang/go${GOLANG_VERSION}.linux-${ARCH}.tar.gz \
    | tar -C /usr/local -xz

ENV GOPATH /go
ENV PATH $GOPATH/bin:/usr/local/go/bin:$PATH
