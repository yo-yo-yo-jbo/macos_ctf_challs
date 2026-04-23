#!/usr/bin/env python3
#
#  Solution
#  --------
#  1. The SUID binary can read ECDSA signatures and messages and validate them.
#  2. The cryptography is solid - besides one case where the messages share a nonce (message 13 and 37).
#  3. Use the nonce to recover private key and sign the required message to get the flag.
#
import hashlib
import os
import subprocess
import sys
import tempfile

# Constnats: NIST P-256 (secp256r1).
P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
A = P - 3
GX = 0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
GY = 0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551

# Fine-tunables
MAGIC = b'https://jonathanbaror.com'
GIMMIE_PATH = '/Users/ctf/gimmie'
QUOTES_PATH = '/Users/ctf/quotes'

def _add(p1, p2):
    """
        Performs point addition.
    """

    # Add points
    if p1 is None:
        return p2
    if p2 is None:
        return p1
    x1, y1 = p1
    x2, y2 = p2
    if x1 == x2:
        if (y1 + y2) % P == 0:
            return None
        lam = (3 * x1 * x1 + A) * pow(2 * y1, -1, P) % P
    else:
        lam = (y2 - y1) * pow(x2 - x1, -1, P) % P
    x3 = (lam * lam - x1 - x2) % P
    y3 = (lam * (x1 - x3) - y1) % P
    return (x3, y3)

def _mul(k, pt):
    """
        Performs point multiplication.
    """

    # Multiply naively
    r = None
    q = pt
    while k:
        if k & 1:
            r = _add(r, q)
        q = _add(q, q)
        k >>= 1
    return r

def _der_int(x):
    """
        Parses an integer out of a DER integer format.
    """

    # Parse
    b = x.to_bytes((x.bit_length() + 7) // 8 or 1, 'big')
    if b[0] & 0x80:
        b = b'\x00' + b
    return b'\x02' + bytes([len(b)]) + b

def parse_der_sig(data):
    """
        Minimal DER parser for ECDSA signatures.
    """

    # Parse DER sequence
    assert data[0] == 0x30, Exception('Not a DER sequence')
    i = 2
    if data[1] & 0x80:
        i = 2 + (data[1] & 0x7f)

    # Parse integer "r"
    assert data[i] == 0x02, Exception('Missing INTEGER r')
    rlen = data[i + 1]
    r = int.from_bytes(data[i + 2:i + 2 + rlen], 'big')

    # Parse integer "s"
    j = i + 2 + rlen
    assert data[j] == 0x02, Exception('Missing INTEGER s')
    slen = data[j + 1]
    s = int.from_bytes(data[j + 2:j + 2 + slen], 'big')

    # Return the integers
    return r, s

def ecdsa_sign(d, msg, k):
    """
        Sign a message with ECDSA.
    """

    # Perform the signature algorithm
    h = int.from_bytes(hashlib.sha256(msg).digest(), 'big') % N
    rx, _ = _mul(k, (GX, GY))
    r = rx % N
    s = pow(k, -1, N) * (h + r * d) % N
    body = _der_int(r) + _der_int(s)
    return b'\x30' + bytes([len(body)]) + body

def collect_pairs():
    """
        Collect signature and message pairs.
    """

    # Iterate all quotes
    pairs = []
    for name in sorted(os.listdir(QUOTES_PATH)):
        if not name.endswith('.sig'):
            continue
        base = name[:-4]
        sig_path = os.path.join(QUOTES_PATH, name)
        msg_path = os.path.join(QUOTES_PATH, base + '.msg')
        if not os.path.isfile(msg_path):
            continue
        with open(sig_path, 'rb') as f:
            sig = f.read()
        with open(msg_path, 'rb') as f:
            msg = f.read()
        r, s = parse_der_sig(sig)
        pairs.append((base, msg, r, s))

    # Return all pairs
    return pairs

def find_r_collision(pairs):
    """
        Find collusions in nonce.
    """

    # Find collisions or return None
    seen = {}
    for entry in pairs:
        r = entry[2]
        if r in seen:
            return seen[r], entry
        seen[r] = entry
    return None

def recover_key(hit1, hit2):
    """
        Given two signatures sharing r, recover (k, d).
    """

    # Recover key
    _, m1, r, s1 = hit1
    _, m2, _, s2 = hit2
    h1 = int.from_bytes(hashlib.sha256(m1).digest(), 'big') % N
    h2 = int.from_bytes(hashlib.sha256(m2).digest(), 'big') % N
    for denom in ((s1 - s2) % N, (s1 + s2) % N):
        if denom == 0:
            continue
        k = (h1 - h2) * pow(denom, -1, N) % N
        rx, _ = _mul(k, (GX, GY))
        if rx % N != r:
            continue
        d = (s1 * k - h1) * pow(r, -1, N) % N
        return k, d

    # Cannot recover key
    raise Exception('key recovery failed: signatures do not fit the nonce-reuse model')

def main():
    """
        Main routine.
    """

    # Collect pairs
    pairs = collect_pairs()
    assert len(pairs) >= 2, Exception('Expecting more signatures')

    # Find nonce collisions
    hit = find_r_collision(pairs)
    assert hit is not None, Exception('No shared nonce found')

    # Recover key
    k, d = recover_key(*hit)

    # Sign the magic string
    sig = ecdsa_sign(d, MAGIC, k)

    # Write files to a temporary directory and run the process
    with tempfile.TemporaryDirectory() as td:
        msg_path = os.path.join(td, 'magic.msg')
        sig_path = os.path.join(td, 'magic.sig')
        with open(msg_path, 'wb') as f:
            f.write(MAGIC)
        with open(sig_path, 'wb') as f:
            f.write(sig)
        proc = subprocess.run([ GIMMIE_PATH, msg_path, sig_path ], capture_output=True)

    # Show the output
    if proc.stdout:
        sys.stdout.write(proc.stdout.decode('utf-8', errors='replace'))

if __name__ == '__main__':
    main()
