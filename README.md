tecco - test c code
-------------------
tecco is a UNIX-only unit testing framework for C code.

Requirements
------------
In order to build tecco you need a C compiler, a library archiver such
as `GNU ar` and a "make" program.

Installation
------------
Edit config.mk to match your local setup (tecco is installed into the
/usr/local/ namespace by default).

Afterwards enter the following commands to build and install tecco (if
necessary as root):

    make clean install

Using tecco
------------
Just link libtecco.a to your executable at link time.

Credits
-------
Me.