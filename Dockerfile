FROM ubuntu:22.04

ARG PRODUCT="V2H"

# Update gcc/g++ compiler
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python3-pip unzip
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common
RUN apt install -y ca-certificates gpg wget lsb-release
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor > /usr/share/keyrings/kitware-archive-keyring.gpg
RUN echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main" > /etc/apt/sources.list.d/kitware.list
RUN DEBIAN_FRONTEND=noninteractive apt-get update --fix-missing
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake-data=3.28.1-* cmake=3.28.1-*
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libgl1 libjpeg-dev
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y git vim llvm-14 file g++-13 gcc-13

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 1
RUN update-alternatives --config python

# Install SDK
COPY ./*.sh /opt
RUN chmod a+x /opt/*.sh
RUN cd /opt && yes "" | ./*.sh
RUN rm /opt/*.sh

# Install DRP-AI Translator
COPY ./DRP-AI_Translator*-Linux*-x86_64-Install  /opt
RUN chmod a+x /opt/DRP-AI_Translator*-Linux*-x86_64-Install
WORKDIR /opt
RUN yes | DEBIAN_FRONTEND=noninteractive /opt/DRP-AI_Translator*-Linux*-x86_64-Install

# Clone repository
ENV REPO_ROOT="/ruhmi-mpu-drp-ai3"
RUN git clone https://github.com/renesas/ruhmi-mpu-drp-ai3 ${REPO_ROOT}

# Set environment variables
WORKDIR ${REPO_ROOT}
RUN echo 'export SDK="`find /opt/ -name "sysroots"`/../"' >> ~/.bashrc
ENV PYTHONPATH="/opt/DRP-AI_Translator_i8/drpAI_Quantizer/"
ENV TRANSLATOR="/opt/DRP-AI_Translator_i8/translator/"
ENV QUANTIZER="/opt/DRP-AI_Translator_i8/drpAI_Quantizer/"
ENV PRODUCT="${PRODUCT}"

WORKDIR $REPO_ROOT/3rdparty
RUN git clone https://github.com/gabime/spdlog.git

# Set WORKDIR
WORKDIR /$REPO_ROOT/tutorials
