#
#  (C) Copyright Renesas Electronics Corporation
#

import os
import onnx
from onnx import shape_inference


def _dim_list(tensor_type):
    """Return the shape of an onnx TensorType as a list. Dynamic dims are None."""
    shape = []
    for d in tensor_type.shape.dim:
        if d.HasField("dim_value"):
            shape.append(int(d.dim_value))
        else:
            shape.append(None)
    return shape


def load_io_info(model_file):
    """
    Load the ONNX model and return the input/output node information.
    Inputs that are listed in the initializers (weights) are excluded.

    Returns:
        inputs  : [{"name", "shape"}...]
        outputs : [{"name", "shape"}...]
    """
    model = onnx.load_model(model_file)
    try:
        model = shape_inference.infer_shapes(model)
    except Exception as e:
        print("[WARN] shape_inference failed, use raw graph info:", e)

    initializer_names = {init.name for init in model.graph.initializer}

    inputs = []
    for inp in model.graph.input:
        if inp.name in initializer_names:
            continue
        inputs.append({
            "name": inp.name,
            "shape": _dim_list(inp.type.tensor_type),
        })

    outputs = []
    for out in model.graph.output:
        outputs.append({
            "name": out.name,
            "shape": _dim_list(out.type.tensor_type),
        })

    return inputs, outputs


def is_nchw(shape):
    """
    Judge whether a 4-D input shape is treated as NCHW.
    The channel count is not restricted: every 4-D shape is treated as
    NCHW, following the ONNX convention (Conv etc. require NCHW).
    """
    return shape is not None and len(shape) == 4


def _shape_str(shape):
    return "[" + ", ".join(str(int(s)) for s in shape) + "]"


def _normalize_batch(name, shape):
    """Warn if a fixed batch size other than 1 is given (N is treated as 1)."""
    n = shape[0]
    if n is not None and n != 1:
        print("[WARN] input '%s': batch size N=%d is treated as 1 "
              "(the batch dimension is not used in the YAML)" % (name, n))


def _classify_input(name, shape):
    """
    Validate one input shape and return its kind.

    Returns:
        "nchw" for [N, C, H, W], "nc" for [N, C].
    Raises:
        RuntimeError if the shape is not supported.
    """
    if shape is None or len(shape) not in (2, 4):
        raise RuntimeError(
            "Unsupported input shape %s (input '%s'). "
            "Only [N, C, H, W] (NCHW) and [N, C] are supported." % (shape, name))

    # N (index 0) may be dynamic. Every other dimension must be fixed.
    if any(d is None for d in shape[1:]):
        raise RuntimeError(
            "Dynamic shape is not supported: input '%s' has shape %s "
            "(only the batch dimension N may be dynamic)." % (name, shape))

    if len(shape) == 2:
        kind = "nc"
    else:
        if not is_nchw(shape):
            raise RuntimeError(
                "Unsupported input shape %s (input '%s'): not NCHW. "
                "Only [N, C, H, W] (NCHW) and [N, C] are supported." % (shape, name))
        kind = "nchw"

    _normalize_batch(name, shape)
    return kind


def generate_prepost_yaml(model_file, opts, out_yaml_path):
    """
    Generate a minimal Pre/Post YAML and write it to out_yaml_path.

    Args:
        model_file    : input ONNX (FP32)
        opts          : opts of arg_parser
        out_yaml_path : output YAML path

    Returns:
        out_yaml_path

    Raises:
        RuntimeError if the model has no input or an input shape is not
        supported. No file is written in that case.
    """
    inputs, outputs = load_io_info(model_file)
    if len(inputs) == 0:
        raise RuntimeError("ONNX model has no valid input node: " + model_file)

    out_fp32 = (opts.get("cpu_data_type", "float16") == "float32")

    # Validate every input before anything is written.
    kinds = []
    for in_info in inputs:
        print("[PrePost] input name  : ", in_info["name"])
        print("[PrePost] input shape : ", in_info["shape"])
        kind = _classify_input(in_info["name"], in_info["shape"])
        print("[PrePost] format      : ",
              "NCHW (cast + Transpose)" if kind == "nchw" else "[N, C] (cast only)")
        kinds.append(kind)

    lines = []
    lines.append("#######################################")
    lines.append("# Auto-generated minimal prepost (no pre/post)")
    lines.append("#######################################")
    lines.append("")

    _emit_inputs(lines, inputs, kinds)
    _emit_outputs(lines, outputs, out_fp32)
    _emit_preprocess(lines, inputs, kinds)

    os.makedirs(os.path.dirname(os.path.abspath(out_yaml_path)), exist_ok=True)
    with open(out_yaml_path, "w") as f:
        f.write("\n".join(lines) + "\n")

    print("[PrePost] YAML generated: ", out_yaml_path)
    return out_yaml_path


def _pre_name(index):
    return "pre_in_%d" % index


def _emit_inputs(lines, inputs, kinds):
    """
    Emit input_to_pre / input_to_body.

    NCHW : pre = CHW/fp32 [C,H,W], body = HWC/fp16 [H,W,C]
    [N,C]: pre = C/fp32 [C],       body = C/fp16 [C]
    """
    lines.append("input_to_pre:")
    for i, (info, kind) in enumerate(zip(inputs, kinds)):
        shape = info["shape"]
        if kind == "nchw":
            order, pre_shape = "CHW", shape[1:]
        else:
            order, pre_shape = "C", shape[1:]
        lines.append("  -")
        lines.append('    name: "%s"' % _pre_name(i))
        lines.append('    order: "%s"' % order)
        lines.append("    shape: %s" % _shape_str(pre_shape))
        lines.append('    type: "fp32"')
    lines.append("")

    lines.append("input_to_body:")
    for info, kind in zip(inputs, kinds):
        shape = info["shape"]
        if kind == "nchw":
            order = "HWC"
            _, c, h, w = shape
            body_shape = [h, w, c]
        else:
            order, body_shape = "C", shape[1:]
        lines.append("  -")
        lines.append('    name: "%s"       # must match ONNX input name' % info["name"])
        lines.append('    order: "%s"' % order)
        lines.append("    shape: %s" % _shape_str(body_shape))
        lines.append('    type: "fp16"')
    lines.append("")


def _emit_preprocess(lines, inputs, kinds):
    """Emit preprocess: cast (+ transpose for NCHW) for every input."""
    lines.append("#######################################")
    lines.append("# Preprocess (cast FP32->FP16 [+ Transpose CHW->HWC for NCHW])")
    lines.append("#######################################")
    lines.append("preprocess:")
    for i, (info, kind) in enumerate(zip(inputs, kinds)):
        lines.append("  -")
        lines.append('    src  : ["%s"]' % _pre_name(i))
        lines.append('    dest : ["%s"]' % info["name"])
        lines.append("    operations:")
        lines.append("    -")
        lines.append("      op: cast_fp16_fp32")
        lines.append("      param:")
        lines.append("        CAST_MODE: 1    # FP32 to FP16")
        if kind == "nchw":
            lines.append("    -")
            lines.append("      op: transpose")
            lines.append("      param:")
            lines.append("        WORD_SIZE: 1    # 2Byte (fp16)")
            lines.append("        IS_CHW2HWC: 1   # CHW to HWC")
    lines.append("")


def _raw_out_shape(out):
    """Return the ONNX output shape without None entries (batch included)."""
    return [s for s in (out["shape"] or []) if s is not None]


def _out_body_shape(out):
    """Return the output shape without the batch dimension (None removed)."""
    oshape = out["shape"]
    body_shape = oshape[1:] if oshape and oshape[0] in (1, None) else oshape
    return [s for s in (body_shape or []) if s is not None]


def _is_1abc(out):
    """
    Judge whether an ONNX output is 4-D [1, A, B, C] (batch 1 + 3-D data).
    Such an output is treated as ONNX=CHW / body=HWC and gets a transpose.
    """
    shape = _raw_out_shape(out)
    return len(shape) == 4 and shape[0] == 1


def _order_for(shape):
    """
    Return the order string for the number of dimensions.
      3-D : "HWC"
      2-D : "HW"
      1-D : "C"   (classification output; avoids the 3-D assumption of to_nhwc)
      else: "HWC" (fallback)
    """
    n = len(shape)
    if n == 3:
        return "HWC"
    if n == 2:
        return "HW"
    if n == 1:
        return "C"
    return "HWC"


def _emit_outputs(lines, outputs, out_fp32):
    """
    Emit the output data definitions and postprocess (same for every input kind).
    out_fp32 selects the final output type (True: fp32, False: fp16).

      ONNX output [1, A, B, C] (4-D):
        - output_from_post : CHW, shape [A, B, C], fp32 (fp16 if not out_fp32)
        - output_from_body : HWC, shape [B, C, A], fp16
        - postprocess      : transpose (HWC->CHW) [+ cast (FP16->FP32) if out_fp32]
      others (classification outputs, ...):
        - keep the ONNX layout (no transpose); cast (FP16->FP32) if out_fp32,
          otherwise memcopy (WORD_SIZE 2)
    """
    post_type = "fp32" if out_fp32 else "fp16"
    lines.append("#######################################")
    lines.append("# Output data")
    lines.append("#######################################")
    lines.append("output_from_body:")
    for out in outputs:
        if _is_1abc(out):
            # ONNX [1, A, B, C] -> CHW=[A,B,C] -> body(HWC)=[B,C,A]
            _, a, b, c = _raw_out_shape(out)
            body_shape = [b, c, a]
            order = "HWC"
        else:
            body_shape = _out_body_shape(out)
            order = _order_for(body_shape)
        lines.append("  -")
        lines.append('    name: "%s"' % out["name"])
        lines.append("    shape: %s" % _shape_str(body_shape))
        lines.append('    order: "%s"' % order)
        lines.append('    type: "fp16"')
    lines.append("")
    lines.append("output_from_post:")
    for out in outputs:
        if _is_1abc(out):
            # ONNX [1, A, B, C] -> post(CHW)=[A,B,C]
            _, a, b, c = _raw_out_shape(out)
            post_shape = [a, b, c]
            order = "CHW"
        else:
            post_shape = _out_body_shape(out)
            order = _order_for(post_shape)
        lines.append("  -")
        lines.append('    name: "%s_post"' % out["name"])
        lines.append("    shape: %s" % _shape_str(post_shape))
        lines.append('    order: "%s"' % order)
        lines.append('    type: "%s"' % post_type)
    lines.append("")
    lines.append("#######################################")
    lines.append("# Postprocess ([1,A,B,C]:Transpose HWC->CHW / others: none; "
                 "then cast FP16->FP32 (-f float32) or memcopy for non-4-D (-f float16))")
    lines.append("#######################################")
    lines.append("postprocess:")
    for out in outputs:
        lines.append("  -")
        lines.append('    src: ["%s"]' % out["name"])
        lines.append('    dest: ["%s_post"]' % out["name"])
        lines.append("    operations:")
        if _is_1abc(out):
            lines.append("      -")
            lines.append("        op : transpose")
            lines.append("        param:")
            lines.append("          WORD_SIZE: 1    # 2Byte (fp16)")
            lines.append("          IS_CHW2HWC: 0   # HWC to CHW")
        if out_fp32:
            lines.append("      -")
            lines.append("        op : cast_fp16_fp32")
            lines.append("        param:")
            lines.append("          CAST_MODE: 0 # FP16 to FP32")
        elif not _is_1abc(out):
            # Nothing else would remain: keep the FP16 data as is.
            lines.append("      -")
            lines.append("        op : memcopy")
            lines.append("        param:")
            lines.append("          WORD_SIZE: 2 # FP16/2Byte")
    lines.append("")
