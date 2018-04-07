# GBA Development

This file describes how to develop GBA cartridges with `libgmsys` (Game 
Machine System Library). The development could be decomposed into the 
environment setup, the development, the compiling, the linking, and the 
cartridge creation steps.

The `libgmsys` provides libraries and utilities for the steps to create
a GBA cartridge, and the document will describe them by the steps of
development process.

### Environment Setup
One could be familiar with the `vcvarsall` if he is a windows developer.
Such command will be executed to setup the path for include directories,
archive directories and tools' path.

For the `libgmsys`, there's a similar script file named `gbaenv.src` under 
the `tool` directory. But one should not directly execute such file as
the variables setup in the script will lost soon after it completes its
execution. The correct usage for such script is:

    source /path/to/gbaenv.src
	
The validate whether the environments are setup properly, one could type
`alias` command and he will see a series of commands prefixed with `gmsys`.
And try to execute some of these command like `gmsys-gcc`, he will get 
a message like (I use `arm-none-eabi` as the cross compiling platform):

    arm-none-eabi-gcc: fatal error: no input files
    compilation terminated.
	
Indicating the success of setting up environments.

### Development
I recommend one to read an article named 
<a href="http://problemkaputt.de/gbatek.htm">GBATEK</a>
before the commencement of his development. The following essay assume
the reader has already read this article or has experience in developing
GBA programs.

You will find a GameBoy Advance is no more than a ARM v4 micro-computer 
with special memory layout, peripheral configuration and reduced computing 
power (which limits the application area of GBA and is annoying sometime).

The `libgmsys` aims at providing full and sematically explicit access to
specific memory locations so that the developer to fully utilize the function
of a GBA with its code easily readable and maintainable.

