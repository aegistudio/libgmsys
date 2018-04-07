# Game Machine System Library - Development Environment 
# for GBA (GameBoy Advance)
# @Author Haoran Luo

# Configure the toolchain and start up a shell environment
# for the toolchain to run in.

# Retrieve the root path of the tool.
TOOL_ROOT="`dirname $0`"
TOOL_ROOT=`realpath $TOOL_ROOT`

# Add essential script directories to the path. 
export PATH=$PATH:"$TOOL_ROOT"
export PATH=$PATH:"$TOOL_ROOT/gba"
export PATH=$PATH:"$TOOL_ROOT/common"

# Start up a new bash shell so that the configured 
# environments could be inherited into the shell.
bash
