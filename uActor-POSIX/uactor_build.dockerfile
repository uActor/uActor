FROM ubuntu:20.04

ARG USER=1000
ARG GROUP=1000

RUN export DEBIAN_FRONTEND="noninteractive" && apt-get update -y && apt-get upgrade -y && apt-get install -y build-essential cmake ninja-build libboost-all-dev curl libcurl4-openssl-dev

RUN groupadd -g ${GROUP} uactor && useradd -u ${USER} -g ${GROUP} uactor

#  docker build -t uactor_build --build-arg USER=$(id -u) --build-arg GROUP=$(id -g) - < uactor_build.dockerfile
