package com.grigaror.nest;

import android.app.NativeActivity;
import android.os.Bundle;
import android.content.pm.PackageManager;

public class MainActivity extends NativeActivity
{
    static
    {
        System.loadLibrary("main");
    }
    
    private static native void nativeInit();
    private static native void nativePause();
    private static native void nativeResume();
    private static native void nativeDestroy();
    
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        nativeInit();
    }
    
    @Override
    protected void onPause()
    {
        super.onPause();
        nativePause();
    }
    
    @Override
    protected void onResume()
    {
        super.onResume();
        nativeResume();
    }
    
    @Override
    protected void onDestroy()
    {
        super.onDestroy();
        nativeDestroy();
    }
}
