From ubuntu:18.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg2 \
    jq \
    moreutils \
    software-properties-common

RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg | apt-key add - && \
    curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | apt-key add - && \
    curl -s -L https://nvidia.github.io/nvidia-docker/ubuntu18.04/nvidia-docker.list | \
        tee /etc/apt/sources.list.d/nvidia-docker.list && \
    add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
        $(lsb_release -cs) stable"

RUN apt-get update && apt-get install -y --no-install-recommends \
    docker-ce-cli \
    nvidia-container-runtime

WORKDIR /work

COPY src/. .
RUN cp /etc/nvidia-container-runtime/config.toml ./

RUN chmod +x /work/run.sh

CMD ["bash"]
