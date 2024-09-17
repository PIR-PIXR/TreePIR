# NOTE: This is intended to be used as a CLion toolchain.
#       It is not intended to be used as a standalone Docker image.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y apt-utils build-essential clang-12 git-lfs pkg-config python3 python3-pip cmake curl zip unzip tar ranger gdb sudo neovim software-properties-common fonts-inter kitty-terminfo
RUN useradd -m ubuntu && echo "ubuntu:ubuntu" | chpasswd && adduser ubuntu sudo
RUN echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
ENV TERM=xterm-kitty

USER ubuntu
WORKDIR /home/ubuntu
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
RUN sudo ./vcpkg/vcpkg install hexl nlohmann-json boost-multi-index simdjson
RUN sudo pip install tabulate matplotlib pandas Jinja2
