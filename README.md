# Nest
A very lightweight chat application made with C

## Dependencies
All external dependencies are bundled into the project in `ext/` directory as sourcs and header files. Their directory tree may differ from original, but the file contents are unchanged. Only `build.mk`s are added in order to moduralize the Makefile.

Server:
+ Argon2
+ ENet
+ SQLite3

Client:
+ ENet
+ Miniaudio
+ Opus
+ RayLib
+ SQLite3


## Building
To compile Nest client or server, use `make`. You may also want to specify the platform (PLATFORM= windows/unix), compiler (CC= gcc/zig cc/clang/etc.) and module (MODULE= client/server).

## Android
While I tried to add Android as a client platform, there were some issues and I couldn't finish before my deadline. There is some Android code already, but it doesn't work yet. I will add full support as soon as possible.
