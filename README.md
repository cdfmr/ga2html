# ga2html

Convert xml files produced by [GReader-Archive](https://github.com/Aulddays/GReader-Archive) into html pages

Written and tested in OS X, should work in Linux, requires slight modification to be compatible with Windows.

### Requirement

expat

### How to build

gcc -o ga2html -O2 -lexpat ga2html.c

### How to use

ga2html xmlfile1 xmlfile2 ...