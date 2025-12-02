# =====================================================
# Base: NVIDIA CUDA + cuDNN su Ubuntu 22.04
# =====================================================
FROM nvidia/cuda:12.9.0-cudnn-devel-ubuntu22.04

SHELL ["/bin/bash", "-c"]

# Evita richieste interattive
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8
ENV TZ=Europe/Rome

# =====================================================
# Configurazione APT + pacchetti di base
# =====================================================
RUN echo 'APT::Install-Recommends "0"; APT::Install-Suggests "0";' > /etc/apt/apt.conf.d/01norecommend && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        sudo \
        locales \
        tzdata \
        curl \
        wget \
        gnupg \
        lsb-release \
        software-properties-common \
        build-essential \
        cmake \
        git \
        pkg-config \
        python3-pip \
        python3-tk \
        python3.10-venv \
        vim \
        nano \
        gedit \
        xterm \
        nemo \
        openssl \
        libssl-dev \
        libssh2-1-dev \
        libopencv-dev \
        ffmpeg && \
    locale-gen en_US en_US.UTF-8 && \
    update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 && \
    ln -fs /usr/share/zoneinfo/$TZ /etc/localtime && \
    dpkg-reconfigure --frontend noninteractive tzdata && \
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# =====================================================
# Crea utente non-root con sudo senza password
# =====================================================
ARG USERNAME=ldc
ARG GROUPNAME=ldc
ARG USER_UID=1000
ARG USER_GID=1999

RUN groupadd --gid $USER_GID $GROUPNAME && \
    useradd --create-home --home-dir /home/$USERNAME --shell /bin/bash \
            --uid $USER_UID --gid $USER_GID $USERNAME && \
    usermod -aG sudo $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME && \
    chown $USER_UID:$USER_GID -R /home/$USERNAME

USER $USERNAME
WORKDIR /home/$USERNAME

# =====================================================
# Prompt colorato e VS Code install
# =====================================================
RUN echo 'export PS1="\[\e[1;35m\]\u@\h:\w\$ \[\e[0m\]"' >> ~/.bashrc && \
    sudo apt-get update && sudo apt-get install -y apt-transport-https && \
    wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg && \
    sudo install -D -o root -g root -m 644 packages.microsoft.gpg /etc/apt/keyrings/packages.microsoft.gpg && \
    echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" | \
    sudo tee /etc/apt/sources.list.d/vscode.list > /dev/null && \
    rm -f packages.microsoft.gpg && \
    sudo apt-get update && sudo apt-get install -y code && \
    sudo apt-get clean && sudo rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# =====================================================
# DJI ESDK DEPENDENCIES CHECK
# =====================================================
RUN echo "---- DJI ESDK dependencies check ----" && \
    openssl version && \
    pkg-config --modversion libssh2 || true && \
    pkg-config --modversion opencv4 || true && \
    ffmpeg -version | head -n 1

CMD ["/bin/bash"]
