// ui.c

#include "ui.h"

#include <stdio.h>

#include "../ext/raylib.h"

#include "res/res.h"
#include "../protocol.h"
#include "../utils.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#endif

#define BLACK ColorFromHSV(0, 0, 0.1)
#define DARK ColorFromHSV(0, 0, 0.2)
#define GRAY ColorFromHSV(0, 0, 0.4)
#define MEDIUM ColorFromHSV(0, 0, 0.6)
#define LIGHT ColorFromHSV(0, 0, 0.8)
#define WHITE ColorFromHSV(0, 0, 0.9)
#define NORMAL ColorFromHSV(220, 0.8, 0.6)
#define BRIGHT ColorFromHSV(220, 0.8, 0.8)
#define ERROR ColorFromHSV(0, 0.8, 0.9)

typedef enum
{
    MENU_ACCOUNT,
    MENU_REGISTER,
    MENU_STARTUP,
    MENU_WELCOME,
    MENU_CHAT_MESSAGE,
    MENU_CHAT_CREATE,
    MENU_CHAT_JOIN,
    MENU_CHAT_CALL
} Menu;

typedef struct
{
    int x;
    int y;
} Vector2i;

static const char *statusMsg = 0;
static int statusArg = 0;

static int width = 800;
static int height = 600;

static Font font;
static int menu = MENU_STARTUP;
static int accountId = 0;

static char *msgText = 0;
static size_t msgData = -1;
static char *username = 0;
static size_t usernameData = -1;
static char *password = 0;
static size_t passwordData = -1;

static Account *accounts = 0;
static size_t accountCount = 0;
static Chat *chats = 0;
static size_t chatCount = 0;
static User *users = 0;
static size_t userCount = 0;

static Message *messages = 0;
static size_t messageCount;

static int currentChat = 0;
static int messageScrollPos = 0;
static int messageScrollVel = 0;

static int callChat = 0;

static CallMember *callMembers = 0;
static size_t callMemberCount = 0;

static void (*startup)();
static int (*signup)(const char *, const char *);
static int (*login)(const char *, const char *);
static void (*accountOpen)(int);
static void (*accountSave)(int, const char *, const char *);
static void (*chatCreate)(const char *);
static void (*chatJoin)(const char *);
static void (*messageSend)(int, int, const char *, size_t);
static void (*userRequest)(int);
static void (*messageRequest)(int, int);
static void (*callJoin)(int);
static void (*callLeave)();

#ifdef __ANDROID__
struct android_app *GetAndroidApp();
char lastChar = 0;
int showKeyboard = 0;

JNIEXPORT void JNICALL Java_com_grigaror_nest_KeyboardHelper_nativeOnText(JNIEnv *env, jclass cls, jstring text)
{
    const char *utf = (*env)->GetStringUTFChars(env, text, 0);
    __android_log_print(ANDROID_LOG_INFO, "NEST", utf);
    lastChar = *utf;
    (*env)->ReleaseStringUTFChars(env, text, utf);
}

void initKb()
{
    __android_log_print(ANDROID_LOG_INFO, "NEST", "Test");
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity)
        return;
    
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = 0;
    (*vm)->AttachCurrentThread(vm, &env, 0);
    jobject activity = app->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getClassLoader = (*env)->GetMethodID(env, activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = (*env)->CallObjectMethod(env, activity, getClassLoader);
    jclass classLoaderClass = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID loadClass = (*env)->GetMethodID(env, classLoaderClass,
        "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring className = (*env)->NewStringUTF(env, "com.grigaror.nest.KeyboardHelper");
    jobject clsObj = (*env)->CallObjectMethod(env, classLoader, loadClass, className);
    (*env)->DeleteLocalRef(env, className);
    jclass keyboardHelper = (jclass)clsObj;
    jmethodID showMethod = (*env)->GetStaticMethodID(env, keyboardHelper, "init", "(Landroid/app/NativeActivity;)V");
    (*env)->CallStaticVoidMethod(env, keyboardHelper, showMethod, activity);
    (*vm)->DetachCurrentThread(vm);
}

void showKb()
{
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity)
        return;
    
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = 0;
    (*vm)->AttachCurrentThread(vm, &env, 0);
    jobject activity = app->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getClassLoader = (*env)->GetMethodID(env, activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = (*env)->CallObjectMethod(env, activity, getClassLoader);
    jclass classLoaderClass = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID loadClass = (*env)->GetMethodID(env, classLoaderClass,
        "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring className = (*env)->NewStringUTF(env, "com.grigaror.nest.KeyboardHelper");
    jobject clsObj = (*env)->CallObjectMethod(env, classLoader, loadClass, className);
    (*env)->DeleteLocalRef(env, className);
    jclass keyboardHelper = (jclass)clsObj;
    jmethodID showMethod = (*env)->GetStaticMethodID(env, keyboardHelper, "show", "(Landroid/app/NativeActivity;)V");
    (*env)->CallStaticVoidMethod(env, keyboardHelper, showMethod, activity);
    (*vm)->DetachCurrentThread(vm);
}

void hideKb()
{
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity)
        return;
    
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = 0;
    (*vm)->AttachCurrentThread(vm, &env, 0);
    jobject activity = app->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getClassLoader = (*env)->GetMethodID(env, activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = (*env)->CallObjectMethod(env, activity, getClassLoader);
    jclass classLoaderClass = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID loadClass = (*env)->GetMethodID(env, classLoaderClass,
        "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring className = (*env)->NewStringUTF(env, "com.grigaror.nest.KeyboardHelper");
    jobject clsObj = (*env)->CallObjectMethod(env, classLoader, loadClass, className);
    (*env)->DeleteLocalRef(env, className);
    jclass keyboardHelper = (jclass)clsObj;
    jmethodID showMethod = (*env)->GetStaticMethodID(env, keyboardHelper, "hide", "(Landroid/app/NativeActivity;)V");
    (*env)->CallStaticVoidMethod(env, keyboardHelper, showMethod, activity);
    (*vm)->DetachCurrentThread(vm);
}
#endif

// Stolen from RayLib examples
static void AddCodepointRange(Font *font, const char *data, int dataSize, int start, int stop)
{
    int rangeSize = stop - start + 1;
    int currentRangeSize = font->glyphCount;
    int updatedCodepointCount = currentRangeSize + rangeSize;
    int *updatedCodepoints = malloc(updatedCodepointCount * sizeof(int));

    for (int i = 0; i < currentRangeSize; i++) updatedCodepoints[i] = font->glyphs[i].value;
    for (int i = currentRangeSize; i < updatedCodepointCount; i++)
        updatedCodepoints[i] = start + (i - currentRangeSize);

    UnloadFont(*font);
    *font = LoadFontFromMemory(".ttf", data, dataSize, 200, updatedCodepoints, updatedCodepointCount);
    free(updatedCodepoints);
}

static Vector2i measureText(const char *text, Font font, int fontSize)
{
    Vector2 size = MeasureTextEx(font, text, fontSize, 0);
    return (Vector2i) { size.x, size.y };
}

static void drawText(int x, int y, int w, int h, const char *text, Font font, int fontSize, int align, Color color)
{
    Vector2i size = measureText(text, font, fontSize);
    int padding = (h - size.y) / 2;
    DrawTextEx(font, text, (Vector2) { align ? (align == 2 ? x + w - size.x : x + (w - size.x) / 2) : x + padding,
        y + padding }, fontSize, 0, color);
}

static bool drawButton(int x, int y, int w, int h, const char *text, Font font, int fontSize,
    Color rectNormal, Color textNormal, Color rectActive, Color textActive)
{
    bool hovered = GetMouseX() > x && GetMouseX() < x + w && GetMouseY() > y && GetMouseY() < y + h;
    DrawRectangle(x, y, w, h, hovered ? rectActive : rectNormal);
    drawText(x, y, w, h, text, font, fontSize, true, hovered ? textActive : textNormal);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int drawSlider(int *pressed, int *value, int max, int x, int y, int w, int h, int stroke,
    Color bgNormal, Color bgActive, Color fgNormal, Color fgActive)
{
    bool hovered = GetMouseX() > x && GetMouseX() < x + w && GetMouseY() > y && GetMouseY() < y + h;
    if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        *pressed = true;
    else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        *pressed = false;
    
    DrawRectangle(x, y + (h - stroke) / 2, w, stroke, hovered ? bgActive : bgNormal);
    DrawRectangle(x, y + (h - stroke) / 2, w * *value / max, stroke, hovered ? fgActive : fgNormal);
    DrawRectangle(x + w * *value / max - stroke / 2, y, stroke, h,
        *value > 0 ? (hovered ? fgActive : fgNormal) : (hovered ? bgActive : bgNormal));
    
    if (*pressed)
    {
        *value = (GetMouseX() - x) * max / w;
        *value = *value > 0 ? (*value < max ? *value : max) : 0;
    }
    return *value;
}

static bool drawTextbox(char **text, size_t *data, int x, int y, int w, int h, char *defaultText, Font font,
    int fontSize, Color rectNormal, Color textNormal, Color rectActive, Color textActive, Color textDefault)
{
    bool hovered = GetMouseX() > x && GetMouseX() < x + w && GetMouseY() > y && GetMouseY() < y + h;
    int padding = (h - fontSize) / 2;
    int areaW = w - padding * 2;
    int *cursor = (int *)data;
    int *offset = (int *)data + 1;
    int active = hovered || *cursor >= 0;
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        *cursor = -1;
        if (hovered && *text)
        {
#ifdef __ANDROID__
            showKb();
#endif
            int mouseX = GetMouseX() - x - padding + *offset;
            int minDist = mouseX;
            *cursor = 0;
            for (int i = 0; i <= strlen(*text); i++)
            {
                char temp = (*text)[i];
                (*text)[i] = '\0';
                int dist = mouseX - (int)MeasureTextEx(font, *text, fontSize, 0).x;
                dist = dist < 0 ? -dist : dist;
                (*text)[i] = temp;
                if (dist < minDist)
                {
                    minDist = dist;
                    *cursor = i;
                }
            }
        }
    }
    
    DrawRectangle(x, y, w, h, active ? rectActive : rectNormal);
    
    if (*cursor >= 0)
    {
        size_t length = strlen(*text) + 1;
        *cursor = *cursor < length ? *cursor : length - 1;
#ifdef __ANDROID__
        showKeyboard = true;
        __android_log_print(ANDROID_LOG_INFO, "NEST", text);
        *text = addElement(*text, length++, &lastChar, (*cursor)++, sizeof(char));
#else
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= ' ' && key <= '~')
            {
                char ch = (char)key;
                *text = addElement(*text, length++, &ch, (*cursor)++, sizeof(char));
            }
            key = GetCharPressed();
        }
        if (((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && *cursor))
            *text = removeElement(*text, length--, --(*cursor), sizeof(char));
        else if ((IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) && *cursor < length - 1)
            *text = removeElement(*text, length--, *cursor, sizeof(char));
        else if ((IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) && *cursor > 0)
            (*cursor)--;
        else if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) && *cursor < length - 1)
            (*cursor)++;
        else if (IsKeyPressed(KEY_HOME))
            *cursor = 0;
        else if (IsKeyPressed(KEY_END))
            *cursor = length - 1;
#endif
        
        if (*text)
        {
            char temp = (*text)[*cursor];
            (*text)[*cursor] = '\0';
            int cursorX = (int)MeasureTextEx(font, *text, fontSize, 0).x;
            (*text)[*cursor] = temp;
            
            if (cursorX - *offset < 0)
                *offset = cursorX;
            else if (cursorX - *offset > areaW - 2)
                *offset = cursorX - areaW + 2;
            else if (*offset > cursorX - areaW + 2)
                *offset = cursorX - areaW;
            *offset = *offset > 0 ? *offset : 0;
        }
    }
    else
        *offset = 0;
    
    BeginScissorMode(x, y, w, h);
    if (**text)
        drawText(x - *offset, y, w, h, *text, font, fontSize, false, active ? textActive : textNormal);
    else if (*cursor < 0)
        drawText(x - *offset, y, w, h, defaultText, font, fontSize, false, textDefault);
    EndScissorMode();
    
    if (*cursor < 0)
        return false;
    int cursorX = 0;
    if (*text)
    {
        char temp = (*text)[*cursor];
        (*text)[*cursor] = '\0';
        cursorX = (int)MeasureTextEx(font, *text, fontSize, 0).x;
        (*text)[*cursor] = temp;
    }
    int visibleCursorX = cursorX - *offset;
    if (visibleCursorX >= 0 && visibleCursorX <= areaW - 2)
        DrawRectangle(x + padding + visibleCursorX, y + padding, 2, fontSize, active ? textActive : textNormal);
    
    if (IsKeyPressed(KEY_ENTER))
        *cursor = -1;
    return *cursor == -1;
}

static void drawTextBoxed(int x, int y, int w, int h,
    const char *text, size_t len, Font font, int fontSize, bool align, Color color)
{
    size_t length = len >= 0 ? len : strlen(text);
    float scale = (float)fontSize / font.baseSize;
    int glyphH = fontSize + fontSize / 2;
    
    int offsetY = 0;
    size_t lineStart = 0;

    for (size_t i = 0; i <= length; i++)
    {
        int codepointSize = 0;
        int codepoint = (i < length) ? GetCodepoint(text + i, &codepointSize) : '\n';
        
        if (codepoint != 0x3f && i < length)
            i += codepointSize - 1;

        if (codepoint == '\n' || i == length)
        {
            int lineWidth = 0;
            for (size_t j = lineStart; j < i; j++)
            {
                int cpSize = 0;
                int cp = GetCodepoint(text + j, &cpSize);
                int idx = GetGlyphIndex(font, cp);
                if (cp != 0x3f) j += cpSize - 1;
                
                int glyphW = font.glyphs[idx].advanceX ?
                    font.glyphs[idx].advanceX * scale : font.recs[idx].width * scale;
                if (lineWidth || cp != ' ')
                    lineWidth += glyphW;
            }
            
            if (lineWidth > w)
            {
                size_t wordStart = lineStart;
                size_t currentLineStart = lineStart;
                int currentLineWidth = 0;
                
                for (size_t j = lineStart; j <= i; j++)
                {
                    int cpSize = 0;
                    int cp = (j < i) ? GetCodepoint(text + j, &cpSize) : ' ';
                    int idx = GetGlyphIndex(font, cp);
                    if (cp != 0x3f && j < i)
                        j += cpSize - 1;
                    
                    if (cp == ' ' || cp == '\t' || j == i)
                    {
                        int wordWidth = 0;
                        for (size_t k = wordStart; k < j; k++)
                        {
                            int wCpSize = 0;
                            int wCp = GetCodepoint(text + k, &wCpSize);
                            int wIdx = GetGlyphIndex(font, wCp);
                            if (wCp != 0x3f)
                                k += wCpSize - 1;
                            
                            int glyphW = font.glyphs[wIdx].advanceX ?
                                font.glyphs[wIdx].advanceX * scale : font.recs[wIdx].width * scale;
                            wordWidth += glyphW;
                        }
                        
                        if (currentLineWidth + wordWidth > w && currentLineWidth > 0)
                        {
                            int lineX = align ? x + w - currentLineWidth : x;
                            int drawX = 0;
                            
                            for (size_t k = currentLineStart; k < wordStart; k++)
                            {
                                int wCpSize = 0;
                                int wCp = GetCodepoint(text + k, &wCpSize);
                                int wIdx = GetGlyphIndex(font, wCp);
                                if (wCp != 0x3f)
                                    k += wCpSize - 1;
                                
                                int glyphW = font.glyphs[wIdx].advanceX ?
                                    font.glyphs[wIdx].advanceX * scale : font.recs[wIdx].width * scale;
                                
                                if (wCp != ' ' && wCp != '\t')
                                    DrawTextCodepoint(font, wCp, (Vector2){ lineX + drawX, y + offsetY }, fontSize, color);
                                
                                if (drawX || wCp != ' ')
                                    drawX += glyphW;
                            }
                            
                            offsetY += glyphH;
                            if (offsetY + fontSize > h && h)
                                return;
                            currentLineWidth = 0;
                            currentLineStart = wordStart;
                        }
                        
                        currentLineWidth += wordWidth;
                        
                        if (cp == ' ' || cp == '\t')
                        {
                            int spaceW = font.glyphs[idx].advanceX ?
                                font.glyphs[idx].advanceX * scale : font.recs[idx].width * scale;
                            currentLineWidth += spaceW;
                        }
                        
                        wordStart = j + 1;
                    }
                }
                
                int lineX = align ? x + w - currentLineWidth : x;
                int drawX = 0;
                
                for (size_t k = currentLineStart; k < i; k++)
                {
                    int wCpSize = 0;
                    int wCp = GetCodepoint(text + k, &wCpSize);
                    int wIdx = GetGlyphIndex(font, wCp);
                    if (wCp != 0x3f)
                        k += wCpSize - 1;
                    
                    int glyphW = font.glyphs[wIdx].advanceX ?
                        font.glyphs[wIdx].advanceX * scale : font.recs[wIdx].width * scale;
                    
                    if (wCp != ' ' && wCp != '\t')
                        DrawTextCodepoint(font, wCp, (Vector2){ lineX + drawX, y + offsetY }, fontSize, color);
                    
                    if (drawX || wCp != ' ')
                        drawX += glyphW;
                }
            }
            else
            {
                int lineX = align ? x + w - lineWidth : x;
                int offsetX = 0;
                
                for (size_t j = lineStart; j < i; j++)
                {
                    int cpSize = 0;
                    int cp = GetCodepoint(text + j, &cpSize);
                    int idx = GetGlyphIndex(font, cp);
                    if (cp != 0x3f)
                        j += cpSize - 1;
                    
                    int glyphW = font.glyphs[idx].advanceX ?
                        font.glyphs[idx].advanceX * scale : font.recs[idx].width * scale;
                    
                    if (cp != ' ' && cp != '\t')
                        DrawTextCodepoint(font, cp, (Vector2){ lineX + offsetX, y + offsetY }, fontSize, color);
                    
                    if (offsetX || cp != ' ')
                        offsetX += glyphW;
                }
            }
            
            offsetY += glyphH;
            if (offsetY + fontSize > h && h)
                break;
            lineStart = i + 1;
        }
    }
}

static Vector2i measureTextBoxed(int w, const char *text, size_t len, Font font, int fontSize)
{
    size_t length = len >= 0 ? len : strlen(text);
    float scale = (float)fontSize / font.baseSize;
    int glyphH = fontSize + fontSize / 2;

    int maxX = 0;
    int y = 0;
    size_t lineStart = 0;
    int lineCount = 0;

    for (size_t i = 0; i <= length; i++)
    {
        int codepointSize = 0;
        int codepoint = (i < length) ? GetCodepoint(text + i, &codepointSize) : '\n';
        
        if (codepoint != 0x3f && i < length)
            i += codepointSize - 1;

        if (codepoint == '\n' || i == length)
        {
            int lineWidth = 0;
            for (size_t j = lineStart; j < i; j++)
            {
                int cpSize = 0;
                int cp = GetCodepoint(text + j, &cpSize);
                int idx = GetGlyphIndex(font, cp);
                if (cp != 0x3f)
                    j += cpSize - 1;
                
                int glyphW = font.glyphs[idx].advanceX ?
                    font.glyphs[idx].advanceX * scale : font.recs[idx].width * scale;
                if (lineWidth || cp != ' ')
                    lineWidth += glyphW;
            }
            
            if (lineWidth > w)
            {
                size_t wordStart = lineStart;
                int lineX = 0;
                
                for (size_t j = lineStart; j <= i; j++)
                {
                    int cpSize = 0;
                    int cp = (j < i) ? GetCodepoint(text + j, &cpSize) : ' ';
                    int idx = GetGlyphIndex(font, cp);
                    if (cp != 0x3f && j < i)
                        j += cpSize - 1;
                    
                    if (cp == ' ' || cp == '\t' || j == i)
                    {
                        int wordWidth = 0;
                        for (size_t k = wordStart; k < j; k++)
                        {
                            int wCpSize = 0;
                            int wCp = GetCodepoint(text + k, &wCpSize);
                            int wIdx = GetGlyphIndex(font, wCp);
                            if (wCp != 0x3f)
                                k += wCpSize - 1;
                            
                            int glyphW = font.glyphs[wIdx].advanceX ?
                                font.glyphs[wIdx].advanceX * scale : font.recs[wIdx].width * scale;
                            wordWidth += glyphW;
                        }
                        
                        if (lineX + wordWidth > w && lineX > 0)
                        {
                            if (lineX > maxX)
                                maxX = lineX;
                            lineCount++;
                            lineX = 0;
                        }
                        
                        lineX += wordWidth;
                        
                        if (cp == ' ' || cp == '\t')
                        {
                            int spaceW = font.glyphs[idx].advanceX ?
                                font.glyphs[idx].advanceX * scale : font.recs[idx].width * scale;
                            lineX += spaceW;
                        }
                        
                        wordStart = j + 1;
                    }
                }
                
                if (lineX > maxX)
                    maxX = lineX;
                lineCount++;
            }
            else
            {
                if (lineWidth > maxX)
                    maxX = lineWidth;
                lineCount++;
            }
            
            lineStart = i + 1;
        }
    }
    
    y = lineCount * glyphH;
    
    return (Vector2i){ maxX, y };
}

void UI_open()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(width, height, "Nest");
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(400, 300);
    msgText = strdup("");
    username = strdup("");
    password = strdup("");
    font = LoadFontFromMemory(".ttf", regularFontStart, regularFontEnd - regularFontStart, 200, 0, 0);
    AddCodepointRange(&font, regularFontStart, regularFontEnd - regularFontStart, 0x80, 0x17f);
    AddCodepointRange(&font, regularFontStart, regularFontEnd - regularFontStart, 0x400, 0x45F);
    AddCodepointRange(&font, regularFontStart, regularFontEnd - regularFontStart, 0x25A0, 0x25CF);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
#ifdef __ANDROID__
    initKb();
#endif
}

int UI_draw()
{
    if (!IsWindowReady())
        return 1;
    
    int mouseX = GetMouseX();
    int mouseY = GetMouseY();
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    
    BeginDrawing();
    ClearBackground(BLACK);
    
#ifdef __ANDROID__
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        showKeyboard = false;
#endif
    
    if (menu == MENU_STARTUP)
    {
        startup();
        menu = accountCount ? MENU_ACCOUNT : MENU_REGISTER;
    }
    else if (menu == MENU_ACCOUNT)
    {
        drawText(0, 0, width, height / 5, "Select or add an account", font, height / 10, true, WHITE);
        
        if (drawButton(width / 4, height * 8 / 10, width / 2, height / 10, 
            "Add new account", font, height / 12, NORMAL, LIGHT, BRIGHT, WHITE))
                menu = MENU_REGISTER;
        
        const int maxRowLength = 4;
        int size = width / 6;
        int rowCount = (accountCount - 1) / maxRowLength + 1;
        int lastRowSize = (accountCount - (rowCount - 1) * rowCount) * size;
        int startY = (height - rowCount * size) / 2;
        
        for (int i = 0; i < accountCount; i++)
        {
            int row = i / maxRowLength;
            int col = i % maxRowLength;
            if (drawButton((width - (row == rowCount - 1 ? lastRowSize : maxRowLength * size)) / 2 + col * size,
                startY + row * size, size, size, accounts[i].name, font, size / 5, BLANK, LIGHT, DARK, BRIGHT))
            {
                login(accounts[i].name, accounts[i].password);
                accountOpen(accountId = accounts[i].user);
                menu = MENU_WELCOME;
            }
        }
    }
    else if (menu == MENU_REGISTER)
    {
        int w = width * 2 / 3;
        int h = height / 10;
        int b = h / 2;
        int f = h * 3 / 4;
        int x = (width - w) / 2;
        int y = (height - h * 4 - b * 3) / 2;
        int w2 = (w - b) / 2;
        int f2 = h / 2;
        
        drawText(x, y, w, h, "Nest", font, h * 2, true, WHITE);
        
        drawTextbox(&username, &usernameData, x, y + h + b, w, h,
            "Username", font, f, DARK, WHITE, GRAY, WHITE, MEDIUM);
        drawTextbox(&password, &passwordData, x, y + h * 2 + b * 2, w, h,
            "Password", font, f, DARK, WHITE, GRAY, WHITE, MEDIUM);
            
        if ((int)usernameData != -1 || (int)passwordData != -1)
            statusArg = 0;
        
        if (drawButton(x, y + h * 3 + b * 3, w2, h, "Signup", font, f, NORMAL, LIGHT, BRIGHT, WHITE))
            if (!*username)
                statusArg = 1;
            else if (!*password)
                statusArg = 2;
            else if (accountId = signup(username, password))
            {
                accountSave(accountId, username, password);
                accountOpen(accountId);
                menu = MENU_WELCOME;
            }
            else
                statusArg = 3;
        else if (drawButton(x + w2 + b, y + h * 3 + b * 3, w2, h, "Login", font, f, NORMAL, LIGHT, BRIGHT, WHITE))
            if (!*username)
                statusArg = 1;
            else if (!*password)
                statusArg = 2;
            else if (accountId = login(username, password))
            {
                accountSave(accountId, username, password);
                accountOpen(accountId);
                menu = MENU_WELCOME;
            }
            else
                statusArg = 4;
        
        if (statusArg == 1)
            drawText(x, y + h * 4 + b * 3, w, h, "Enter username.", font, f2, true, ERROR);
        else if (statusArg == 2)
            drawText(x, y + h * 4 + b * 3, w, h, "Enter password.", font, f2, true, ERROR);
        else if (statusArg == 3)
            drawText(x, y + h * 4 + b * 3, w, h, "Username is already used.", font, f2, true, ERROR);
        else if (statusArg == 4)
            drawText(x, y + h * 4 + b * 3, w, h, "Incorrect username or password.", font, f2, true, ERROR);
    }
    else
    {
        int border = width / 4;
        int borderSize = 2;
        int chatH = height / 8;
        int inputH = height / 20;
        
        DrawRectangle(border, 0, borderSize, height, DARK);
        if (menu == MENU_CHAT_MESSAGE)
            DrawRectangle(0, (currentChat + 1) * chatH, border, chatH, DARK);
        if (mouseX < border && mouseY > chatH && mouseY < (chatCount + 1) * chatH)
        {
            int chat = mouseY / chatH - 1;
            DrawRectangle(0, (chat + 1) * chatH, border, chatH, DARK);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                currentChat = chat;
                messageRequest(chats[currentChat].id, 0);
                menu = MENU_CHAT_MESSAGE;
                statusArg = 0;
            }
        }
        for (size_t i = 0; i < chatCount; i++)
        {
            int padding = chatH / 8;
            int textH = chatH / 2 - padding;
            int chatY = (i + 1) * chatH;
            DrawRectangle(padding, chatY + padding, chatH - padding * 2, chatH - padding * 2, GRAY); // Avatar
            
            BeginScissorMode(0, chatY, border, chatH);
            drawText(chatH, chatY + padding, border - padding, textH,
                chats[i].name, font, textH, false, WHITE);
            if (chats[i].lastMessageContent)
                drawText(chatH - textH / 8, chatY + textH + padding, border - padding, textH,
                    chats[i].lastMessageContent, font, textH * 3 / 4, false, LIGHT);
            EndScissorMode();
            
            DrawRectangle(0, chatY + chatH, border, borderSize, DARK);
        }
        DrawRectangle(0, chatH, width, borderSize, DARK);
        
        if (menu != MENU_CHAT_CALL)
        {
            if (drawButton(0, 0, border / 2, chatH, "Create", font, chatH / 2, BLANK, LIGHT, BLANK, BRIGHT))
            {
                menu = MENU_CHAT_CREATE;
                statusArg = 0;
            }
            if (drawButton(border / 2, 0, border / 2, chatH, "Join", font, chatH / 2, BLANK, LIGHT, BLANK, BRIGHT))
            {
                menu = MENU_CHAT_JOIN;
                statusArg = 0;
            }
        }
        
        Chat *chat = &chats[currentChat];
        if (currentChat)
            drawText(border, 0, 0, chatH, chat->name, font, chatH / 2, false, LIGHT);
        
        switch (menu)
        {
        case MENU_WELCOME:
            drawText(border, chatH, width - border, height - chatH,
                "Welcome to Nest!\nSelect, create or join to a chat.", font, width / 16, true, GRAY);
            break;
        case MENU_CHAT_MESSAGE:
            if (!callChat || callChat == currentChat)
                if (drawButton(width - width / 6, 0, width / 6, chatH, "Call", 
                        font, chatH / 2, BLANK, LIGHT, BLANK, BRIGHT))
                {
                    if (!callChat)
                        callJoin(chat->id);
                    menu = MENU_CHAT_CALL;
                    callChat = currentChat;
                }
        
            if ((drawTextbox(&msgText, &msgData, border + inputH, height - inputH * 2, width - border - inputH * 4,
                    inputH, "Enter message...", font, inputH, DARK, WHITE, GRAY, WHITE, MEDIUM) ||
                drawButton(width - inputH * 2, height - inputH * 2, inputH, inputH, ">", font, inputH,
                    NORMAL, LIGHT, BRIGHT, WHITE)) && strlen(msgText))
            {
                messageSend(chat->id, MESSAGE_TEXT, msgText, strlen(msgText));
                msgText = "";
            }
                
            int posY = messageScrollPos;
            
            BeginScissorMode(0, chatH + borderSize, width, height - chatH - borderSize - inputH * 2);
            for (size_t i = 0, prevSenderId = 0; i < messageCount; i++)
            {
                Message *message = &messages[i];
                Vector2i contentSize = { 0, 0 };
                int nameSize = width / 30;
                
                if ((prevSenderId && prevSenderId != message->senderId || i == messageCount - 1) &&
                    prevSenderId != accountId)
                {
                    bool userLoaded = false;
                    for (size_t j = 0; j < userCount; j++)
                        if (users[j].id == prevSenderId)
                        {
                            if (users[j].name)
                                drawText(border + 10, height - inputH * 3 - 10 - posY,
                                    width - border - 20, nameSize, users[j].name, font, nameSize, 0, WHITE);
                            userLoaded = true;
                            break;
                        }
                    if (!userLoaded)
                    {
                        addUser((User){ prevSenderId, 0, 0, 0 });
                        userRequest(prevSenderId);
                    }
                    posY += nameSize;
                }
                
                int contentTextSize = width / 40;
                switch (message->type)
                {
                case MESSAGE_TEXT:
                    contentSize = measureTextBoxed(
                        (width - border) * 3 / 4, message->content, message->contentLen, font, contentTextSize);
                    drawTextBoxed(message->senderId == accountId ? width - contentSize.x - 10 : border + 10,
                        height - inputH * 3 - 10 - posY - contentSize.y + contentTextSize,
                        contentSize.x, contentSize.y, message->content, message->contentLen,
                        font, contentTextSize, message->senderId == accountId, LIGHT);
                    posY += contentSize.y + contentTextSize / 2;
                    break;
                }
                
                prevSenderId = message->senderId;
            }
            EndScissorMode();
            
            messageScrollVel -= (int)(GetMouseWheelMove() * height);
            messageScrollPos += messageScrollVel / 100;
            if (posY < height - chatH - inputH * 3)
            {
                if (messageScrollVel < 0)
                    messageScrollVel = 0;
                messageScrollPos += (height - chatH - inputH * 3 - posY) / 10;
            }
            if (messageScrollPos > 0)
                messageScrollPos = 0;
            messageScrollVel = messageScrollVel * 9 / 10;
            break;
        case MENU_CHAT_CREATE:
            drawText(border, 0, 0, chatH, "Creating chat...", font, chatH / 2, false, LIGHT);
            if ((drawTextbox(&msgText, &msgData, border + inputH, height / 2, width - border - inputH * 4, 
                    inputH, "Enter name...", font, inputH, DARK, WHITE, GRAY, WHITE, MEDIUM) ||
                drawButton(width - inputH * 2, height / 2, inputH, inputH, ">", font, inputH,
                    NORMAL, LIGHT, BRIGHT, WHITE)) && strlen(msgText))
                chatCreate(msgText);
            if (statusArg)
            {
                if (statusArg > 0)
                {
                    msgText = "";
                    for (size_t i = 0; i < chatCount; i++)
                        if (chats[i].id == statusArg)
                        {
                            currentChat = i;
                            break;
                        }
                    messageRequest(statusArg, 0);
                    menu = MENU_CHAT_MESSAGE;
                    statusArg = 0;
                }
                else
                    drawText(border + inputH, height / 2 + inputH, width - border - inputH * 4, 
                        inputH, statusMsg, font, inputH, 1, ERROR);
            }
            break;
        case MENU_CHAT_JOIN:
            drawText(border, 0, 0, chatH, "Joining...", font, chatH / 2, false, LIGHT);
            if ((drawTextbox(&msgText, &msgData, border + inputH, height / 2, width - border - inputH * 4,
                    inputH, "Enter name...", font, inputH, DARK, WHITE, GRAY, WHITE, MEDIUM) ||
                drawButton(width - inputH * 2, height / 2, inputH, inputH, ">", font, inputH,
                    NORMAL, LIGHT, BRIGHT, WHITE)) && strlen(msgText))
                chatJoin(msgText);
            if (statusArg)
            {
                if (statusArg > 0)
                {
                    for (size_t i = 0; i < chatCount; i++)
                        if (chats[i].id == statusArg)
                        {
                            currentChat = i;
                            break;
                        }
                    messageRequest(statusArg, 0);
                    menu = MENU_CHAT_MESSAGE;
                    statusArg = 0;
                }
                else
                    drawText(border + inputH, height / 2 + inputH, width - border - inputH * 4, 
                        inputH, statusMsg, font, inputH, 1, ERROR);
            }
            break;
        case MENU_CHAT_CALL:
            if (drawButton(width - width / 6, 0, width / 6, chatH, "Leave", 
                    font, chatH / 2, BLANK, LIGHT, BLANK, BRIGHT))
            {
                callLeave();
                menu = MENU_CHAT_MESSAGE;
                callChat = 0;
            }
            if (drawButton(0, 0, border, chatH, "Back", font, chatH / 2, BLANK, LIGHT, BLANK, BRIGHT))
                menu = MENU_CHAT_MESSAGE;
            {
                int h = height / 20;
                int posY = (height - chatH - callMemberCount * height / 10) / 2 + chatH;
                int margin = width / 10;
                for (size_t i = 0; i < callMemberCount; i++)
                    if (callMembers[i].userId != accountId)
                    {
                        bool userLoaded = false;
                        for (size_t j = 0; j < userCount; j++)
                            if (users[j].id == callMembers[i].userId)
                            {
                                if (users[j].name)
                                    drawText(border + margin, posY, (width - border) / 2 - margin, h,
                                        users[j].name, font, h, 0, WHITE);
                                userLoaded = true;
                                break;
                            }
                        if (!userLoaded)
                        {
                            addUser((User){ callMembers[i].userId, 0, 0, 0 });
                            userRequest(callMembers[i].userId);
                            userLoaded = true;
                        }
                        drawSlider(&callMembers[i].pressed, &callMembers[i].volume, 120, (width + border) / 2, posY,
                            (width - border) / 2 - margin, h, h / 3, LIGHT, WHITE, NORMAL, BRIGHT);
                        posY += height / 10;
                    }
            }
            break;
        }
    }
    
#ifdef __ANDROID__
    if (!showKeyboard && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        hideKb();
#endif

    EndDrawing();
    return !WindowShouldClose();
}

void UI_close()
{
    UnloadFont(font);
    free(msgText);
    CloseWindow();
}

void setHandlers(void (*onStartup)(),
    int (*onSignup)(const char *, const char *), int (*onLogin)(const char *, const char *),
    void (*onAccountOpen)(int), void (*onAccountSave)(int, const char *, const char *),
    void (*onChatCreate)(const char *), void (*onChatJoin)(const char *),
    void (*onMessageSend)(int, int, const char *, size_t), void (*onUserRequest)(int),
    void (*onMessageRequest)(int, int), void (*onCallJoin)(int), void (*onCallLeave)())
{
    startup = onStartup;
    signup = onSignup;
    login = onLogin;
    accountOpen = onAccountOpen;
    accountSave = onAccountSave;
    chatCreate = onChatCreate;
    chatJoin = onChatJoin;
    messageSend = onMessageSend;
    userRequest = onUserRequest;
    messageRequest = onMessageRequest;
    callJoin = onCallJoin;
    callLeave = onCallLeave;
}

void addChat(Chat chat)
{
    chats = addElement(chats, chatCount++, &chat, chatCount, sizeof(Chat));
}

void addUser(User user)
{
    users = addElement(users, userCount++, &user, userCount, sizeof(User));
}

void updateUser(User user)
{
    for (size_t i = 0; i < userCount; i++)
        if (users[i].id == user.id)
        {
            users[i] = user;
            return;
        }
    users = addElement(users, userCount++, &user, userCount, sizeof(User));
}

void addMembership(Membership membershipship)
{
    for (size_t i = 0; i < chatCount; i++)
        if (chats[i].id == membershipship.chatId)
        {
            chats[i].memberships = addElement(chats[i].memberships, chats[i].memberCount++,
                &membershipship, chats[i].memberCount, sizeof(Membership));
            return;
        }
}

void addMessage(Message message)
{
    for (size_t i = 0; i < chatCount; i++)
        if (chats[i].id == message.chatId)
        {
            if (message.type == MESSAGE_TEXT && message.id > chats[i].lastMessageId)
            {
                if (chats[i].lastMessageContent)
                    free(chats[i].lastMessageContent);
                chats[i].lastMessageId = message.id;
                chats[i].lastMessageContent = strndup(message.content, message.contentLen);
            } 
            break;
        }
    if (message.chatId != chats[currentChat].id)
        return;
    for (size_t i = 0; i < messageCount; i++)
        if (messages[i].id < message.id)
        {
            messages = addElement(messages, messageCount++, &message, i, sizeof(Message));
            return;
        }
    messages = addElement(messages, messageCount++, &message, messageCount, sizeof(Message));
}

void clearMessages()
{
    messageCount = 0;
    if (messages)
    {
        for (size_t i = 0; i < messageCount; i++)
        {
            free(messages[i].sent);
            free(messages[i].content);
        }
        free(messages);
        messages = 0;
    }
}

void addCallMember(int userId)
{
    CallMember newMember = { userId, 100, 0, 0, 0, malloc(AUDIO_BUFFER_SIZE * 2) };
    callMembers = addElement(callMembers, callMemberCount++, &newMember, callMemberCount, sizeof(CallMember));
}

void removeCallMember(int userId)
{
    for (size_t i = 0; i < callMemberCount; i++)
        if (callMembers[i].userId == userId)
        {
            free(callMembers[i].buffer);
            removeElement(callMembers, callMemberCount--, i, sizeof(CallMember));
            return;
        }
}

void clearCall()
{
    for (size_t i = 0; i < callMemberCount; i++)
        free(callMembers[i].buffer);
    free(callMembers);
    callMembers = 0;
    callMemberCount = 0;
}

CallMember *getCallMember(int userId)
{
    for (size_t i = 0; i < callMemberCount; i++)
        if (callMembers[i].userId == userId)
            return &callMembers[i];
    return 0;
}

CallMember *getCallMembers()
{
    return callMembers;
}

size_t getCallMemberCount()
{
    return callMemberCount;
}

void addAccount(Account account)
{
    accounts = addElement(accounts, accountCount++, &account, accountCount, sizeof(Account));
}

void setStatus(const char *msg, int arg)
{
    statusMsg = msg;
    statusArg = arg;
}

int getFrameTime()
{
    return 1000 / GetFPS();
}

int authenticate()
{
    for (size_t i = 0; i < accountCount; i++)
        if (accounts[i].user == accountId)
        {
            if (!login(accounts[i].name, accounts[i].password))
                return 0;
            if (menu == MENU_CHAT_MESSAGE)
                messageRequest(chats[currentChat].id, chats[currentChat].lastMessageId);
            return 1;
        }
    return 0;
}
