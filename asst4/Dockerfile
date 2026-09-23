# Dockerfile
FROM --platform=linux/amd64 ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV NKI_SIMULATOR=1
ENV NEURON_PLATFORM_TARGET_OVERRIDE=trn2

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    git \
    g++ \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

RUN python3 -m venv /opt/venv

ENV PATH="/opt/venv/bin:$PATH"

RUN pip install --upgrade pip setuptools wheel

RUN pip install \
    nki \
    "neuronx-cc==2.*" \
    numpy \
    --extra-index-url https://pip.repos.neuron.amazonaws.com

CMD ["bash"]