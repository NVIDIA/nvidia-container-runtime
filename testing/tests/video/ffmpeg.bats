#!/usr/bin/env bats

# based on nvidia ffmpeg docs

load ../helpers

image=nvidia/cuda:8.0-devel-ubuntu16.04

function setup() {
    check_runtime
    docker pull $image >/dev/null 2>&1 || true
}

@test "ffmpeg video encoding" {
    cat <<EOF > Dockerfile

    FROM $image

    RUN apt-get update && apt-get install -y --no-install-recommends \
            ca-certificates \
            git \
            libgl1-mesa-glx \
            make \
            nasm && \
        rm -rf /var/lib/apt/lists/*

    RUN git clone --depth 1 --branch n3.3.3 https://github.com/ffmpeg/ffmpeg ffmpeg && \
        cd ffmpeg && \
        ./configure --enable-cuda --enable-cuvid --enable-nvenc --enable-nonfree --enable-libnpp \
                    --extra-cflags=-I/usr/local/cuda/include \
                    --extra-ldflags=-L/usr/local/cuda/lib64 && \
        make -j"$(nproc)" install && \
        ldconfig

     ENTRYPOINT ["ffmpeg"]
     WORKDIR /tmp
EOF

    docker build --pull -t "ffmpeg_cuda" .
    docker_run --rm --runtime=nvidia -e NVIDIA_DRIVER_CAPABILITIES=video,compute ffmpeg_cuda -y -hwaccel cuvid -c:v h264_cuvid -vsync 0 -i http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4 -vf scale_npp=1280:720 -vcodec h264_nvenc -t 00:01:00 output.mp4
    [ "$status" -eq 0 ]
}
