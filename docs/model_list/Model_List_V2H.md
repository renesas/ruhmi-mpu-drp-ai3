# Model list for RZ/V2H

---

Below is a list of AI models that Renesas has verified for conversion with the RUHMI and actual operation on an evaluation board.

**[NOTE]** This benchmark shows the results when only AI inference is running. When system operations such as camera input and display output are performed simultaneously, they may affect the inference time.   

## **Representative models**

| AI model | Input Shape | Inferences/s | Inference time<br>(ms) | Task | Params<br>(M) |
|:--|--:|--:|--:|:--|--:|
|[YOLOv5s](./how_to_convert/How_to_convert_yolov5_onnx_models.md)|640,640|73|13.7|Object Detection|7.3|
|[YOLOv5m](./how_to_convert/How_to_convert_yolov5_onnx_models.md)|640,640|39|25.9|Object Detection|21.4|
|[YOLOv8n](./how_to_convert/How_to_convert_yolov8_onnx_models.md)|640,640|87|11.5|Object Detection|3.1|
|[YOLOv8s](./how_to_convert/How_to_convert_yolov8_onnx_models.md)|640,640|58|17.2|Object Detection|11.2|
|[YOLOv8m](./how_to_convert/How_to_convert_yolov8_onnx_models.md)|640,640|28|35.2|Object Detection|25.9|
|[YOLO11n](./how_to_convert/How_to_convert_yolov11_onnx_models.md)|640,640|79|12.7|Object Detection|2.6|
|[YOLO11s](./how_to_convert/How_to_convert_yolov11_onnx_models.md)|640,640|50|20.0|Object Detection|9.4|
|[YOLO11m](./how_to_convert/How_to_convert_yolov11_onnx_models.md)|640,640|25|39.7|Object Detection|20.1|
|[YOLO26n](./how_to_convert/How_to_convert_yolo26_o2m_onnx_models.md)|640,640|72|13.9|Object Detection|2.4|
|[YOLO26s](./how_to_convert/How_to_convert_yolo26_o2m_onnx_models.md)|640,640|47|21.5|Object Detection|9.5|
|[YOLO26m](./how_to_convert/How_to_convert_yolo26_o2m_onnx_models.md)|640,640|24|41.7|Object Detection|20.4|
|[YOLOX_s](./how_to_convert/How_to_convert_yolox_onnx_models.md)|640,640|68|14.8|Object Detection|9.0|
|[YOLOX_m](./how_to_convert/How_to_convert_yolox_onnx_models.md)|640,640|33|30.1|Object Detection|25.3|
|[MobileNetV2](./how_to_convert/How_to_download_ONNX_models.md)|224,224|833|1.2|Classification|3.5|
|[ResNet50-v1](./how_to_convert/How_to_download_ONNX_models.md)|224,224|244|4.1|Classification|25.6|
|[HRNet](./how_to_convert/How_to_convert_hrnet_onnx_model.md)|256,192|132|7.6|Pose Estimation|28.5|
|[YOLOv8n-pose](./how_to_convert/How_to_convert_yolov8pose_onnx_models.md)|640,640|85|11.7|Multi Person Pose Estimation|3.3|
|[YOLOv8s-pose](./how_to_convert/How_to_convert_yolov8pose_onnx_models.md)|640,640|56|17.8|Multi Person Pose Estimation|11.6|
|[YOLOv8m-pose](./how_to_convert/How_to_convert_yolov8pose_onnx_models.md)|640,640|28|36.1|Multi Person Pose Estimation|26.4|
|[YOLOv8n-seg](./how_to_convert/How_to_convert_yolov8-seg_onnx_models.md)|640,640|71|14.0|Instance Segmentation|3.4|
|[YOLOv8s-seg](./how_to_convert/How_to_convert_yolov8-seg_onnx_models.md)|640,640|47|21.2|Instance Segmentation|11.8|
|[YOLOv8m-seg](./how_to_convert/How_to_convert_yolov8-seg_onnx_models.md)|640,640|23|43.1|Instance Segmentation|27.3|
|[face_landmark](./how_to_convert/How_to_download_ONNX_models.md)|192,192|625|1.6|Face Landmark Detection|0.6|
|[hand_landmark](./how_to_convert/How_to_download_ONNX_models.md)|256,256|370|2.7|Hand Landmark Detection|2.0|
|[MiDaS v2.1 Small](./how_to_convert/How_to_convert_MiDaS_onnx_models.md)|256,256|125|8.0|Depth Estimation|16.6|

---
## **List of models for each task**

:pushpin: UPDATE ARE PLANNED

----   
