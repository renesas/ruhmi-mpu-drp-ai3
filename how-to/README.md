# How to use RUHMI

This directory contains solutions related to Renesas [RUHMI](https://www.renesas.com/en/software-tool/ruhmi-framework) AI compiler for RZ/V.

## Index
- [How to use RUHMI](#how-to-use-ruhmi)
  - [Index](#index)
  - [:rocket: AI Sample Application for RZ/V2H and RZ/V2N](#rocket-ai-sample-application-for-rzv2h-and-rzv2n)
  - [:dart: AI Use Case Applications for RZ/V2H and RZ/V2N](#dart-ai-use-case-applications-for-rzv2h-and-rzv2n)
  - [:gear: Validation Application](#gear-validation-application)
  - [:bulb: Tips](#bulb-tips)

## :rocket: AI Sample Application for RZ/V2H and RZ/V2N

Available sample applications:   

| Application | Model | Source Code | 
|:--- |:---|:---|
| Classification | ResNet50 |[LINK](./sample_app/app_resnet50_cam/) |
| Semantic Segmentation | DeepLabv3 |   [LINK](./sample_app/app_deeplabv3_cam/) | 

## :dart: AI Use Case Applications for RZ/V2H and RZ/V2N

Available samples:

| Application | Model | Source Code |
|:--- |:---|:---|
| Driver Monitoring System | YoloX |[LINK](./use_case_app/Driver_monitoring_system/) |
| Multi Camera Vehicle Detection | YoloX |[LINK](./use_case_app/Multi_camera_vehicle_detection/) |
| Analog Meter Reader | YoloX + UNET |[LINK](./use_case_app/analog_meter_reader/) |

These sample applications are compatible with RZ/V2H AI SDK v6.00, RZ/V2N AI SDK v6.30 or later versions. <br>
The RZ/V2H AI SDK v6.00 and the RZ/V2N AI SDK v6.30 are already available in the links below.

* [RZ/V2H AI SDK](https://www.renesas.com/ja/software-tool/rzv2h-ai-software-development-kit)
* [RZ/V2N AI SDK](https://www.renesas.com/ja/software-tool/rzv2n-ai-software-development-kit)


## :gear: Validation Application
Validation Application is a C++ application that allow users to verify the AI models compiled by RUHMI [^1] on the target board.  

- [Validation Application](./validation)

## :bulb: Tips
- [How to optimize post-processing](tips/post_processing_optimization_method/README.md)
- [How to stabilize AI systems on RZ/V2H or RZ/V2N](tips/system_stabilization_method/README.md)

---

