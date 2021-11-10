from pybind11.setup_helpers import Pybind11Extension, build_ext
import os
from glob import glob
from setuptools import setup

yaml_path = os.path.join(os.environ['YAML_DIR'], 'include')
eigen_path = os.environ['EIGEN_DIR']
fftw_path = os.path.join(os.environ['FFTW_DIR'], 'include/')
cfitsio_path = os.path.join(os.environ['CFITSIO_DIR'], 'include/')
include_dirs = [yaml_path, fftw_path, eigen_path, cfitsio_path,
                                    'include',
                                    '../gbfits'] + glob('../*/include')

ext_modules = [
    Pybind11Extension("wcsfit",
                      sources=["pydir/wcsfit.cpp"],
                      define_macros=[('USE_EIGEN', True),
                                     ('USE_YAML', True),
                                     ('FLEN_CARD', 81)],
                      include_dirs=include_dirs,
                      libraries=['gbdes', 'yaml-cpp', 'cfitsio', 'fftw3'],#, 'eigen'],
                      library_dirs=['obj', '/home/csaunder/software/yaml-cpp/lib',
                                    os.path.join(os.environ['CONDA_PREFIX'], 'lib')],
                      #library_dirs=['build/temp.linux-x86_64-3.8a/']
                      extra_compile_args=['-w'],#['-Wno-sign-compare', '-Wno-reorder']
    )
]

setup(ext_modules=ext_modules)
