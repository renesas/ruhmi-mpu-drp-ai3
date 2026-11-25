#
#  DRP-AI Translator Wrapper - Step1: INT8 Quantization
#  (C) Copyright Renesas Electronics Corporation
#
#  Generate an INT8 ONNX from the FP32 ONNX with drpai_quantizer.cli_interface.
#  The quantizer always runs in --tvm mode: the
#  calibration data (<calib_data>/<input name>/<index>.npy) is created by the
#  caller (compile_onnx_model_quant.py) and copied to a working directory.

import os
import sys
import shlex
import shutil
import subprocess


def _build_opts_args(quantization_option):
    """
    Split -p / --quantization_option into an argument list.
    The content is split with shlex and passed through as is; --opts is not
    merged.
    """
    if not quantization_option:
        return []
    return shlex.split(quantization_option)


def _check_calib_data(calib_data):
    """
    Check the calibration data folder: <calib_data>/<input name>/*.npy.
    Raises RuntimeError if the folder or the .npy files are missing.
    """
    if not calib_data or not os.path.isdir(calib_data):
        raise RuntimeError("[Step1] calibration data folder not found: %s" % calib_data)
    found = False
    for name in sorted(os.listdir(calib_data)):
        in_dir = os.path.join(calib_data, name)
        if os.path.isdir(in_dir) and any(f.endswith(".npy") for f in os.listdir(in_dir)):
            found = True
    if not found:
        raise RuntimeError(
            "[Step1] no .npy file under <input name>/ in the calibration data folder: %s"
            % calib_data)


def _prepare_work_dir(model_file, calib_data, work_dir):
    """
    Build the working directory for the --tvm quantizer.

        <work_dir>/
        |-- model.onnx
        `-- input/<input node name>/<index>.npy   (copied from calib_data)
    """
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(work_dir)
    shutil.copyfile(model_file, os.path.join(work_dir, "model.onnx"))
    shutil.copytree(calib_data, os.path.join(work_dir, "input"))


def run_quantization(model_file, opts, output_dir, calib_data):
    """
    Step1: run the INT8 quantization and return the path of the INT8 ONNX.

    Args:
        model_file : input FP32 ONNX
        opts       : opts of arg_parser
        output_dir : output directory (-o). The INT8 ONNX is placed here.
        calib_data : path (str) of the folder that holds the calibration data
                     (.npy files): <calib_data>/<input name>/<index>.npy

    Returns:
        int8_onnx_path : path of the generated INT8 ONNX

    Raises:
        RuntimeError if the calibration data is missing, the quantizer fails
        (non-zero exit) or produces no output.
    """
    quant_tool = opts.get("quantization_tool") or "."

    base = os.path.splitext(os.path.basename(model_file))[0]
    int8_onnx_path = os.path.abspath(os.path.join(output_dir, base + "_INT8.onnx"))

    _check_calib_data(calib_data)

    # Remove any stale output so that a failed run cannot be hidden by it.
    if os.path.exists(int8_onnx_path):
        os.remove(int8_onnx_path)

    work_dir = os.path.abspath(os.path.join(output_dir, base + "_calib"))
    _prepare_work_dir(model_file, os.path.abspath(calib_data), work_dir)

    # The quantizer (v1.13) fails with "TypeError: Argument 'datareader_path'
    # has incorrect type" when --datareader_path is missing, even with --tvm.
    # In --tvm mode the literal string "None" is passed instead (no DataReader).
    datareader_path = "None"
    cmd = [
        sys.executable, "-m", "drpai_quantizer.cli_interface",
        "--input_model", work_dir,
        "--output_model", int8_onnx_path,
        "--datareader_path", datareader_path,
        "--tvm",
    ]
    # Additional quantizer options (-p) are passed through
    cmd += _build_opts_args(opts.get("quantization_option", ""))

    print("-------------------------------------------------")
    print("[Step1] Run INT8 Quantizer")
    print("  mode       :  --tvm")
    print("  input      : ", model_file)
    print("  output     : ", int8_onnx_path)
    print("  calib      : ", calib_data)
    print("  work dir   : ", work_dir)
    print("  datareader : ", datareader_path)
    print("  command    : ", " ".join(cmd))

    # Run with the quantizer directory as the current directory
    env = os.environ.copy()
    ret = subprocess.run(cmd, cwd=os.path.abspath(quant_tool), env=env)

    # A non-zero exit is always an error (no fallback to an existing file).
    if ret.returncode != 0:
        raise RuntimeError("[Step1] Quantization failed (rc=%d)" % ret.returncode)
    if not os.path.exists(int8_onnx_path):
        raise RuntimeError("[Step1] INT8 ONNX not generated: " + int8_onnx_path)

    # The working directory is only needed during quantization.
    shutil.rmtree(work_dir)

    print("[Step1] INT8 ONNX generated: ", int8_onnx_path)
    return int8_onnx_path
