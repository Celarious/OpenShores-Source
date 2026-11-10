FROM ubuntu:22.04 AS runtime-deps

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libgl1 \
        libglu1-mesa \
        libopenal1 \
        libqt5network5 \
        libqt5opengl5 \
        libqt5sql5-psql \
        libsdl2-2.0-0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 1000 --shell /bin/bash openshores \
    && install -d -o openshores -g openshores /opt/openshores/bin /opt/openshores/lib /var/lib/openshores

FROM runtime-deps AS debug-deps

USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends gdb procps \
    && rm -rf /var/lib/apt/lists/*
USER openshores

FROM runtime-deps AS server-runtime

USER root
COPY bin/ /opt/openshores/bin/
COPY lib/ /opt/openshores/lib/
COPY motds /var/lib/openshores/motds
COPY health-port.sh /opt/openshores/bin/health-port
RUN chmod 0755 /opt/openshores/bin/health-port

ENV QT_QPA_PLATFORM=offscreen
WORKDIR /var/lib/openshores
USER openshores

FROM debug-deps AS server-debug

USER root
COPY bin/ /opt/openshores/bin/
COPY lib/ /opt/openshores/lib/
COPY motds /var/lib/openshores/motds
COPY health-port.sh /opt/openshores/bin/health-port
RUN chmod 0755 /opt/openshores/bin/health-port

ENV QT_QPA_PLATFORM=offscreen
WORKDIR /var/lib/openshores
USER openshores