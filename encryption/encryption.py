import os
import shutil
from Crypto.Cipher import AES
from Crypto.Util import Counter
from binascii import unhexlify

MAGIC = b"BOOM"

def encrypt_aes_ctr(fw, key):
    fw_size = len(fw)

    # 16-byte IV – this is the exact 128-bit initial counter
    iv = os.urandom(16)

    # Build a 128-bit big-endian counter starting at iv
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, "big"))

    cipher = AES.new(key, AES.MODE_CTR, counter=ctr)

    # Stream-encrypt the entire firmware
    ciphertext = cipher.encrypt(fw)

    fw_size_bytes = fw_size.to_bytes(4, "big")
    package = MAGIC + fw_size_bytes + iv + ciphertext

    out_name = "firmware_encrypted.bin"
    out_path = os.path.abspath(out_name)
    with open(out_path, "wb") as f:
        f.write(package)

    base_dir = os.path.dirname(os.path.abspath(__file__))
    dest_dir = os.path.join(base_dir, "..", "firmware")
    os.makedirs(dest_dir, exist_ok=True)

    dest_path = os.path.join(dest_dir, out_name)
    shutil.move(out_path, dest_path)
    print("first 16 ciphertext bytes:", ciphertext[:16].hex())
    return dest_path


if __name__ == "__main__":
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
    fw_path = os.path.join(BASE_DIR, "..", "firmware", "firmware.bin")

    KEY_HEX = "aefdc322e127a32095306edb11ec7b1a19d6b7ae58b6840c6adf076009f74d24"
    AES_KEY = unhexlify(KEY_HEX)

    with open(fw_path, "rb") as f:
        fw_file = f.read()

    dest = encrypt_aes_ctr(fw_file, AES_KEY)
    print("Encrypted firmware written to:", dest)
