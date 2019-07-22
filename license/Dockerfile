FROM debian:buster-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        ruby \
        ruby-dev \
        bundler \
        cmake \
        pkg-config \
        git \
        build-essential \
        libssl-dev

RUN gem install licensee

CMD ["/bin/bash"]
