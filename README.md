NXTprotocol
===========

NXTprotocol is mostly self-contained, but it does use the following: libcurl, zlib,
nacl http://nacl.cr.yp.to/install.html and of course libwebsockets: http://libwebsockets.org/trac/libwebsockets

On ubuntu -lz -lcurl nacl/randombytes.o nacl/libnacl.a will get it to link

On a mac, you need to modify the link.txt file in libwebsockets/lib/Cmakefiles/test-server.dir and
add libcurl.dylib nacl/randombytes.o nacl/libnacl.a

You might have to install nacl and generate all the nacl files, only a few are actually used (as of now)

Change the following line in libwebsocketsglue.h to a path on your computer
#define NXTPROTOCOL_HTMLFILE "/Users/jl777/NXTprotocol.html"
The NXTprotocol.html file is what is displayed when http://127.0.0.1:7777/ is accessed. It is currently very primitive
but shows how to interface to the websockets.

You also need to copy the test-server.c file into the actual libwebsockets/test-server directory

If all goes well, it should just build

James


