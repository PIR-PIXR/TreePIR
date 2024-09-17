#FROM ubuntu:20.04
#
#ENV DEBIAN_FRONTEND=noninteractive
#
#RUN apt-get update && \
#    apt-get install -y apt-utils build-essential clang-12 git-lfs pkg-config python3 python3-pip cmake curl zip unzip tar ranger gdb
#    # git lfs install
#
#RUN cd /root && \
#    git clone https://github.com/Microsoft/vcpkg.git && \
#    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics && \
#    ./vcpkg/vcpkg install hexl
#
## RUN git clone https://github.com/menonsamir/spiral.git /root/spiral
#
#WORKDIR /root/spiral
#
#COPY . .
#
#RUN pip install tabulate
#
#RUN python3 select_params.py 20 256

########################################

#FROM ubuntu:20.04
#
#ENV DEBIAN_FRONTEND=noninteractive
#
#RUN apt-get update && \
#    apt-get install -y apt-utils build-essential clang-12 git-lfs pkg-config python3 python3-pip cmake curl zip unzip tar ranger gdb
#    # git lfs install
#
#RUN useradd -m ubuntu
#USER ubuntu
#
#RUN cd /home/ubuntu && \
#    git clone https://github.com/Microsoft/vcpkg.git && \
#    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics && \
#    ./vcpkg/vcpkg install hexl
#
## RUN git clone https://github.com/menonsamir/spiral.git /root/spiral
#
#WORKDIR /home/ubuntu/spiral
#
#COPY . .
#
#RUN pip install tabulate
#
#RUN python3 select_params.py 20 256

# ====================================
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y apt-utils build-essential clang-12 git-lfs pkg-config python3 python3-pip cmake curl zip unzip tar ranger gdb sudo neovim

RUN useradd -m ubuntu && echo "ubuntu:ubuntu" | chpasswd && adduser ubuntu sudo
RUN echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER ubuntu
WORKDIR /home/ubuntu
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

RUN sudo ./vcpkg/vcpkg install hexl

WORKDIR /home/ubuntu/Process_Workspace
WORKDIR /home/ubuntu/spiral
COPY . .
RUN sudo pip install tabulate
#RUN sudo python3 select_params.py 20 256
