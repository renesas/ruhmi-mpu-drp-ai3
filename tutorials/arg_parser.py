#
#  (C) Copyright Renesas Electronics Corporation
#

import ast
from optparse import OptionParser
import os
import sys

def _parse_norm(parser, name, value, positive):
    """Parse a list string such as [0.1,0.2,0.3] into 3 floats (parser.error otherwise)."""
    try:
        data = ast.literal_eval(value)
        values = [float(v) for v in data]
    except (ValueError, SyntaxError, TypeError):
        parser.error("invalid %s value: %s (expected e.g. [0.1,0.2,0.3])" % (name, value))
    if len(values) != 3:
        parser.error("%s needs 3 values: %s" % (name, value))
    if positive and any(v <= 0 for v in values):
        parser.error("%s must be positive: %s" % (name, value))
    return values


def get_args():
    """
    Parse the command line and return the option dict and the model file name.
      Input  : None (command line arguments)
      Output : (opts: dict, model_file: str)
    """
    usage = "usage: %prog [options] model_file"
    parser = OptionParser(usage)
    parser.set_defaults(
        drp_compiler_dir='Not Setting',
        toolchain_dir='/opt/poky/5.0.11',
        quantization_tool='Not Setting',
        cpu_data_type="float16",
        acl_mode="acl_off",
        # --- Added by the Translator wrapper ---
        # ImageNet values (default of pre_process_imagenet_pytorch). A value given
        # on the command line overrides them.
        norm_mean="[0.485, 0.456, 0.406]",
        norm_std="[0.229, 0.224, 0.225]",
        s_addr="0x00000000",
    )
    # --- RUHMI compatible options ---
    parser.add_option("-d", "--drp_compiler_dir", dest="drp_compiler_dir", help="DRP-AI Translator root directory", metavar="DIR")
    parser.add_option("-t", "--toolchain_dir", dest="toolchain_dir", help="Cross-compilation toolchain root directory", metavar="DIR")
    parser.add_option("-o", "--output_dir", dest="output_dir", help="Output directory", metavar="DIR")
    parser.add_option("-s", "--input_shape", dest="input_shape",
                      help="Input shape of the AI model, e.g. 1,3,224,224 (required)")
    parser.add_option("-q", "--fp16", action="store_true", dest="fp16", help="Convert to FP16")
    parser.add_option("-c", "--quantization_tool", dest="quantization_tool", help="Quantization tool directory", metavar="DIR")
    parser.add_option("-f", "--cpu_data_type", dest="cpu_data_type", type="choice",
                      choices=["float16", "float32"], default="float16",
                      help="Data type of the final output (float16/float32). "
                           "float16 omits the last FP16->FP32 cast",
                      metavar="float16 or float32")
    parser.add_option("--images", dest="image_dir", help="Specifies the directory where calibration images are contained.", metavar="DIR")
    parser.add_option("-p", "--quantization_option", dest="quantization_option", default="", help="drpai quantization option, example -p \"-az\"", metavar="OPTION")

    # --- Options added by the Translator wrapper ---
    parser.add_option("--norm_mean", dest="norm_mean",
                      help="Normalization mean for calibration data and PreProcess, e.g. [0.485,0.456,0.406]", metavar="LIST")
    parser.add_option("--norm_std", dest="norm_std",
                      help="Normalization std for calibration data and PreProcess, e.g. [0.229,0.224,0.225]", metavar="LIST")
    parser.add_option("--s_addr", dest="s_addr",
                      help="Start address for DRP-AI object mapping (default 0x00000000)", metavar="ADDR")

    (options, args) = parser.parse_args()
    opts = vars(options)
    if len(args) != 1:
        parser.error("Expected a model file")

    print("[Check arguments]")
    model_file = str(args[0])
    print("  Input AI model         : ", model_file)

    # Cross-compiler path (SDK)
    try:
        cc_path = os.environ["SDK"]
        opts["toolchain_dir"] = cc_path
        print("  SDK path               : ", opts["toolchain_dir"])
    except KeyError:
        print("  SDK path               : ", opts["toolchain_dir"], " (*default)")

    # DRP-AI Translator path

    if(opts["drp_compiler_dir"]=="Not Setting"):
        try:
            drp_path = os.environ["TRANSLATOR"]
            opts["drp_compiler_dir"] = drp_path
        except KeyError:
            pass # default setting
    if(os.path.isdir(opts["drp_compiler_dir"])):
        print("  DRP-AI Translator path : ", opts["drp_compiler_dir"])
    else:
        print("[ERROR] The path to TRANSLATOR(--drp_compiler_dir) is not exist.")
        sys.exit(2)
    # DRP-AI Quantizer path
    if(opts["quantization_tool"]=="Not Setting"):
        try:
            drp_path = os.environ["QUANTIZER"]
            opts["quantization_tool"] = drp_path
        except KeyError:
            pass # default setting
    if(os.path.isdir(opts["quantization_tool"])):
        print("  DRP-AI Quantizer path : ", opts["quantization_tool"])
    else:
        print("[ERROR] The path to QUANTIZER(--quantization_tool) is not exist.")
        sys.exit(2)
    print("  Output dir             : ", opts["output_dir"])

    # Input shape: required. It is the output shape of PreProcess; the YAML
    # judgement is based on the loaded ONNX.
    if opts["input_shape"] is None:
        print("  [Note] Input shape is not set")
    else:
        try:
            input_shape = tuple([int(dim) for dim in opts["input_shape"].split(',')])
        except ValueError:
            parser.error("invalid -s value: %s (expected e.g. 1,3,224,224)" % opts["input_shape"])
        print("  Input shape (from -s)  : ", input_shape)
        opts["input_shape"] = input_shape

    # Normalization values: 3 numbers each, std must be positive. The parsed
    # lists are stored in opts["norm_mean_list"] / opts["norm_std_list"].
    opts["norm_mean_list"] = _parse_norm(parser, "--norm_mean", opts["norm_mean"], False)
    opts["norm_std_list"] = _parse_norm(parser, "--norm_std", opts["norm_std"], True)

    print("  norm_mean              : ", opts["norm_mean"])
    print("  norm_std               : ", opts["norm_std"])
    print("  Output data type (-f)  : ", opts["cpu_data_type"])

    return opts, model_file


if __name__ == "__main__":
    o, m = get_args()
    print(o)
    print(m)