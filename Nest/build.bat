@echo off

Set PLATFORM=Windows

Set BUILD_ENV=D:\Devkit\bin
Set ZIG_ENV=D:\Zig

Set ANDROID_SDK=D:\AndroidSDK
Set ANDROID_NDK=D:\AndroidNDK

Set ANDROID_PLATROFM_TOOLS=%ANDROID_SDK%\platform-tools
Set ANDROID_BUILD_TOOLS=%ANDROID_SDK%\build-tools\34.0.0
Set ANDROID_CC=%ANDROID_NDK%\toolchains\llvm\prebuilt\windows-x86_64\bin

Set PATH=%BUILD_ENV%;%ZIG_ENV%;%ANDROID_CC%;%ANDROID_PLATROFM_TOOLS%;%ANDROID_BUILD_TOOLS%;%PATH%

D:
cd D:\Grigaror\Nest

If "%1" == "hybrid" Goto server
If "%1" == "server" Goto server
If "%1" == "client" Goto client
If "%1" == "ui" Goto client

Echo Invalid module name.
Goto error

:server
    If "%PLATFORM%" == "Windows" Goto server_windows
    If "%PLATFORM%" == "Linux" Goto server_linux
    
    :server_windows
        Taskkill /f /im "server.exe"
        
        gcc -ggdb -o "server\server.exe" "src\server\server.c" "src\protocol.c" ^
            "libs\desktop\libargon2.a" "libs\desktop\libsqlite3.a" "libs\desktop\libenet.a" -lws2_32 -lwinmm
        If %ERRORLEVEL% NEQ 0 Goto error
        
        gdb "server\server"
        If %ERRORLEVEL% NEQ 0 Goto error
        Goto success
    
    :server_linux:
        zig cc -target x86_64-linux-gnu -O3 -o "server\nest_server" "src\server\server.c" "src\protocol.c" ^
            "libs\server\libargon2.a" "libs\server\libsqlite3.a" "libs\server\libenet.a"
        If %ERRORLEVEL% NEQ 0 Goto error
        Goto success

:client
    If "%PLATFORM%" == "Windows" Goto client_windows
    If "%PLATFORM%" == "Android" Goto client_android
    
    Echo Invalid platform.
    Goto error
    
    :client_windows
        Taskkill /f /im "client.exe"
        
        gcc -O3 "src\client\client.c" "src\protocol.c" "src\client\ui.c" "src\utils.c" "src\client\res\desktop.o" ^
            "libs\desktop\libenet.a" "libs\desktop\libopus.a" "libs\desktop\libraylib.a" ^
            "libs\desktop\libminiaudio.a" "libs\desktop\libsqlite3.a" ^
            -lgdi32 -lopengl32 -lws2_32 -lwinmm -o "client\client.exe"
        If %ERRORLEVEL% NEQ 0 Goto error
        
        REM gdb "client\client"
        If %ERRORLEVEL% NEQ 0 Goto error
        Goto success
        
    :client_android
        Echo Compiling C...
        clang --target=aarch64-linux-android26 -shared -fPIC -Wl,--no-undefined ^
            -I "%ANDROID_NDK%\sources\android\native_app_glue" ^
            "src\client\client.c" "src\protocol.c" "src\client\ui.c" "src\utils.c" "src\client\res\android.o" ^
            "libs\android\libenet.a" "libs\android\libopus.a" "libs\android\libraylib.a" ^
            "libs\android\libminiaudio.a" "libs\android\libsqlite3.a" ^
            %ANDROID_NDK%\sources\android\native_app_glue\android_native_app_glue.c ^
            -landroid -lEGL -lGLESv2 -llog -lm -o "build\libmain.so"
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        REM Echo Compiling Java...
        REM javac -source 8 -target 8 -bootclasspath "%ANDROID_SDK%\platforms\android-26\android.jar" ^
        REM     -d "build\classes" "android\java\com\grigaror\nest\MainActivity.java"
        REM If %ERRORLEVEL% NEQ 0 Goto error
        REM Echo.
        REM 
        REM Echo Dexing...
        REM Call d8 --min-api 26 --lib "%ANDROID_SDK%\platforms\android-26\android.jar" --output "build\dex" ^
        REM     "build\classes\com\grigaror\nest\MainActivity.class"
        REM If %ERRORLEVEL% NEQ 0 Goto error
        REM Echo.
        
        Echo Compiling Java...
        javac -source 8 -target 8 -bootclasspath "%ANDROID_SDK%\platforms\android-26\android.jar" ^
            -d "build\classes" "android\java\com\grigaror\nest\KeyboardHelper.java"
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Echo Archiving JAR...
        jar cf build\classes.jar -C build\classes .
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Echo Dexing...
        Call d8 --min-api 26 --lib "%ANDROID_SDK%\platforms\android-26\android.jar" ^
            --output "build\dex" "build\classes.jar"
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Echo Copying objects...
        mkdir "build\apk\lib\arm64-v8a"
        Copy "build\libmain.so" "build\apk\lib\arm64-v8a\libmain.so"
        Copy "build\dex\classes.dex" "build\apk\classes.dex"
        Copy "android\AndroidManifest.xml" "build\apk\AndroidManifest.xml"
        Echo.
        
        echo Archiving APK...
        aapt2 link -o "build\temp.apk" --manifest "android\AndroidManifest.xml" ^
            -I "%ANDROID_SDK%\platforms\android-26\android.jar" --auto-add-overlay
        If %ERRORLEVEL% NEQ 0 Goto error
        echo.
        
        Echo Adding objects...
        Cd build\apk
        jar uf ..\temp.apk lib classes.dex
        REM jar uf ..\temp.apk lib
        If %ERRORLEVEL% NEQ 0 Goto error
        Cd ..\..
        Echo.
        
        Echo Aligning...
        zipalign -f -p 4 build\temp.apk build\nest.apk
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Echo Signing...
        Call apksigner sign --ks "%ANDROID_SDK%\debug.keystore" --ks-key-alias androiddebugkey ^
            --ks-pass pass:android --key-pass pass:android "build\nest.apk"
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Echo Installing...
        REM adb -s 192.168.1.7 uninstall com.grigaror.nest
        adb -s 192.168.1.7 install -r "build\nest.apk"
        If %ERRORLEVEL% NEQ 0 Goto error
        Echo.
        
        Goto success

:error
    Pause
:success
    Exit
