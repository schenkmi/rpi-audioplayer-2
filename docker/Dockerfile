FROM debian:trixie

# Setup environment
ENV DEBIAN_FRONTEND noninteractive

ENV GIT_NAME Michael Schenk
ENV GIT_EMAIL michaelschenk72@gmail.com

# The container has no package lists, so need to update first
RUN dpkg --add-architecture i386 && \
    apt-get -o APT::Retries=3 update -y
RUN apt-get -o APT::Retries=3 install -y --no-install-recommends \
        bc \
        build-essential \
        bzr \
        ca-certificates \
        cmake \
        cpio \
        file \
        g++-multilib \
        git \
        pkgconf \
        sudo \
        libc6:i386 \
        libncurses5-dev \
        locales \
        openssh-server \
        python3 \
        python3-flake8 \
        python3-magic \
        python3-nose2 \
        python3-pexpect \
        python3-pytest \
        rsync \
        shellcheck \
        unzip \
        wget \
        mc \
        vim \
        && \
    apt-get -y autoremove && \
    apt-get -y clean

# To be able to generate a toolchain with locales, enable one UTF-8 locale
RUN sed -i 's/# \(en_US.UTF-8\)/\1/' /etc/locale.gen && \
    /usr/sbin/locale-gen

RUN useradd -ms /bin/bash br-user && \
    chown -R br-user:br-user /home/br-user

RUN mkdir -p /etc/sudoers.d/ && \
    echo br-user ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/br-user && \
    chmod 0440 /etc/sudoers.d/br-user

RUN git config --global user.name $GIT_NAME && \
    git config --global user.email $GIT_EMAIL

USER br-user
WORKDIR /home/br-user
ENV HOME /home/br-user
ENV LC_ALL en_US.UTF-8

