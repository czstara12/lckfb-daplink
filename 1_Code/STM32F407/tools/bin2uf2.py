#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL UF2 firmware export.
# Created-By: gpt-5
# Signed-off-by: xcwynya

import argparse
import struct
import sys
from pathlib import Path


_MAGIC_START0 = 0x0A324655
_MAGIC_START1 = 0x9E5D5157
_MAGIC_END = 0x0AB16F30
_FAMILY_ID_PRESENT = 0x2000
_PAYLOAD_SIZE = 256
_DATA_SIZE = 476


def _to_uf2(source, base_address, family_id):
    block_count = (len(source) + _PAYLOAD_SIZE - 1) // _PAYLOAD_SIZE

    def make_block(block_number):
        offset = block_number * _PAYLOAD_SIZE
        payload = source[offset:offset + _PAYLOAD_SIZE].ljust(_PAYLOAD_SIZE, b"\x00")
        header = struct.pack(
            "<8I", _MAGIC_START0, _MAGIC_START1, _FAMILY_ID_PRESENT,
            base_address + offset, _PAYLOAD_SIZE, block_number,
            block_count, family_id,
        )
        return header + payload + bytes(_DATA_SIZE - len(payload)) + struct.pack("<I", _MAGIC_END)

    return b"".join(make_block(number) for number in range(block_count))


def _uint32(value):
    try:
        result = int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是整数") from error
    if not 0 <= result <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("必须在 0 到 0xffffffff 之间")
    return result


def _main():
    parser = argparse.ArgumentParser(description="将 BIN 固件转换为 UF2 文件")
    parser.add_argument("input", type=Path, help="输入 BIN 文件")
    parser.add_argument("output", type=Path, help="输出 UF2 文件")
    parser.add_argument("--base-address", type=_uint32, default=0x08010000)
    parser.add_argument("--family-id", type=_uint32, default=0x6D0922FA)
    arguments = parser.parse_args()
    source = arguments.input.read_bytes()
    arguments.output.write_bytes(_to_uf2(source, arguments.base_address, arguments.family_id))


def _self_test():
    source = bytes(range(256)) + b"\xa5"
    result = _to_uf2(source, 0x08010000, 0x6D0922FA)

    assert len(result) == 1024
    assert struct.unpack_from("<8I", result) == (
        0x0A324655, 0x9E5D5157, 0x2000, 0x08010000,
        256, 0, 2, 0x6D0922FA,
    )
    assert result[32:288] == source[:256]
    assert struct.unpack_from("<8I", result, 512)[3:8] == (
        0x08010100, 256, 1, 2, 0x6D0922FA,
    )
    assert result[544] == 0xA5
    assert result[545:800] == bytes(255)
    assert struct.unpack_from("<I", result, 508)[0] == 0x0AB16F30
    assert struct.unpack_from("<I", result, 1020)[0] == 0x0AB16F30


if __name__ == "__main__":
    _self_test() if sys.argv[1:] == ["--self-test"] else _main()
