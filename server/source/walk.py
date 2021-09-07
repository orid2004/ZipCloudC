import os
import zipfile
import sys
import shutil
import zlib

def compress_file(path, outdir, name):
    zipf = zipfile.ZipFile("/tmp/src.zip", 'w', zipfile.ZIP_DEFLATED)
    zipf.write(os.path.abspath(path))
    zipf.close()
    with open("/tmp/src.zip", "rb") as src:
        with open(f"{outdir}/{name}.zip", "wb") as dst:
            dst.write(zlib.compress(src.read(), 9))
    os.remove("/tmp/src.zip")


def decompress_file(path, outdir):
    with open(path, "rb") as src:
        with open("tmp.zip", "wb") as dst:
            dst.write(zlib.decompress(src.read()))
    with zipfile.ZipFile("tmp.zip", 'r') as zip_ref:
        zip_ref.extractall(outdir)
    os.remove("tmp.zip")


if __name__ == '__main__':
    if sys.argv[1] == "-c":
        compress_file(*sys.argv[2:5])
    elif sys.argv[1] == "-d":
        decompress_file(*sys.argv[2:4])
