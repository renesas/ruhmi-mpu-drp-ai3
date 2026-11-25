# RUHMI Framework AI Model Compiler for RZ/V (RUHMI MPU DRP-AI3)

[![License](https://img.shields.io/badge/License-LICENSE-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu-green.svg)](./setup/README.md)
[![Python](https://img.shields.io/badge/Python-3.10-green.svg)](./setup/README.md)

Renesas [RUHMI](https://www.renesas.com/en/software-tool/ruhmi-framework) is the name of the AI framework provided by Renesas. RUHMI aims to provide a common AI tool 
environment across MCU and MPU devices.

## Supported Embedded Platforms

- Renesas RZ/V2H Evaluation Board Kit ([How to get](https://www.renesas.com/document/gde/rzv2h-group-handbook))
- Renesas RZ/V2N Evaluation Board Kit ([How to get](https://www.renesas.com/document/gde/rzv2n-group-handbook))

## :book: Introduction

### Overview

To deploy the AI model to DRP-AI on the target board, you need to compile the model with RUHMI MPU DRP-AI3 to generate Runtime Model Data (Compile).  
SDK generated from RZ/V Linux Package and DRP-AI Support Package is required to compile the model.  

After compiled the model, you need to copy the file to the target board (Deploy).  

<img src=./img/deploy_flow_V2H.png width=600>

## :zap: Quick Start Guide
### Installation

- [Installation guide](./setup/README.md)
 
### Compile model with RUHMI MPU DRP-AI3

- [Compiling model for RUHMI MPU DRP-AI3 (RZ/V2H and RZ/V2N)](./tutorials/README.md)

### Build Application & Run inference on board  
- [Application example](./apps)

----
## :clipboard: Confirmed AI Model List
If you want to know which models have been tested by Renesas, please refer to [Model List](./docs/Model_List.md).

----
## :hammer_and_wrench: Advanced Usage Guide

### Sample Applications  
To find more AI examples, please see [How-to](./how-to) page.  

#### [Option] Prune the model with DRP-AI Extension Package

- [What is Pruning and Quantization?](./pruning/README.md)
- [Installing DRP-AI Extention Package](./pruning/setup/README.md)
- [Pruning and retraining by DRP-AI Extension Package](./pruning/how-to/torchvision_resnet50/README.md)

----
## :gear: File Configuration

| Directory | Details |
|:---|:---|
|tutorials |Sample compile script and guides|
|apps |Sample inference application|
|setup | Setup scripts for building a RUHMI environment |
|docs |"Model List", and other documentation|
|img |Image files used in this document|
|how-to | Sample to solve specific problems |
|pruning | Sample scripts to prune the models with DRP-AI Extension Package  |

----
