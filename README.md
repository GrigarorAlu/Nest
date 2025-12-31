# Nest
A very lightweight chat application

## Dependencies
All external dependencies are bundled into the project as `.a` files in `libs/` folder and `.h` files in `src/ext`.

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


## Compiling
To compile Nest client or server, use GCC or LLVM. There's a build script (`build.bat`), but it works only for Windows

#### Example server (UNIX):

`gcc -o nest_server src/server/server.c src/protocol.c libs/server/libargon2.a libs/server/libsqlite3.a libs/server/libenet.a`

#### Example client (Windows, with w64devkit):

`gcc -o client.exe src/client/client.c src/protocol.c src/client/ui.c src/utils.c src/client/res/desktop.o libs/desktop/libenet.a libs/desktop/libopus.a libs/desktop/libraylib.a libs/desktop/libminiaudio.a libs/desktop/libsqlite3.a -lgdi32 -lopengl32 -lws2_32 -lwinmm`

## Android
While I tried to add Android as a client platform, there were some issues and I couldn't finish before my deadline. There is some Android code already, but it doesn't work yet. I will add full support as soon as possible.

# Mess
The project structure and code are very messy right now, but I decided to upload it anyway. This will be fixed as soon as possible.
