import os,sys
import pathlib

def GetChimaera():
    util_path = pathlib.Path(__file__).parent.resolve()
    code_generators_path = os.path.dirname(util_path)
    code_generators_path = os.path.dirname(code_generators_path)
    chimaera_path = os.path.dirname(code_generators_path)
    return chimaera_path


CHIMAERA_ROOT = GetChimaera()
