#!/usr/bin/env python3
"""
Deterministic CBOR Test Vector Generator for CDDL Torture Suite.
Zero external dependencies (uses standard library only).
"""
import struct
from typing import Any, Dict, List, Tuple

# --- Minimal Deterministic CBOR Encoder (RFC 8949) ---
def cbor_uint(major: int, val: int) -> bytes:
    prefix = major << 5
    if val < 24:
        return bytes([prefix | val])
    elif val <= 0xFF:
        return struct.pack(">BB", prefix | 24, val)
    elif val <= 0xFFFF:
        return struct.pack(">BH", prefix | 25, val)
    elif val <= 0xFFFFFFFF:
        return struct.pack(">BI", prefix | 26, val)
    else:
        return struct.pack(">BQ", prefix | 27, val)

def cbor_encode(obj: Any) -> bytes:
    if isinstance(obj, bool):
        return b"\xf5" if obj else b"\xf4"
    elif isinstance(obj, int):
        if obj >= 0:
            return cbor_uint(0, obj)
        else:
            return cbor_uint(1, -1 - obj)
    elif isinstance(obj, bytes):
        return cbor_uint(2, len(obj)) + obj
    elif isinstance(obj, str):
        utf8 = obj.encode("utf-8")
        return cbor_uint(3, len(utf8)) + utf8
    elif isinstance(obj, list):
        return cbor_uint(4, len(obj)) + b"".join(cbor_encode(x) for x in obj)
    elif isinstance(obj, dict):
        items = b"".join(cbor_encode(k) + cbor_encode(v) for k, v in obj.items())
        return cbor_uint(5, len(obj)) + items
    elif isinstance(obj, tuple) and len(obj) == 2 and obj[0] == "TAG":
        tag_num, val = obj[1]
        return cbor_uint(6, tag_num) + cbor_encode(val)
    raise TypeError(f"Unsupported type: {type(obj)}")

def tag(num: int, val: Any) -> Tuple[str, Tuple[int, Any]]:
    return ("TAG", (num, val))


# --- Baseline Valid Payload Builder ---
def build_valid_baseline() -> Any:
    # Inner embedded CoMID-like structure (#6.570)
    inner_coid = tag(570, {
        1: ["comid-torture-01", 1],
        2: [{1: tag(111, b"\x2b\x06\x01\x04\x01"), 2: "rev-B"}]
    })
    inner_coid_bstr = cbor_encode(inner_coid)

    # Embedded CBOR Sequence (.cborseq [ + (uint, tstr) ])
    # Note: Concatenated raw items, NOT wrapped in a CBOR array!
    cborseq_payload = (
        cbor_encode(1) + cbor_encode("seq-first") +
        cbor_encode(2) + cbor_encode("seq-second")
    )

    return tag(501, {
        # 0: id -> one-or-more<$id-socket> (using array of 2+ items across early & late plugs)
        0: [
            tag(37, b"\x12\x34" * 8),  # Early plug: UUID
            "abc-9876"                   # Late plug: .regexp "[a-z]{3}-[0-9]{4}" (XSD: whole text, no anchors)
        ],
        # 1: header -> unwrapped-array [ ~base-header, payload-count: uint ],
        #    i.e. [ version: 1, ? profile: tstr, payload-count: uint ]
        1: [1, "urn:ietf:rats:torture", 42],
        # 2: controls -> control-showcase
        2: {
            "uint-bits": (1 << 0) | (1 << 2) | (1 << 7),  # 0x85 (bits 0, 2, 7)
            "bstr-bits": b"\x87",                         # Byte 0 has bits 0, 1, 2, 7 set
            "bounded-uint": 65535,                        # 2 bytes (.size 1..2)
            "bounded-hash": b"\xaa" * 32,                 # 32 bytes
            "svn-threshold": 5,                           # .ge 5
            "temp-offset": -10,                           # .lt 0
            "non-zero": 1,                                # .ne 0
            "strict-color": 4,                            # .within color-enum (1/2/4/8)
            "even-small-int": 8,                          # .le 10 .and even
            "encoded-seq": cborseq_payload
        },
        # 3: choices -> choice-and-cut-test
        3: {
            "type": 1,
            "val": "valid-string-for-type-1",
            "extra-meta": 999
        },
        # 4: embedded -> bstr .cbor inner-coid-tag
        4: inner_coid_bstr,
        # 5: optional-non-empty -> non-empty<sparse-flags-map>
        5: {0: True},
        # 101: $$vendor-extension-group -> 0 .plus 10 (the literal 10). The group socket is a
        # choice, so key 100 ("x" .cat "v1" == "xv1") must NOT appear together with 101.
        101: 10,
        # Negative vendor extension key allowed by * int .lt 0 => any
        -1: "custom-vendor-val"
    })


# --- Negative Test Vectors (Each MUST fail validation) ---
def build_negative_vectors() -> Dict[str, Tuple[str, Any]]:
    vectors = {}

    # 1. Generic one-or-more<T> empty array or 1-element array violation
    v1 = build_valid_baseline()
    v1[1][1][0] = [1500]  # 1-element array! one-or-more<T> is T / [ 2* T ]
    vectors["neg_01_one_or_more_single_elem_array"] = (
        "Must fail: one-or-more<T> forbids a 1-element array [T]; requires bare T or [2* T]",
        v1
    )

    # 2. RFC 9165 .and non-empty<M> violation
    v2 = build_valid_baseline()
    v2[1][1][5] = {}  # Empty map satisfies sparse-flags-map, but fails .and ({ + any => any })
    vectors["neg_02_non_empty_and_combinator"] = (
        "Must fail: Key 5 is an empty map {}, violating .and ({ + any => any })",
        v2
    )

    # 3. Cut (^) Backtracking Prevention
    v3 = build_valid_baseline()
    v3[1][1][3] = {
        "type": 1,
        "val": 12345  # Invalid! type==1 requires tstr. The implicit cut of 'val:' forbids falling back to * tstr => any
    }
    vectors["neg_03_cut_prevents_wildcard_fallback"] = (
        "Must fail: 'val:' implies a cut; uint 12345 cannot fall back to * tstr => any",
        v3
    )

    # 4. .bits Control Violation on uint
    v4 = build_valid_baseline()
    v4[1][1][2]["uint-bits"] = (1 << 3)  # Bit 3 is not in flag-bits (0, 1, 2, 7)
    vectors["neg_04_uint_bits_disallowed_bit"] = (
        "Must fail: Bit 3 is set (0x08), which is not permitted by flag-bits",
        v4
    )

    # 5. .size Control on uint (Byte-width check)
    v5 = build_valid_baseline()
    v5[1][1][2]["bounded-uint"] = 65536  # Requires 3 bytes (0x010000), exceeds .size (1..2)
    vectors["neg_05_uint_size_exceeded"] = (
        "Must fail: 65536 requires 3 bytes, violating uint .size (1..2)",
        v5
    )

    # 6. .cbor Embedded Validation Failure
    v6 = build_valid_baseline()
    bad_inner = tag(570, {1: ["valid-id", "BAD-VERSION-TYPE"], 2: [{2: "rev"}]})
    v6[1][1][4] = cbor_encode(bad_inner)
    vectors["neg_06_embedded_cbor_inner_mismatch"] = (
        "Must fail: Inner .cbor payload has tstr instead of uint for tag-version",
        v6
    )

    # 7. .cborseq Sequence Odd/Trailing Element Failure
    v7 = build_valid_baseline()
    v7[1][1][2]["encoded-seq"] = cbor_encode(1) + cbor_encode("ok") + cbor_encode(2)  # Missing 2nd tstr!
    vectors["neg_07_cborseq_incomplete_tuple"] = (
        "Must fail: .cborseq [ + (uint, tstr) ] has trailing uint without matching tstr",
        v7
    )

    # 8. Unwrapped array (~base-header) value violation
    v8 = build_valid_baseline()
    v8[1][1][1] = [2, "urn:profile", 10]  # version is 2, violates version: uint .eq 1
    vectors["neg_08_unwrapped_array_eq_violation"] = (
        "Must fail: Unwrapped base-header version is 2, violating .eq 1",
        v8
    )

    return vectors


if __name__ == "__main__":
    valid_cbor = cbor_encode(build_valid_baseline())
    print(f"=== VALID BASELINE CBOR ({len(valid_cbor)} bytes) ===")
    print(valid_cbor.hex())
    print()

    for name, (desc, payload) in build_negative_vectors().items():
        raw = cbor_encode(payload)
        print(f"=== {name} ({len(raw)} bytes) ===")
        print(f"Reason: {desc}")
        print(raw.hex())
        print()
