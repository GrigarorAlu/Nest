package com.grigaror.nest;

import android.app.NativeActivity;
import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

public class KeyboardHelper
{
	private static EditText editText;
	
	static
	{
		System.loadLibrary("main");
	}
	
	private static native void nativeOnText(String text);
	
	public static void init(final NativeActivity activity)
	{
		activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				android.util.Log.i("NEST", "created thread");
				editText = new EditText(activity);
				editText.setVisibility(View.INVISIBLE);
				editText.setSingleLine(true);
				editText.setFocusable(true);
				editText.setFocusableInTouchMode(true);
				editText.addTextChangedListener(new TextWatcher()
				{
					@Override
					public void onTextChanged(CharSequence s, int start, int before, int count)
					{
						if (count > 0)
						{
							nativeOnText(s.toString());
							editText.setText("");
						}
					}
					
					@Override
					public void beforeTextChanged(CharSequence s, int a, int b, int c)
					{
					}
					
					@Override
					public void afterTextChanged(Editable s)
					{
					}
				});
				activity.addContentView(editText, new ViewGroup.LayoutParams(1, 1));
			}
		});
	}
	
    public static void show(final NativeActivity activity)
    {
		activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				editText.setVisibility(View.VISIBLE);
				editText.requestFocus();
				InputMethodManager imm = (InputMethodManager)activity.getSystemService(Context.INPUT_METHOD_SERVICE);
				imm.showSoftInput(editText, InputMethodManager.SHOW_IMPLICIT);
			}
		});
    }
    
    public static void hide(final NativeActivity activity)
    {
		activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				InputMethodManager imm = (InputMethodManager)activity.getSystemService(Context.INPUT_METHOD_SERVICE);
				imm.hideSoftInputFromWindow(editText.getWindowToken(), 0);
			}
		});
    }
}
