# Validation Application in C++

This page explains how to use the application provided in this directory, which runs on the target board.  
The application can run various models such as classification, detection, etc. without modifying the runtime program.  

## Index
- [Overview](#overview)
- [File Configuration](#file-configuration)
- [Necessary Environment](#necessary-environment)
- [Model Compilation](#model-compilation)
- [Instructions](#instructions)
  - [1. How to Build the Application](#1-how-to-build-the-application)
  - [2. How to Run the Application](#2-how-to-run-the-application)
- [Application Specification](#application-specification)  
  - [Usage](#usage)  
  - [Sample Debugging Flow](#sample-debugging-flow)
- [Appendix](#appendix)

## Overview  
RUHMI[^1] provides C++ and Python API for running AI models on the target board.  
The Validation Application is a C++ application that runs inference using AI model data compiled by RUHMI[^1] with the following functionalities.  
- Run AI model inference without pre/post-processing.  
- Measure the average AI inference processing time without system dependencies.  
- Obtain the profile data that shows the layer allocation to DRP-AI/CPU.  
- Save AI inference output as a binary file.  
  (Sample pre/post-processing python scripts for ResNet are also provided.)  

The following table shows the target environment.  

| Device | AI SDK | Status |  
| ---- | ---- | ---- |  
| RZ/V2H |[RZ/V2H AI SDK v6.00 or later](https://www.renesas.com/software-tool/rzv2h-ai-software-development-kit) | :white_check_mark: SUPPORTED |  
| RZ/V2N |[RZ/V2N AI SDK v6.30 or later](https://www.renesas.com/software-tool/rzv2n-ai-software-development-kit) | :white_check_mark: SUPPORTED |  


## File Configuration

| File/Directory | Details |  
|:---|:---|  
|CMakeLists.txt |CMake configuration |  
|validation.cpp |C++ application main source code |  
|README.md |This file. Instructions to use the application.|  

## Necessary Environment

Please refer to [Installation](../../README.md#installation) to prepare the necessary environment.  
This page assumes that the above environment has already been prepared and following environment variables have been registered.  

```sh
export REPO_ROOT=<.../ruhmi-rzv-transaltor>   # Your own path to the cloned repository.
export SDK=</opt/poky/5.0.11>                 # Your own Linux SDK path. Set appropriate path according to your product to use.
export TRANSLATOR=<.../DRP-AI_Translator_i8/translator>
export QUANTIZER=<.../DRP-AI_Translator_i8/drpAI_Quantizer>
export PRODUCT=<V2H or V2N>                   # Product name (V2H, or V2N)
```

## Model Compilation

Before running the application, you need to compile the AI models.  
The model compilation process is explained in the [Compile AI models](../../tutorials/README.md) section.  

In this page, we use the ResNet ONNX model as an example, which is `resnet18_onnx` directory compiled with `compile_onnx_model*.py`.

## Instructions
### 1. How to Build the Application

#### 1.1. Prepare the environment

1. Move to the application directory and create `build` directory.

    ```sh
    cd $REPO_ROOT/how-to/validation
    mkdir build && cd build
    ```

2. Run `cmake` command.
    ```sh
    cmake -DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/apps/toolchain/runtime.cmake -DV2H=ON ..
    ```

#### 1.2. Build

1. In the `build` directory, run the `make` command.

    ```sh
    make -j$(nproc)
    ```

2. After running the make command, the following file will be generated in the `build` directory.  
    - `validation`

### 2. How to Run the Application

This section assumes that the user has prepared the Boot Environment on the target board.

#### 2.1. Copy to the Board

Copy the following files to the rootfs of Boot Environment.  

| Name | Path | Details |  
|:---|:---|:---|  
|Model Data | E.g., `${REPO_ROOT}/tutorials/resnet18_onnx`| Model compiled in the [Compile AI models](../../tutorials).  <br>Here, we use ResNet ONNX as an example. |  
|Application |`${REPO_ROOT}/how-to/validation/build/validation` | Application binary file built in this [page](#1-how-to-build-the-application) . |  

The rootfs should have the following directory structure.  

```sh
/
└── home
    └── weston
        └── validation
            ├── lib            
            ├── resnet18_onnx
            │   ├── input0.bin
            │   ├── addr_map.txt
            │   ├── ...
            │   └── weight.bin
            └── validation
```

#### 2.2. Run

After boot-up the board, move to the directory you stored the application and run the `validation` file.  
Please refer to [Usage](#usage) for option details.

```sh
cd ~/validation
./validation -m resnet18_onnx
```

- Following is the expected output for ResNet18 ONNX model compiled for DRP-AI on RZ/V2H Evaluation Board Kit.  
    ```sh
    [INFO ][DrpAiRuntimeWrapper] DRP-AI area: addr=0x240000000 size=0x80000000 (end=0x2c0000000), start_address=0x240000000
    [INFO ][DrpAiRuntimeWrapper] object region: start=0x240000000 size=0xf46440 (end=0x240f46440), area_end=0x2c0000000
    [INFO ][DrpAiRuntimeWrapper] LoadModel succeeded: resnet18_onnx/
    [INFO] Input bin file : resnet18_onnx//input_0.bin
    Warming up...
    Running DRP-AI runtime for 10 times
    [TIME] AI Processing Time (Avg. over 10 runs ) : 2.71 msec.
    ```

> Note: If you encounter `[ERROR] There is no input file`, please prepare `<model_dir>/input*.bin`.  
ResNet sample input data can be generated by following [Pre-processing in Sample Development Flow](#1-pre-processing)

## Application Specification
### Usage  
Validation Application supports the following command-line options.

```sh
./validation -m <model dir> [-n <loop_num>] [-f <freq_index>] [-o] [-p] [-h]
```
|Option| Operand| Description | Required | Default |
|---|:--|:--|---|---|
| `-m`<br> `--model_dir`  | `<model_dir>`| AI model directory compiled by RUHMI[^1].  | Mandatory | N/A |  
| `-n`<br> `--loop-num`  | `<loop_num>`| Number of loops to get the average AI processing time.  | Optional | 10 |  
| `-f`<br> `--freq-index`  | `<freq_index>`| DRP-AI frequency index, which must be 1~127. <br> Valid only for RZ/V2H and RZ/V2N.  | Optional | 1 | 
| `-o`<br> `--output-bin`  | N/A|  Specify this option to save binary output file in `<model_dir>`. <br>See [Saving output file](#saving-output-file)  | Optional | N/A |   
| `-p`<br> `--profilerun`  | N/A|  Specify this option to save profile data files, `*.txt` and `*.csv`, <br>which show the layer allocation to DRP-AI/CPU.<br>See [Saving profile data](#saving-profile-data)  | Optional | N/A |   

#### Saving output file  
Validation Application can save the AI inference result as a binary file.  

To save the file, please run  the command with **`-o` option**.  
Following log is an example.
```sh
root@rzv2h-evk:~/validation# ./validation -m resnet18_onnx -o
[INFO] Enabled saving output binary file.
[INFO] Input bin file : resnet18_onnx/input_0.bin
Warming up...
Running DRP-AI runtime for 10 times
[TIME] AI Processing Time (Avg. over 10 runs ) : 2.23 msec.
Finish file out resnet18_onnx/data_out_0_fp16.bin
```

The output binary file will be saved as `<model_dir>/data_out_*_fp*.bin`.  
> Note: There may be multiple output binary files if the AI model has multiple outputs.  

### Sample Debugging Flow
Validation Application can measure the AI inference time.  
It does not include AI model pre/post-processing, which is required to check accuracy.  
It reads binary data as input and saves the inference results as binary data.  
Combining this application with pre/post-processing in Python code makes debugging easier.

The following procedures demonstrate how to check ResNet model accuracy.  
> Please note that pre/post-processing is designed specifically for ResNet.  
If you would like to use a different model, you need to use its corresponding pre/post-processing.

#### 1. Pre-processing
1. Prepare input image data, apply pre-processing in Python and save as a binary file.  
Following is example Python script.
    ```py
    from PIL import Image
    import numpy as np

    im = Image.open("sample.bmp")
    im = im.resize((224,224),Image.BILINEAR)  # resize to 224x224
    im_np = np.asarray(im)               # convert to numpy array
    im_np = im_np.transpose(2,0,1)       # Transpose to (3,224,224)
    im_np = im_np.astype(np.float32)     # cast to FP32
    im_np = im_np/255                    # data range 0.0 - 1.0
    mean = [0.485, 0.456, 0.406]         # define normalize parameters
    std = [0.229, 0.224, 0.225]          # define normalize parameters
    for i in range(3):                   # apply normalize
        im_np[i] = (im_np[i] - mean[i])/std[i]
    im_np.tofile("input_0.bin")          # save as binary file
    # The input_0.bin size : 3*224*224*4 = 602,112[Byte]
    ```

2. Place `input_0.bin` to `<model_dir>/`.  
For ResNet, overwrite `resnet18_onnx/input_0.bin`.

#### 2. Inference
1. Run `validation` with `-o` option on the target board according to [2. How to Run the Application](#2-how-to-run-the-application) and [Saving output file](#saving-output-file).

2. Check the output is generated as `<model_dir>/data_out_*.bin`.
For Resnet, `resnet18_onnx/data_out_0_fp16.bin` is generated.

#### 3. Post-processing
1. Check Top-5 classification result by applying the post-processing in Python.  
Following is example Python script, which uses [`ImageNetLabels.txt`](../../apps/assets/ImageNetLabels.txt).  
    ```py
    import numpy as np

    def show_top5(pred_out):
        # run softmax
        sum = np.sum(np.exp(pred_out))
        pred_softmax = np.exp(pred_out)/sum
        # show label
        top5 = pred_softmax.argsort()[-5:]
        # open label data
        with open("ImageNetLabels.txt","r") as f:
            labels = f.readlines()
        # Get inference result
        for i in range(5):
            index = top5[4-i]
            name = labels[index+1].replace("\n","")
            score = pred_softmax[index]
            print(f" TOP {i+1} : class {name:<20}, Score : {score:.3f}, index : {index}")

    pred = np.fromfile("resnet18_onnx/data_out_0_fp16.bin",np.float16).astype(np.float32)
    #pred = np.fromfile("resnet18_onnx/data_out_0_fp32.bin",np.float32) # FP32 Case
    show_top5(pred)
    ```

2. You will get results like the following.
    ```sh
    TOP 1 : class beagle              , Score : 0.468, index : 162
    TOP 2 : class English foxhound    , Score : 0.284, index : 167
    TOP 3 : class Walker hound        , Score : 0.146, index : 166
    TOP 4 : class basset              , Score : 0.039, index : 161
    TOP 5 : class bloodhound          , Score : 0.017, index : 163
    ```
If the expected inference results are not obtained, please recheck the normalization coefficients in compilation script, the calibration data settings, the data types and data order in pre and post-processing.  

---  

## Appendix

### RUHMI Runtime Library API

Regarding the list of RUHMI Runtime API used in the application, please see [DRP-AI Wrapper API References](../../docs/Runtime_Wrap.md)

