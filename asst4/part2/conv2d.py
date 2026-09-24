import numpy as np
import math

import neuronxcc.nki as nki
import neuronxcc.nki.language as nl
import neuronxcc.nki.isa as nisa
from neuronxcc.nki import baremetal

"""
A fused convolution - maxpool kernel that you need to implement for Part 2.

Parameters:
    X: the input tensor
    W: the weights of the convolution filters.
    bias: the biases of the convolution filters.
    pool_size: the size of the pool filter and pool stride.

expect: X.shape == [batch_size, in_channels, input_height, input_width]
expect: W.shape == [out_channels, in_channels, filter_height, filter_width]
expect: bias.shape == [out_channels]
expect: filter_height == filter_width
expect: pool_size == 1 || pool_size == 2
expect: input_channels % 128 == 0
expect: output_channels % 128 == 0

out_height = input_height - filter_height + 1
out_width = input_width - filter_width + 1

out_pool_height = out_height // pool_size
out_pool_width = out_width // pool_size

The shape of the output should be [batch_size, out_channels, out_pool_height, out_pool_width]

"""


@nki.compiler.skip_middle_end_transformations
@nki.jit
def fused_conv2d_maxpool(X, W, bias, pool_size=1):
    batch_size, in_channels, input_height, input_width = X.shape
    out_channels, in_channels_, filter_height, filter_width = W.shape
    out_channels_ = bias.shape[0]

    assert (
            in_channels_ == in_channels and out_channels_ == out_channels
    ), f"Shape mismatch. {in_channels}, {in_channels_}, {out_channels}, {out_channels_}"

    out_height = input_height - filter_height + 1
    out_width = input_width - filter_width + 1

    out_pool_height = out_height // pool_size
    out_pool_width = out_width // pool_size

    # Can assume multiple of 128 to avoid using mask
    assert in_channels % 128 == out_channels % 128 == 0

    # Can assume one PSUM bank can at least fit one row of the pixels
    assert nl.tile_size.gemm_moving_fmax >= out_width

    # Initialize output array
    X_out = nl.ndarray(
        shape=(batch_size, out_channels, out_pool_height, out_pool_width),
        dtype=X.dtype,
        buffer=nl.hbm,
    )

    # Various tiling dimensions (You may want to define more of them)
    TILE_C = nl.tile_size.pmax
    n_tiles_c_in = in_channels // TILE_C
    n_tiles_c_out = out_channels // TILE_C

    for co_tile in nl.affine_range(n_tiles_c_out):
        co_start = co_tile * TILE_C
        co_end = co_start * TILE_C

        bias_tile = nl.ndarray((TILE_C, 1), dtype=bias.dtype, buffer=nl.sbuff)
        nisa.dma_copy(
            src=bias[co_start:co_end].reshape((TILE_C, 1)), dst=bias_tile
        )

        weight_tiles = nl.ndarray((TILE_C, filter_height, filter_width, n_tiles_c_in, TILE_C), dtype=W.dtype,
                                  buffer=nl.sbuff)

        for ci_tile in nl.affine_range(n_tiles_c_in):
            ci_start = ci_tile * TILE_C
            ci_end = ci_start + TILE_C

            for fy in nl.affine_range(filter_height):
                for fx in nl.affine_range(filter_width):
                    nisa.dma_transpose(
                        src=W[co_start:co_end, ci_start:ci_end, fy, fx],
                        dst=weight_tiles[:, fy, fx, ci_tile,:],
                        axes=(1, 0),
                    )


            for b in nl.affine_range(batch_size):

                if pool_size == 1:
                    for oy in nl.affine_range(out_height):
                        result_psum = nl.zeros((TILE_C, out_width), dtype=nl.float32, buffer=nl.psum)
                        for fy in nl.affine_range(filter_height):
                            input_y = oy + fy
                            for fx in nl.affine_range(n_tiles_c_in):
                                ci_start = ci_tile * TILE_C
                                ci_end = ci_start + TILE_C
                                x_tile = nl.ndarray((TILE_C, out_width), dtype=X.dtype, buffer=nl.sbuf)
                                nisa.dma_copy(src=X[b, ci_start:ci_end, input_y,fx:fx + out_width], dst=x_tile)
                                result_psum += nisa.nc_matmul(weight_tiles[:fy,fx,ci_tile,:], x_tile)

                        result_sb = nl.copy(result_psum, dtype=X.dtype)
                        result_bias = nisa.tensor_scala(result_sb, nl.addd, bias_tile)
                        # SBUF -> HBM
                        nisa.dma_copy(src=result_bias, dst=X_out[b, co_start:co_end,oy,:])
                else:
                    for py in nl.affine_range(out_pool_height):
                        conv_y0 = py * 2
                        conv_y1 = conv_y0 + 1
                        row0_psum = nl.zeros((TILE_C, out_width), dtype=nl.float32, buffer=nl.psum)
                        for fy in nl.affine_range(filter_height):
                            input_y = conv_y0 + fy
                            for fx in nl.affine_range(filter_width):
                                for ci_tile in nl.affine_range(n_tiles_c_in):
                                    ci_start = ci_tile * TILE_C
                                    ci_end = ci_start + TILE_C
                                    x_tile = nl.ndarray((TILE_C, out_width), dtype=X.dtype, buffer=nl.sbuf)
                                    nisa.dma_copy(src=X[b,ci_start:ci_end,input_y,fx:fx + out_width], dst=x_tile)
                                    row0_psum += nisa.nc_matmul(weight_tiles[:,fy,fx,ci_tile,:], x_tile)
                        row0 = nl.copy(row0_psum, dtype=X.dtype)
                        row0_pairs = row0.reshape((TILE_C, out_pool_width, 2))
                        row0_pool = nisa.tensor_reduce(nl.max, row0_pairs, axis=2)
                        row1_psum = nl.zeros((TILE_C, out_width), dtype=nl.float32, buffer=nl.psum)

                        for fy in nl.affine_range(filter_height):
                            input_y = conv_y1 + fy
                            for fx in nl.affine_range(filter_width):
                                for ci_tile in nl.affine_range(n_tiles_c_in):
                                    ci_start = ci_tile * TILE_C
                                    ci_end = ci_start + TILE_C

                                    x_tile = nl.ndarray((TILE_C, out_width), dtype=X.dtype, buffer=nl.sbuf)
                                    nisa.dma_copy(src=X[b,ci_start:ci_end,input_y,fx:fx + out_width], dst=x_tile)
                                    row1_psum += nisa.nc_matmul(weight_tiles[:,fy,fx,ci_tile,:], x_tile)

                        row1 = nl.copy(row1_psum, dtype=X.dtype)
                        row1_pairs = row1.reshape((TILE_C, out_pool_width, 2))
                        row1_pool = nisa.tensor_reduce(nl.max, row1_pairs, axis=2)
                        pooled = nisa.tensor_tensor(row0_pool, row1_pool, op=nl.maximum)
                        pooled_bias = nisa.tensor_scalar(pooled, nl.add, bias_tile)
                        nisa.dma_copy(src=pooled_bias, dst=X_out[b,co_start:co_end,py,:])
    return X_out
