from __future__ import division

from logging import getLogger; log = getLogger("mcviz.jet.setup")

import hashlib
import os
import sys
import tarfile
import urllib2

from commands import getstatusoutput
from cStringIO import StringIO
from os import environ, listdir, chdir, getcwd
from os.path import join as pjoin, isdir
from textwrap import dedent
from sys import stdout

# These two are sensitive to import order (!?)
from distutils.core import setup, Extension

from distutils.command.build_ext import build_ext as _build_ext

FASTJET_URL_SHA256HASHES = [
    ("http://www.lpthe.jussieu.fr/~salam/fastjet/repo/fastjet-2.4.3.tar.gz", 
     "0560622140f9f2dfd9e316bfba6a7582c4aac68fbe06f333bd442363f54a3e40"),
]

class FastJetConfigFailed(RuntimeError):
    "fastjet-config didn't run successfully"
    
class FastJetDownloadFailed(RuntimeError):
    "None of the download paths succeeded"

class InvalidSHA(RuntimeError):
    "SHA digest verification failed"

def fastjet_config(prefix=None):
    """
    Get the fastjet C++ configuration/linking flags
    """
    if prefix:
        old_path = environ["PATH"]
        environ["PATH"] = "%s:%s" % (pjoin(prefix, "bin"), environ["PATH"])
    
    bad = False
        
    status, output = getstatusoutput("fastjet-config --libs --rpath")
    if status: bad = True
    
    status, fastjet_prefix = getstatusoutput("fastjet-config --prefix")
    if status: bad = True
        
    if prefix:
        environ["PATH"] = old_path
    
    if bad:
        raise FastJetConfigFailed
    
    return [pjoin(fastjet_prefix, "include")], output.split(" ")
    
def download_file(url):
    """
    Download `url` and return the result as a string.
    """
    log.info("Downloading %s", url)
    
    connection = urllib2.urlopen(url)
    total = int(connection.info().getheader('Content-Length').strip())
    total /= 1024**2
    result = ""
    while True:
        bytes = connection.read(1024 * 10)

        if not bytes:
            break
        
        result += bytes
        
        progress = len(result) / 1024**2
        print "\x1b[1K\rProgress: %.1f MB / %.1f MB" % (progress, total),
        stdout.flush()
    print "\x1b[1K\rDownloaded {0} successfully!".format(url)
    return result

def checksha(data, sha, algorithm):
    """
    Test `data` against `sha` with `algorithm`
    """
    h = algorithm()
    h.update(data)
    if sha != h.hexdigest():
        raise InvalidSHA

def fetch_fastjet(path):
    """
    Try to fetch fastjet from any URL in FASTJET_URL_SHA256HASHES
    """
    for url, sha in FASTJET_URL_SHA256HASHES:
        try:
            data = download_file(url)
            checksha(data, sha, hashlib.sha256)
            tardata = StringIO(data)
            tar = tarfile.open(fileobj=tardata)
            tar.extractall(path)
            return pjoin(path, listdir(path)[0])
        except (urllib2.HTTPError, InvalidSHA):
            continue
        
    raise FastJetDownloadFailed

def install_fastjet(prefix):
    """
    Install fastjet into `prefix`
    """
    
    assert os.access(sys.prefix, os.W_OK), ("Prefix is not writable. You "
        "probably want to install mcviz.jet to a virtualenv. Please follow the "
        "Installation instructions at http://mcviz.net")

    path = pjoin(prefix, "src", "fastjet")
    if not isdir(path):
        os.makedirs(path)
        
    fastjet_path = fetch_fastjet(path)
    
    oldpath = getcwd()
    chdir(fastjet_path)
    
    print "Configuring fastjet"
    
    status, output = getstatusoutput("./configure --prefix={0}".format(prefix))
    if status:
        print "Fastjet configure failed. Output:"
        print output
        raise FastJetConfigFailed("Fastjet ./configure failed in {0}".format(fastjet_path))
    
    print "Building fastjet (this will take a moment)"
    status, output = getstatusoutput("make && make install")
    if status:
        print "Fastjet build failed"
        raise FastJetConfigFailed("Fastjet make failed in {0}".format(fastjet_path))
    
    print ".. fastjet build completed"
    
    chdir(oldpath)

def setup_fastjet():
    """
    Return the fastjet configuration.
    
    * First try the current $PATH for fastjet-config
    * Then try the current sys.prefix (python's prefix)
    * If that didn't work, try to install it to sys.prefix.
    """
    try:
        # Maybe it's available on the system?
        return fastjet_config()
        
    except FastJetConfigFailed:
        try:
            # Maybe it's available in the python prefix?
            return fastjet_config(sys.prefix)
        except FastJetConfigFailed: 
            # Try and install it..
            install_fastjet(sys.prefix)
            return fastjet_config(sys.prefix)

class build_ext(_build_ext, object):
    """
    Override the extension building to build fastjet when we build mcviz.jet.fastjet
    """
    def build_extension(self, ext):
        if ext.name == "mcviz.jet.fastjet":
            ext.include_dirs, ext.extra_link_args = setup_fastjet()
        return super(build_ext, self).build_extension(ext)

version = "0.1"

fastjet = Extension(
    'mcviz.jet.fastjet',
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

    # Build fastjet
    cmdclass={'build_ext': build_ext},
)
