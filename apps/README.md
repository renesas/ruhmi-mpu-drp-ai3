# DRP-AI3 Application Example (RZ/V2H, RZ/V2N)

This page explains how to use the application provided in this directory, which is the example to run ResNet inference (ResNet18 ONNX) on the target board.

## Overview

To run inference with the AI model data compiled by RUHMI MPU DRP-AI3, an inference application is necessary.  
At this point, this application must be written in C++ for the RUHMI MPU DRP-AI3 Runtime Library.  
Here, we explain how to compile and deploy the application example for ResNet models, which have already been compiled in the [Compile with Sample Scripts (RZ/V2H)](../tutorials/README.md).

## Set environment variables

Same as [Installation](../setup/README.md).  

## How to Build the Application

### 1. Prepare the directory

Move to the application directory and create `build` directory.

```sh
cd $REPO_ROOT/apps
mkdir build
cd build
```

Run `cmake` command.

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain/runtime.cmake ..
```

### 2. Build

In the `build` directory, run the `make` command.

```sh
make -j$(nproc)
```

After runinng the make command, following file would be generated in the `build` directory.

- tutorial_app

## How to Run the Application

This section assumes that the user has prepared the Boot Environment on the target board.

### 1. Copy to the Board

Copy the following files to the rootfs of Boot Environment.  

| Name            | Path                                                                                        | Details                                                                                                                                                                   |
|:--------------- |:------------------------------------------------------------------------------------------- |:------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Model Data      | `$REPO_ROOT/tutorials/resnet*`                                                              | Model compiled in the [Compile AI models](../tutorials). DRP-AI Preprocessing Runtime Object files, (`preprocess` directory) are also included.                           |
| Input Data      | `$REPO_ROOT/apps/assets/sample.bmp`                                                            | Windows Bitmap file, which is input data for image classification.                                                                                                        |
| Label List      | `$REPO_ROOT/apps/assets/synset_words_imagenet.txt`<br>`$REPO_ROOT/apps/assets/ImageNetLabels.txt` | `synset_words_imagenet.txt`: Label list for ResNet18 post-processing.<br>`ImageNetLabels.txt`: Label list for ResNet50 post-processing when compiling TensorFlow Hub model. |
| Application     | `$REPO_ROOT/apps/build/tutorial_app`                                                        | Compiled in this [page](#2-build).                                                                                                                 |

The rootfs should look like below.  

```sh
/
└── home
    └── root
        └── drp-ai
            ├── resnet18_onnx                   ... Model Data
            │   ├── aimac_desc.bin
            │   ...
            │   ├── weight.bin
            │   └── preprocess
            │       ├── aimac_desc.bin
            │       ...
            │       └── weight.bin
            ├── assets
            │   ├── sample.bmp ... Input Data
            │   ├── synset_words_imagenet.txt  ... Label List
            │   └── ImageNetLabels.txt         ... Label List
            └── tutorial_app                   ... Application
```

As a working example, a series of commands is described below.


### 2. Run

After boot-up the board, move to the directory you stored the application and run the `tutorial_app` file.  

```sh
cd home/root/drp-ai
./tutorial_app
[INFO ][DrpAiRuntimeWrapper] DRP-AI area: addr=0x240000000 size=0x80000000 (end=0x2c0000000), start_address=0x240000000
[INFO ][DrpAiRuntimeWrapper] object region: start=0x240000000 size=0xd508c0 (end=0x240d508c0), area_end=0x2c0000000
[INFO ][DrpAiRuntimeWrapper] LoadModel succeeded: resnet18_onnx
[TIME] Pre Processing Time: 1.73 msec.
Running runtime (freq_index=1, zero_copy=off)
[TIME] AI Processing Time: 2.30 msec.
Output data type : FP16.
Result ----------------------
  Top 1 [ 74.6%] : [beagle]
  Top 2 [  9.1%] : [English foxhound]
  Top 3 [  4.5%] : [basset, basset hound]
  Top 4 [  0.7%] : [bluetick]
  Top 5 [  0.5%] : [bloodhound, sleuthhound]
```

The application runs the ResNet inference on below image:   
![sample.bmp](assets/sample.bmp).

----