FROM ubuntu:20.04

ARG USER=1000
ARG GROUP=1000

RUN export DEBIAN_FRONTEND="noninteractive" && apt-get update -y && apt-get upgrade -y && apt-get install -y build-essential cmake ninja-build libboost-all-dev curl libcurl4-openssl-dev

RUN useradd -u ${USER} -g ${GROUP} uactor

#  docker build -t ubuntu_cpp_build:18.04 --build-arg USER=$(id -u) --build-arg GROUP=$(id -g) - < ubuntu-18.04.dockerfile
