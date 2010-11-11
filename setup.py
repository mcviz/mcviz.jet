from setuptools import setup
from distutils.core import Extension
from textwrap import dedent

version = "0.1"

prefix = "/home/pwaller/Projects/External/HepInstallArea"

from commands import getstatusoutput
status, link_args = getstatusoutput("{prefix}/bin/fastjet-config --libs --rpath".format(prefix=prefix))

fastjet = Extension(
    'mcviz.jet.fastjet',
    include_dirs=['{prefix}/include'.format(prefix=prefix)],
    extra_link_args=link_args.split(" "),
    extra_compile_args=["-ggdb3"],
    sources=['src/fastjet.cpp'])

setup(
    name='mcviz.jet',
    version=version,
    description="mcviz.jet",
    long_description=dedent("""
        A library used to perform jet algorithms.
    """).strip(),      
    classifiers=[
        'Development Status :: 1 - Alpha',
        'Intended Audience :: Physicists :: Developers',
        'GNU Affero General Public License v3',
    ],
    keywords='mcviz hep jetalgorithm fastjet antikt',
    author='Johannes Ebke and Peter Waller',
    author_email='dev@mcviz.net',
    url='http://mcviz.net',
    license='Affero GPLv3',
    
    namespace_packages=["mcviz"],
    packages=['mcviz.jet'],
    ext_modules=[fastjet],
)
