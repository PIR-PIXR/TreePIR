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

# Run apt-get and other root-level tasks first
RUN apt-get update && \
    apt-get install -y apt-utils build-essential clang-12 git-lfs pkg-config python3 python3-pip cmake curl zip unzip tar ranger gdb sudo neovim

# Create the ubuntu user
RUN useradd -m ubuntu && echo "ubuntu:ubuntu" | chpasswd && adduser ubuntu sudo

# Allow ubuntu to run sudo commands without a password
RUN echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to the ubuntu user and install vcpkg under /home/ubuntu
USER ubuntu
WORKDIR /home/ubuntu

RUN git clone https://github.com/Microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Install hexl
RUN sudo ./vcpkg/vcpkg install hexl

# Define working directory and copy files
WORKDIR /home/ubuntu/spiral
COPY . .

# Run pip install and other commands as ubuntu user
RUN sudo pip install tabulate
#RUN sudo python3 select_params.py 20 256
