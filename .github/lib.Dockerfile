ARG BASE_IMAGE=debian:buster

FROM ubuntu:24.04

WORKDIR /build/handlebars.c

RUN apt-get update && apt-get install -y \
        autoconf \
        automake \
        gcc \
        git \
        libjson-c-dev \
        libtalloc-dev \
        libyaml-dev \
        libtool \
        m4 \
        make \
        pkg-config

COPY . .

RUN mkdir -p /dist/handlebars && autoreconf -vif && \
    ./configure --disable-Werror --disable-testing-exports --prefix=/dist/handlebars && \
    make all check install

