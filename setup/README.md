# Installation

## Requirements

Requirements are listed below.

- OS : Ubuntu 22.04  
- Python : 3.10
- Common Packages : git, wget, unzip
- Additional Packages for Docker installation : docker
- Evaluation Board:  RZ/V2H EVK or RZ/V2N EVK
- Related Software Version:
  - DRP-AI Translator
    - RZ/V2H or RZ/V2N
      - [DRP-AI_Translator_i8 v1.11 or later](https://www.renesas.com/software-tool/drp-ai-translator-i8)
      - DRP-AI_Translator_i8-v1.11-Linux-x86_64-Install
  - SDK
    - RZ/V2H
      - [RZ/V2H AI SDK v6.00 or later](https://www.renesas.com/us/en/software-tool/rzv2h-ai-software-development-kit)
      - RTK0EF0180F06000SJ.zip
    - RZ/V2N
      - [RZ/V2N AI SDK v6.30 or later](https://www.renesas.com/us/en/software-tool/rzv2n-ai-software-development-kit)
      - RTK0EF0189F06300SJ.zip

You can install RUHMI MPU DRP-AI3 either [directly on your host system](#installing-ruhmi-mpu-drp-ai3) or [using Docker](#installing-ruhmi-mpu-drp-ai3-with-docker).

## Installing RUHMI MPU DRP-AI3

This section describes how to install RUHMI MPU DRP-AI3 directly on your host system without using Docker.

### 1. Preparation

Before installing RUHMI MPU DRP-AI3, please follow the instructions below to install the software listed in [Requirements](#requirements).

#### Install Required Packages

```bash
# Update package list and install required packages
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y python3-pip unzip
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common
sudo apt install -y ca-certificates gpg wget lsb-release
sudo add-apt-repository ppa:ubuntu-toolchain-r/test

# Add Kitware repository for latest CMake
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo gpg --dearmor > /tmp/kitware-archive-keyring.gpg
sudo cp /tmp/kitware-archive-keyring.gpg /usr/share/keyrings/kitware-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main" | sudo tee /etc/apt/sources.list.d/kitware.list

# Update package list again and install development tools
sudo DEBIAN_FRONTEND=noninteractive apt-get update --fix-missing
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y libgl1 libjpeg-dev
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y git vim llvm-14 file g++-13 gcc-13

# Set Python alternatives
sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 1
sudo update-alternatives --config python
```

#### Install DRP-AI Translator

Install the DRP-AI Translator that you have downloaded from the Renesas website:

```bash
# Set product type
export PRODUCT=V2H  # Change to V2N as needed

# Install DRP-AI Translator
chmod +x $path_to_translator_file/DRP-AI_Translator_i8-*-Install
cd /opt
sudo $path_to_translator_file/DRP-AI_Translator_i8-*-Install
sudo chown -R $(id -u):$(id -g) /opt/DRP-AI_Translator_i8
```

#### Install SDK for RZ/V

1. Download the appropriate SDK for your board:
   - RZ/V2H: [RZ/V2H AI SDK v6.00 or later](https://www.renesas.com/us/en/software-tool/rzv2h-ai-software-development-kit)
   - RZ/V2N: [RZ/V2N AI SDK v6.30 or later](https://www.renesas.com/us/en/software-tool/rzv2n-ai-software-development-kit)

2. Install the SDK:

```bash
# Extract toolchain script for ZIP files, or use directly for .sh files
cd $path_to_downloaded_sdk_file
unzip RTK*.zip
TOOLCHAIN_SCRIPT=$(find ./ -name "*toolchain*.sh" | head -n 1)

chmod +x ${TOOLCHAIN_SCRIPT}

# Install SDK
sudo ${TOOLCHAIN_SCRIPT}
```

### 3. Clone the Repository

```bash
# Set REPO_ROOT directory
export REPO_ROOT=${HOME}/ruhmi-mpu-drp-ai3

# Clone the repository
git clone https://github.com/renesas/ruhmi-mpu-drp-ai3 ${REPO_ROOT}
```

### 4. Set Environment Variables

```bash
# Set SDK path
echo 'export SDK="'$(find /opt/ -name "sysroots")/../'"' >> ~/.bashrc

# Set DRP-AI Translator paths
echo 'export TRANSLATOR="/opt/DRP-AI_Translator_i8/translator/"' >> ~/.bashrc

# Set QUANTIZER for RZ/V2H or RZ/V2N only
echo 'export PYTHONPATH="/opt/DRP-AI_Translator_i8/drpAI_Quantizer/"' >> ~/.bashrc
echo 'export QUANTIZER="/opt/DRP-AI_Translator_i8/drpAI_Quantizer/"' >> ~/.bashrc
echo 'export PRODUCT="V2H"' >> ~/.bashrc
echo 'export REPO_ROOT="${HOME}/ruhmi-mpu-drp-ai3"' >> ~/.bashrc

# Apply changes to current shell
source ~/.bashrc
```

### 5. Setup RUHMI MPU DRP-AI3 Environment

```bash
# Remove python3-yaml to avoid conflicts
sudo apt-get -y purge python3-yaml

# Clone additional dependencies
git clone https://github.com/gabime/spdlog.git ${REPO_ROOT}/3rdparty

```

### 6. Verify Installation

After completing the installation steps, you can verify the installation by running the sample applications provided in the tutorials directory: [tutorials/README.md](../tutorials/README.md).

## Installing RUHMI MPU DRP-AI3 with Docker
### 1. Preparation for Docker

Before installing RUHMI RUHMI MPU DRP-AI3 with Docker, please ensure you have Docker installed and running on your Ubuntu 22.04 system.

```sh
# Install required packages if not already installed
sudo apt update
sudo apt install -y wget unzip
```

### 2. Prepare Required Files

Before proceeding, ensure you have the following files in your working directory:

1. DRP-AI Translator installer (appropriate version for your board)
   - For RZ/V2H or RZ/V2N: Download from [DRP-AI Translator i8 page](https://www.renesas.com/software-tool/drp-ai-translator-i8)
2. SDK package (appropriate version for your board)
   - For RZ/V2H: Download from [RZ/V2H AI SDK page](https://www.renesas.com/us/en/software-tool/rzv2h-ai-software-development-kit)
   - For RZ/V2N: Download from [RZ/V2N AI SDK page](https://www.renesas.com/us/en/software-tool/rzv2n-ai-software-development-kit)
3. Dockerfile for your board

You can download the Dockerfile from the official repository:

```sh
# Download Dockerfile
wget https://raw.githubusercontent.com/renesas/ruhmi-mpu-drp-ai3/main/Dockerfile -O Dockerfile
```

### 3. Prepare SDK Files

```sh
# Set SDK file path
# For RZ/V2H, or RZ/V2N: specify the downloaded ZIP file
SDK_FILE="downloaded_sdk_file"

# Extract toolchain script
mkdir -p sdk_temp
unzip -q ${SDK_FILE} -d sdk_temp
TOOLCHAIN_SCRIPT=$(find sdk_temp -type f -name "*toolchain*.sh" | head -n 1)
```

### 4. Prepare Build Directory

To avoid copying unnecessary files during Docker build, create a separate build directory:

```sh
# Create build directory
mkdir -p docker_build

# Determine DRP-AI Translator filename based on product
TRANSLATOR_FILE="DRP-AI_Translator_i8-v1.12-Linux-x86_64-Install"

# Copy only required files to build directory
cp Dockerfile docker_build/
cp ${TRANSLATOR_FILE} docker_build/
cp ${TOOLCHAIN_SCRIPT} docker_build/
```

### 5. Build Docker Image

```sh
cd docker_build
# Required File Check
# The following are examples of files required for V2H.
tree .
.
├── DRP-AI_Translator_i8-v1.12-Linux-x86_64-Install
├── Dockerfile
└── rz-vlp-glibc-x86_64-core-image-weston-cortexa55-rzv2h-evk-toolchain-5.0.11.sh
# Build Docker image
docker build -t ruhmi_mpu_drp_ai3_image_${USER} -f Dockerfile .
```

**Tip:** If you encounter an error like "404 Not Found," try adding the `--no-cache` option with "`docker build`".

### 6. Run Docker Container

```sh
docker run -it --name ruhmi_mpu_drp_ai3_container_${USER} ruhmi_mpu_drp_ai3_image_${USER}
```
