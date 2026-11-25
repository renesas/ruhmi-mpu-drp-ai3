#
#  (C) Copyright Renesas Electronics Corporation 2026
#

import os
import sys
import shutil
import pathlib
import subprocess
import numpy as np

from arg_parser import get_args
from prepost_yaml_generator import generate_prepost_yaml, load_io_info
from run_quantizer import run_quantization
from run_translator import run_translator

# Number of random calibration samples per input and its fixed seed.
NUM_RANDOM_SAMPLES = 10
RANDOM_SEED = 0
# Image extensions read for the calibration data.
IMAGE_EXTENSIONS = (".jpg", ".jpeg", ".png", ".bmp")


def pre_process_imagenet_pytorch(img, mean=[0.485, 0.456, 0.406], stdev=[0.229, 0.224, 0.225], dims=None, need_transpose=False):
    # Imported here so that these packages are only needed with --images.
    import cv2
    from PIL import Image
    import torchvision.transforms.functional as F

    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = Image.fromarray(img)
    img = F.resize(img, 256, Image.BILINEAR)
    img = F.center_crop(img, 224)
    img = F.to_tensor(img)
    std = stdev
    img = F.normalize(img, mean, std, inplace=False)
    if not need_transpose:
        img = img.permute(1, 2, 0) # NHWC
    img = np.asarray(img, dtype='float32')
    return img


def _pre_process_with_shape(img, mean, stdev, height, width):
    """
    Same as pre_process_imagenet_pytorch() (CHW output) but the image is resized
    directly to the ONNX input size (height, width) instead of 256 + 224 center
    crop. The aspect ratio is not kept and no part of the image is cut off (the
    center crop below keeps the size as it is).
    """
    import cv2
    from PIL import Image
    import torchvision.transforms.functional as F

    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = Image.fromarray(img)
    img = F.resize(img, (height, width), Image.BILINEAR)
    img = F.center_crop(img, [height, width])
    img = F.to_tensor(img)
    img = F.normalize(img, mean, stdev, inplace=False)
    return np.asarray(img, dtype='float32')


def create_calib_from_images(image_dir, inputs, mean, stdev, calib_dir):
    """
    Create <calib_dir>/<input name>/<index>.npy from an image folder.
    Only a model with one NCHW input is supported.
    """
    if len(inputs) != 1 or len(inputs[0]["shape"]) != 4:
        raise RuntimeError(
            "[Step1] --images supports only a model with one NCHW input "
            "(multiple inputs and [N, C] inputs are not supported).")
    if not os.path.isdir(image_dir):
        raise RuntimeError("[Step1] image folder not found: %s" % image_dir)

    name = inputs[0]["name"]
    _, c, h, w = inputs[0]["shape"]
    # Non-image files and sub folders are skipped silently.
    files = sorted(p for p in pathlib.Path(image_dir).iterdir()
                   if p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS)
    if not files:
        raise RuntimeError("[Step1] no image file (%s) in %s"
                           % (", ".join(IMAGE_EXTENSIONS), image_dir))

    import cv2
    out_dir = os.path.join(calib_dir, name)
    os.makedirs(out_dir, exist_ok=True)
    for i, path in enumerate(files):
        image = cv2.imread(str(path))
        if image is None:
            raise RuntimeError("[Step1] failed to read image: %s" % path)
        try:
            data = pre_process_imagenet_pytorch(image, mean, stdev, need_transpose=True)
            if tuple(data.shape) != (c, h, w):
                raise ValueError("shape %s is not the input shape %s"
                                 % (tuple(data.shape), (c, h, w)))
        except ValueError:
            # The input shape is not 224x224: follow the shape of the ONNX.
            data = _pre_process_with_shape(image, mean, stdev, h, w)
        if tuple(data.shape) != (c, h, w):
            raise RuntimeError("[Step1] preprocessed image %s has shape %s but the model input is %s"
                               % (path, tuple(data.shape), (c, h, w)))
        np.save(os.path.join(out_dir, "%d.npy" % i), np.expand_dims(data, 0))
        print("calib data", path)


def create_random_input(inputs, rng):
    """
    Create one random frame: {input name: float32 array}.
    The batch dimension N is 1. A dynamic dimension other than N is an error.
    """
    data = {}
    for inp in inputs:
        shape = inp["shape"]
        if not shape:
            raise RuntimeError("[Step1] input '%s' has no shape information." % inp["name"])
        if any(d is None for d in shape[1:]):
            raise RuntimeError(
                "[Step1] Dynamic shape is not supported: input '%s' has shape %s "
                "(only the batch dimension N may be dynamic)." % (inp["name"], shape))
        data[inp["name"]] = rng.standard_normal([1] + list(shape[1:])).astype(np.float32)
    return data


def create_calib_from_random(inputs, calib_dir):
    """Create <calib_dir>/<input name>/{0..9}.npy from random data."""
    rng = np.random.default_rng(RANDOM_SEED)
    # Create everything first so that an error leaves no files behind.
    frames = [create_random_input(inputs, rng) for _ in range(NUM_RANDOM_SAMPLES)]
    for inp in inputs:
        out_dir = os.path.join(calib_dir, inp["name"])
        os.makedirs(out_dir, exist_ok=True)
        for i, frame in enumerate(frames):
            np.save(os.path.join(out_dir, "%d.npy" % i), frame[inp["name"]])
        print("[Step1] random calibration: %s %s x %d"
              % (inp["name"], list(frames[0][inp["name"]].shape), NUM_RANDOM_SAMPLES))


def create_calibration_data(model_file, opts, output_dir):
    """Create <output_dir>/calib and return its path."""
    calib_dir = os.path.abspath(os.path.join(output_dir, "calib"))
    if os.path.exists(calib_dir):
        shutil.rmtree(calib_dir)
    os.makedirs(calib_dir)

    inputs, _ = load_io_info(model_file)
    if opts.get("image_dir"):
        create_calib_from_images(opts["image_dir"], inputs, opts["norm_mean_list"],
                                 opts["norm_std_list"], calib_dir)
    else:
        create_calib_from_random(inputs, calib_dir)
    return calib_dir


def import_preprocess():
    """Import drpai_preprocess. RuntimeError if it is not available."""
    try:
        from drpai_preprocess import preruntime, drpai_param, op
    except ImportError as e:
        raise RuntimeError("drpai_preprocess is not available (%s). "
                           "It is required to compile PreProcess." % e)
    return preruntime, drpai_param, op


def preprocess_skip_reason(inputs):
    """Return why PreProcess is not generated, or None if it is generated."""
    if any(len(i["shape"]) == 2 for i in inputs):
        return "the model has an [N, C] input"
    if len(inputs) != 1:
        return "the model has multiple inputs"
    return None


def check_input_shape(opts, inputs):
    """-s must agree with the ONNX input shape (batch dimension ignored)."""
    shape = inputs[0]["shape"]
    if(opts["input_shape"]==None):
        return
    given = list(opts["input_shape"])
    if len(given) != len(shape) or given[1:] != list(shape[1:]):
        raise RuntimeError("-s %s does not match the ONNX input shape %s (input '%s')"
                           % (",".join(str(d) for d in given), shape, inputs[0]["name"]))


def main():
    print("=================================================")
    print("RUHMI MPU DRP-AI3")
    print("=================================================")
    # 1. Parse arguments. This comes first so that --help (and argument errors)
    #    work without requiring PRODUCT environment variable.
    opts, model_file = get_args()

    # Validate PRODUCT environment variable
    PRODUCT = os.getenv("PRODUCT")
    if PRODUCT not in ("V2H", "V2N"):
        print("[Error] Environment variable (PRODUCT) is not V2H or V2N")
        print("        Please set e.g. $export PRODUCT=V2H")
        sys.exit(-1)

    if not os.path.isfile(model_file):
        print("[Error] Input ONNX model not found:", model_file)
        sys.exit(-1)

    output_dir = opts["output_dir"]
    if not output_dir:
        print("[Error] Output directory (-o) is required.")
        sys.exit(-1)
    os.makedirs(output_dir, exist_ok=True)

    # drpai_preprocess is required (checked before any long running step).
    try:
        preruntime, drpai_param, op = import_preprocess()
    except RuntimeError as e:
        print("[Error]", e)
        sys.exit(-1)

    print("=================================================")
    print(" DRP-AI Translator Wrapper")
    print("   model      : ", model_file)
    print("   output_dir : ", output_dir)
    print("   product    : ", PRODUCT)
    print("=================================================")

    # Normalization setting (--norm_mean / --norm_std. Default: ImageNet values)
    mean    = opts["norm_mean_list"] # Normalization setting(Mean)
    stdev   = opts["norm_std_list"]  # Normalization setting(Standard deviation)

    # Execute drp-ai optimizer
    opt_model_path = output_dir+"/_opt_fp.onnx"
    translator_dir = os.environ.get("TRANSLATOR")
    opt_scr_path = os.path.join(translator_dir,
                               "DRP-AI_Translator", "onnx_optimizer", "run_onnx_optimizer.py"
                               )
    cmd_opt = ["python3", opt_scr_path,
               "--file_in", model_file, "--file_out", opt_model_path]
    res = subprocess.run(cmd_opt, capture_output=False)
    model_file = opt_model_path

    # 2 & 3. Load the ONNX model, judge the formats and generate the Pre/Post YAML
    prepost_yaml_path = os.path.join(output_dir, "prepost_auto.yaml")
    try:
        generate_prepost_yaml(model_file, opts, prepost_yaml_path)
    except RuntimeError as e:
        print("[Error]", e)
        sys.exit(-1)

    # PreProcess is skipped for [N, C] / multiple inputs; otherwise -s must agree
    # with the ONNX input shape.
    inputs, _ = load_io_info(model_file)
    skip_reason = preprocess_skip_reason(inputs)
    if skip_reason is None:
        try:
            check_input_shape(opts, inputs)
        except RuntimeError as e:
            print("[Error]", e)
            skip_reason = "Need to Check & Modify PreProcess setting"

    # 4. Create the calibration data (.npy) and run Step1: INT8 quantization
    #    (the INT8 ONNX is placed in output_dir)
    try:
        calib_data = create_calibration_data(model_file, opts, output_dir)
        int8_onnx_path = run_quantization(model_file, opts, output_dir, calib_data)
    except RuntimeError as e:
        print("[Error]", e)
        sys.exit(-1)

    # 5 & 6. Step2: Translator conversion -> place the artifacts in output_dir
    run_translator(int8_onnx_path, prepost_yaml_path, opts, output_dir)

    if skip_reason is not None:
        print("-------------------------------------------------")
        print("[PreProcess] skipped: %s" % skip_reason)
    else:
        # Set input shape from onnx shape
        if(opts["input_shape"]==None):
            opts["input_shape"] = inputs[0]["shape"]
            print("   Set Input shape to match the ONNX value:", opts["input_shape"])
        # 4. Compile pre-processing using DRP-AI Pre-processing Runtime Only for RZ/V2H
        # 4.1. Define the pre-processing data
        config = preruntime.Config()

        # 4.1.1. Define input data of preprocessing
        config.shape_in     = [1, 480, 640, 3]
        config.format_in    = drpai_param.FORMAT.BGR
        config.order_in     = drpai_param.ORDER.HWC
        config.type_in      = drpai_param.TYPE.UINT8

        # 4.1.2. Define output data of preprocessing (Will be model input)
        model_shape_in = list(opts["input_shape"])
        config.shape_out    = model_shape_in
        config.format_out   = drpai_param.FORMAT.RGB
        config.order_out    = drpai_param.ORDER.CHW
        config.type_out     = drpai_param.TYPE.FP32
        # Note: type_out depends on DRP-AI. Usually FP32.

        # 4.1.3. Define operators to be run.
        r = 255
        cof_add = [-m*r for m in mean]
        cof_mul = [1/(s*r) for s in stdev]
        config.ops = [
            op.Resize(model_shape_in[3], model_shape_in[2], op.Resize.BILINEAR),
            op.Normalize(cof_add, cof_mul)
        ]

        # 4.2. Run DRP-AI Pre-processing Runtime
        print("-------------------------------------------------")
        print("[PreProcess] compile")

        translator_dir = os.path.abspath(
            os.path.join(opts["drp_compiler_dir"], "..", "translator"))
        os.environ["TRANSLATOR"] = os.path.join(translator_dir, "")

        output_abs = os.path.abspath(output_dir)
        preprocess_dir = os.path.join(output_abs, "preprocess")
        start_dir = os.getcwd()
        try:
            os.chdir(output_abs)
            preruntime.PreRuntime(config, preprocess_dir, PRODUCT)
            yaml_src = os.path.join(output_abs, "preprocess.yaml")
            yaml_dst = os.path.join(preprocess_dir, "preprocess.yaml")
            if os.path.exists(yaml_src):
                os.makedirs(preprocess_dir, exist_ok=True)
                if os.path.exists(yaml_dst):
                    os.remove(yaml_dst)
                shutil.move(yaml_src, yaml_dst)
            else:
                print("[WARN] preprocess.yaml was not found in", output_abs)
        except (Exception, SystemExit) as e:
            print("[Error] PreProcess compile failed:", e)
            sys.exit(-1)
        finally:
            os.chdir(start_dir)
        print("[PreProcess] generated: ", preprocess_dir)

    # Remove the calibration data, the quantizer working directory, temp onnx files 
    onnx_name = os.path.splitext(os.path.basename(model_file))[0]
    for temp_dir in (os.path.join(output_dir, "calib"),
                     os.path.join(output_dir, onnx_name + "_calib")):
        if os.path.isdir(temp_dir):
            shutil.rmtree(temp_dir)
    out_dir_path = pathlib.Path(output_dir).resolve()
    for f in out_dir_path.rglob("*.onnx"):
        print(f"    delete temp onnx: {f}")
        _p = pathlib.Path(f)
        _p.unlink()
        
    print("=================================================")
    print("[Wrapper finished]")
    print("   Please check '{0}' directory".format(output_dir))
    print("=================================================")


if __name__ == "__main__":
    main()
