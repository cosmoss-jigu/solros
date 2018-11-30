#!/usr/bin/env python3
import os
from os.path import join

_CURDIR  = os.path.abspath(os.path.dirname(__file__))

# dir config
SRC_ROOT = os.path.normpath(os.path.join(_CURDIR, ".."))
BLD_ROOT = os.path.normpath(os.path.join(SRC_ROOT, "build"))
TOOLS_ROOT = os.path.normpath(os.path.join(SRC_ROOT, "scripts"))

# my mic id
MY_MIC_ID = 0 # host

if __name__ == "__main__":
    print(globals())
