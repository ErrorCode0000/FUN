#include <windows.h>
#include <GL/gl.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>

// --- 1. MANUEL OPENGL TANIMLAMALARI (GLEW OLMADAN) ---
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_QK_COMPUTE_SHADER              0x91B9
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#define GL_SHADER_STORAGE_BARRIER_BIT     0x2000
#define GL_COMPILE_STATUS                 0x8B81
#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

// EKSİK OLAN KISIM BURADAYDI, EKLENDİ:
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;   // <-- BU SATIR EKLENDİ
typedef char GLchar;

// Fonksiyon Pointer Tipleri
typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC) (void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRY *PFNGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRY *PFNGLMEMORYBARRIERPROC) (GLbitfield barriers);
typedef void (APIENTRY *PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC) (int interval);

// Fonksiyon Objeleri
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
PFNGLMEMORYBARRIERPROC glMemoryBarrier;
PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

// --- 2. AYARLAR & SHADERLAR ---
const int TEXTURE_SIZE = 2048; 
const int NUM_PARTICLES = TEXTURE_SIZE * TEXTURE_SIZE;
int screenWidth = 1280;
int screenHeight = 720;

// COMPUTE SHADER (Fizik Motoru - GPU)
const char* cs_src = R"(
#version 430
layout(local_size_x = 256) in;

struct Particle { vec2 pos; vec2 vel; };
layout(std430, binding = 0) buffer P { Particle p[]; };

uniform float dt;
uniform vec2 mouse;
uniform int mode; // 0:Yok, 1:Cek, 2:It
uniform float aspect;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if(i >= 4194304) return;

    vec2 pos = p[i].pos;
    vec2 vel = p[i].vel;

    // Home calculation
    float tx = float(i % 2048) / 2048.0;
    float ty = float(i / 2048) / 2048.0;
    vec2 home = vec2(tx * 2.0 - 1.0, ty * 2.0 - 1.0);
    home.x *= aspect; // Aspect fix for home pos

    // Physics
    vel += (home - pos) * 0.008; // Spring
    vel *= 0.96; // Friction

    // Mouse Interaction
    if(mode > 0) {
        vec2 dir = mouse - pos;
        float dist2 = dot(dir, dir);
        if(dist2 < 0.2) {
            float f = (0.2 - sqrt(dist2)) * 1.5;
            if(mode == 2) f = -f * 2.5; // Kara delik modu
            vel += normalize(dir) * f * 0.02;
        }
    }

    pos += vel;
    p[i].pos = pos;
    p[i].vel = vel;
}
)";

// VERTEX SHADER (Cizim)
const char* vs_src = R"(
#version 430
struct Particle { vec2 pos; vec2 vel; };
layout(std430, binding = 0) buffer P { Particle p[]; };
uniform float aspect;
out vec3 color;
void main() {
    vec2 pos = p[gl_VertexID].pos;
    vec2 vel = p[gl_VertexID].vel;
    // Aspect Ratio düzeltmesi (Stretch fix)
    gl_Position = vec4(pos.x / aspect, pos.y, 0.0, 1.0);
    
    float s = length(vel) * 50.0;
    color = vec3(0.1 + s, 0.4 + s*0.5, 1.0);
}
)";

// FRAGMENT SHADER
const char* fs_src = R"(
#version 430
in vec3 color;
out vec4 f;
void main() { f = vec4(color, 0.7); }
)";

// --- 3. YARDIMCI SISTEMLER ---
void LoadOpenGLFunctions() {
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)wglGetProcAddress("glMemoryBarrier");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)wglGetProcAddress("glBindBufferBase");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
}

GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
        printf("Shader Error: %s\n", log);
    }
    return s;
}

// Global durum
bool running = true;
int mouseState = 0; // 0:None, 1:Left, 2:Right
float mx = 0, my = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CLOSE: running = false; break;
        case WM_SIZE:
            screenWidth = LOWORD(lParam);
            screenHeight = HIWORD(lParam);
            glViewport(0, 0, screenWidth, screenHeight);
            break;
        case WM_KEYDOWN: if(wParam == VK_ESCAPE) running = false; break;
        case WM_LBUTTONDOWN: mouseState = 1; SetCapture(hwnd); break;
        case WM_RBUTTONDOWN: mouseState = 2; SetCapture(hwnd); break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP: mouseState = 0; ReleaseCapture(); break;
        case WM_MOUSEMOVE: {
            // Mouse koordinatlarını -1..1 aralığına çek ve aspect ratio düzelt
            float aspect = (float)screenWidth / screenHeight;
            mx = ((float)LOWORD(lParam) / screenWidth * 2.0f - 1.0f) * aspect;
            my = -((float)HIWORD(lParam) / screenHeight * 2.0f - 1.0f);
            } break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- MAIN ---
int main() {
    // 1. PENCERE OLUSTURMA (Win32 API)
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "BarisEngine";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, "BarisEngine", "Baris GPU Particles - RTX Optimized", 
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, 0, 0, wc.hInstance, 0);

    // 2. OPENGL BASLATMA (Dummy Context Trick)
    // Modern OpenGL açmak için önce eski bir context açıp pointerları almalıyız.
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1, PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32};
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC tempRC = wglCreateContext(hdc);
    wglMakeCurrent(hdc, tempRC);

    LoadOpenGLFunctions(); // Pointerları çek

    // Şimdi Modern (4.3 Core) Context aç
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    HGLRC hrc = wglCreateContextAttribsARB(hdc, 0, attribs);
    wglMakeCurrent(hdc, hrc);
    wglDeleteContext(tempRC); // Eskiyi sil
    if(wglSwapIntervalEXT) wglSwapIntervalEXT(0); // VSYNC OFF

    // 3. PROGRAMLARI DERLE
    GLuint cp = glCreateProgram();
    GLuint cs = CompileShader(GL_COMPUTE_SHADER, cs_src);
    glAttachShader(cp, cs); glLinkProgram(cp);

    GLuint rp = glCreateProgram();
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);
    glAttachShader(rp, vs); glAttachShader(rp, fs); glLinkProgram(rp);

    // 4. VERI HAZIRLIGI (SSBO)
    // 4 Milyonluk buffer'ı GPU'ya ayır
    struct P { float px,py,vx,vy; };
    std::vector<P> parts(NUM_PARTICLES);
    float aspect = (float)screenWidth / screenHeight;
    for(int i=0; i<NUM_PARTICLES; ++i) {
        float tx = float(i % TEXTURE_SIZE) / TEXTURE_SIZE;
        float ty = float(i / TEXTURE_SIZE) / TEXTURE_SIZE;
        parts[i].px = (tx * 2.0f - 1.0f) * aspect;
        parts[i].py = ty * 2.0f - 1.0f;
    }

    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(P)*NUM_PARTICLES, parts.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    GLuint vao; glGenVertexArrays(1, &vao);

    printf("Sistem Hazir. 4.2 Milyon Parcacik.\nSol Tik: Cekim | Sag Tik: Kara Delik\n");

    // 5. ANA DONGU
    MSG msg;
    while(running) {
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            float currentAspect = (float)screenWidth / screenHeight;

            // FIZIK (GPU)
            glUseProgram(cp);
            glUniform1f(glGetUniformLocation(cp, "dt"), 0.016f);
            glUniform2f(glGetUniformLocation(cp, "mouse"), mx, my); // mx zaten aspect düzeltmeli
            glUniform1i(glGetUniformLocation(cp, "mode"), mouseState);
            glUniform1f(glGetUniformLocation(cp, "aspect"), currentAspect);
            glDispatchCompute((NUM_PARTICLES + 255)/256, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // CIZIM
            glViewport(0, 0, screenWidth, screenHeight);
            glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(rp);
            glUniform1f(glGetUniformLocation(rp, "aspect"), currentAspect);
            glBindVertexArray(vao);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

            SwapBuffers(hdc);
        }
    }
    return 0;
}
