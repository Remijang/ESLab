import torch
import sys
import os

# -------------------------------------------------------------------------
# Configuration
# -------------------------------------------------------------------------
CHECKPOINT_PATH = "checkpoint-layer3-pa"
INT16_MAX = 32767

def quantize_tensor(tensor):
    """
    Returns:
        quantized_tensor (int16), scale (float)
    """
    if tensor.numel() == 0:
        return tensor, 1.0
        
    # Symmetric quantization
    max_val = torch.max(torch.abs(tensor))
    if max_val == 0:
        scale = 1.0
    else:
        scale = max_val / INT16_MAX

    # Quantize: q = x / scale
    q_tensor = (tensor / scale).round().clamp(-INT16_MAX, INT16_MAX).to(torch.int16)
    
    return q_tensor, scale.item()

def format_1d_array(tensor, indent=4):
    """Formats a 1D tensor as C array content."""
    values = tensor.tolist()
    line = " " * indent + ", ".join(f"{v:d}" for v in values)
    return line

def format_2d_array(tensor, indent=1):
    """Formats a 2D tensor as C array content."""
    lines = []
    outer_indent = "\t" * indent
    inner_indent = " "
    
    rows = tensor.tolist()
    for i, row in enumerate(rows):
        row_str = ", ".join(f"{v:d}" for v in row)
        if i < len(rows) - 1:
            lines.append(f"{outer_indent}{{{row_str}}},")
        else:
            lines.append(f"{outer_indent}{{{row_str}}}")
            
    return "\n".join(lines)

def format_3d_array(tensor):
    """Formats a 3D tensor as C array content."""
    blocks = []
    
    for i in range(tensor.shape[0]):
        # Format the 2D block
        block_str = format_2d_array(tensor[i], indent=0)
        # Indent the whole block
        indented_block = []
        for line in block_str.split('\n'):
            indented_block.append(f"\t{{{line}")
        
        # Close the block
        joined_block = "\n".join(indented_block)
        joined_block = joined_block.replace("\t{", "\t{", 1) # Fix first brace
        
        # Add closing brace for the 2D block
        joined_block += "}"
        
        if i < tensor.shape[0] - 1:
            joined_block += ","
            
        blocks.append(joined_block)
        
    return "\n".join(blocks)

def main():
    if not os.path.exists(CHECKPOINT_PATH):
        print(f"Error: {CHECKPOINT_PATH} not found.")
        return

    # Load weights
    sd = torch.load(CHECKPOINT_PATH, map_location="cpu")
    
    # Extract dimensions from weights to ensure correct C array sizing
    # weight_ih_l0 shape is (hidden_size, input_size)
    hidden_size = sd['rnn.weight_ih_l0'].shape[0]
    input_size = sd['rnn.weight_ih_l0'].shape[1]
    
    # Count layers by looking for keys
    num_layers = 0
    while f'rnn.weight_ih_l{num_layers}' in sd:
        num_layers += 1
        
    output_size = sd['output.weight'].shape[0]

    # ---------------------------------------------------------
    # Prepare Data Groups
    # ---------------------------------------------------------
    
    # 1. Alpha
    alpha_val = sd['alpha'].item()

    # 2. weight_ih (Layer 0 only)
    w_ih_0 = sd['rnn.weight_ih_l0'] # (Hidden, Input)
    
    # 3. weight_ih_layers (Layer 1 to N-1)
    w_ih_layers_list = []
    for i in range(1, num_layers):
        w_ih_layers_list.append(sd[f'rnn.weight_ih_l{i}'])
    
    if w_ih_layers_list:
        w_ih_layers = torch.stack(w_ih_layers_list) # (Num-1, Hidden, Hidden)
    else:
        w_ih_layers = None

    # 4. bias_ih (All Layers)
    b_ih_list = []
    for i in range(num_layers):
        b_ih_list.append(sd[f'rnn.bias_ih_l{i}'])
    b_ih = torch.stack(b_ih_list) # (Num, Hidden)

    # 5. weight_hh (All Layers)
    w_hh_list = []
    for i in range(num_layers):
        w_hh_list.append(sd[f'rnn.weight_hh_l{i}'])
    w_hh = torch.stack(w_hh_list) # (Num, Hidden, Hidden)

    # 6. bias_hh (All Layers)
    b_hh_list = []
    for i in range(num_layers):
        b_hh_list.append(sd[f'rnn.bias_hh_l{i}'])
    b_hh = torch.stack(b_hh_list) # (Num, Hidden)

    # 7. Output Layer
    w_out = sd['output.weight']
    b_out = sd['output.bias']

    # ---------------------------------------------------------
    # Perform Quantization
    # ---------------------------------------------------------
    
    q_w_ih_0, s_w_ih_0 = quantize_tensor(w_ih_0)
    
    if w_ih_layers is not None:
        q_w_ih_layers, s_w_ih_layers = quantize_tensor(w_ih_layers)
    
    q_b_ih, s_b_ih = quantize_tensor(b_ih)
    q_w_hh, s_w_hh = quantize_tensor(w_hh)
    q_b_hh, s_b_hh = quantize_tensor(b_hh)
    q_w_out, s_w_out = quantize_tensor(w_out)
    q_b_out, s_b_out = quantize_tensor(b_out)

    # ---------------------------------------------------------
    # Generate C Code
    # ---------------------------------------------------------
    
    print('#include "rnn.h"\n')
    print('#include "math.h"')
    print('#include "arm_math.h"\n')
    
    print("/*  the following variables should not be able to access beyond this scope */")
    
    # Print Alpha
    print(f"const float rnn_alpha = {alpha_val:.4f};\t // alpha is used to make rnn able to convert the unit between cm and z-axis unit")
    
    # Note on Scaling
    print(f"/* QUANTIZATION SCALES (Use these to dequantize):")
    print(f" * weight_ih scale: {s_w_ih_0:.8f}")
    if w_ih_layers is not None:
        print(f" * weight_ih_layers scale: {s_w_ih_layers:.8f}")
    print(f" * bias_ih scale: {s_b_ih:.8f}")
    print(f" * weight_hh scale: {s_w_hh:.8f}")
    print(f" * bias_hh scale: {s_b_hh:.8f}")
    print(f" * output weight scale: {s_w_out:.8f}")
    print(f" * output bias scale: {s_b_out:.8f}")
    print(f" */\n")

    # weight_ih
    print(f"const int16_t weight_ih[HIDDEN_SIZE][{input_size}] = {{")
    print(format_2d_array(q_w_ih_0))
    print("};\n")

    # weight_ih_layers
    if w_ih_layers is not None:
        print(f"const int16_t weight_ih_layers[LAYER_NUM - 1][HIDDEN_SIZE][HIDDEN_SIZE] = {{")
        print(format_3d_array(q_w_ih_layers))
        print("};\n")

    # bias_ih
    print(f"const int16_t bias_ih[LAYER_NUM][HIDDEN_SIZE] = {{")
    print(format_2d_array(q_b_ih))
    print("};\n")

    # weight_hh
    print(f"const int16_t weight_hh[LAYER_NUM][HIDDEN_SIZE][HIDDEN_SIZE] = {{")
    print(format_3d_array(q_w_hh))
    print("};\n")

    # bias_hh
    print(f"const int16_t bias_hh[LAYER_NUM][HIDDEN_SIZE] = {{")
    print(format_2d_array(q_b_hh))
    print("};\n")

    # weight_output
    print(f"const int16_t weight_output[{output_size}][HIDDEN_SIZE] = {{")
    print(format_2d_array(q_w_out))
    print("};\n")

    # bias_output
    print(f"const int16_t bias_output[{output_size}] = {{{format_1d_array(q_b_out, indent=0)}}};")

if __name__ == "__main__":
    main()