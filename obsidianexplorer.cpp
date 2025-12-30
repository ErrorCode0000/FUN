/*
    OBSIDIAN EXPLORER - ULTIMATE SUITE v3.2 (FINAL FIX)
    ---------------------------------------------------
    Fix: Navigation Logic (Restored '.' and '..')
    Fix: GDI+ DrawImage Ambiguity
    Features: Text Edit, Hex Edit, Delete, View, Zip
*/

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <strsafe.h>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace Gdiplus;

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- ID DEFINITIONS ---
#define ID_LISTVIEW    1001
#define ID_PATHBOX     1002
#define ID_PREVIEW     1003
#define IDM_OPEN       2001
#define IDM_COMPRESS   2002
#define IDM_HEX_EDIT   2003
#define IDM_TEXT_EDIT  2004
#define IDM_DELETE     2005
#define ID_TEXT_SAVE   3001
#define ID_EDIT_CTRL   3002

// --- THEME COLORS ---
const COLORREF COL_BG = RGB(30, 30, 30);
const COLORREF COL_FG = RGB(220, 220, 220);
const COLORREF COL_HEX_BG = RGB(20, 20, 20);
const COLORREF COL_HEX_FG = RGB(0, 255, 0); 

// --- GLOBALS ---
HINSTANCE hInst;
HWND hMainWnd;
HWND hListView, hPathBox, hPreviewWnd;
std::wstring currentPath;
bool isInZipMode = false;
ULONG_PTR gdiplusToken;

// --- HEX EDITOR GLOBALS ---
HWND hHexWnd = NULL;
std::wstring hexFilePath;
long long hexCurrentOffset = 0;
long long hexCursorIdx = 0;
long long hexFileSize = 0;
const int HEX_BYTES_PER_ROW = 16;
const int HEX_ROWS = 25;
bool isHexEditing = false;
std::wstring hexEditBuffer = L"";

// --- TEXT EDITOR GLOBALS ---
HWND hTextWnd = NULL;
HWND hEditControl = NULL;
std::wstring textFilePath;

// --- PROTOTYPES ---
void EnableDarkTitleBar(HWND hwnd);
void EnableGodModePrivileges();
void Navigate(std::wstring path);
LRESULT CALLBACK HexWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------
// HELPER: ADD SPECIAL DIR (., ..)
// ---------------------------------------------------------
void AddSpecialDir(std::wstring name) {
    LVITEM lvi = {0}; 
    lvi.mask = LVIF_TEXT | LVIF_PARAM; 
    lvi.iItem = ListView_GetItemCount(hListView);
    lvi.iSubItem = 0;
    lvi.pszText = (LPWSTR)name.c_str(); 
    lvi.lParam = 1; // 1 = DIRECTORY flag
    ListView_InsertItem(hListView, &lvi);
    ListView_SetItemText(hListView, lvi.iItem, 1, (LPWSTR)L"<DIR>");
}

// ---------------------------------------------------------
// TEXT EDITOR LOGIC
// ---------------------------------------------------------
void OpenTextEditor(std::wstring path) {
    textFilePath = path;
    WNDCLASSEX wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = TextWndProc; wc.hInstance = hInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); wc.lpszClassName = L"ObsidianTextEdit"; RegisterClassEx(&wc);

    if (!hTextWnd || !IsWindow(hTextWnd)) {
        hTextWnd = CreateWindowEx(0, L"ObsidianTextEdit", (L"Text Editor - " + path).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, hMainWnd, NULL, hInst, NULL);
    } else {
        SetWindowText(hTextWnd, (L"Text Editor - " + path).c_str()); ShowWindow(hTextWnd, SW_RESTORE); SetForegroundWindow(hTextWnd);
    }

    HMENU hMenu = CreateMenu(); HMENU hFileMenu = CreateMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_TEXT_SAVE, L"&Save"); AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File"); SetMenu(hTextWnd, hMenu);

    std::string fileContent; HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size != INVALID_FILE_SIZE) {
            std::vector<char> buf(size); DWORD read; ReadFile(hFile, buf.data(), size, &read, NULL);
            fileContent.assign(buf.begin(), buf.begin() + read);
        } CloseHandle(hFile);
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, fileContent.c_str(), -1, NULL, 0);
    if (wlen == 0 && GetLastError() == ERROR_NO_UNICODE_TRANSLATION) wlen = MultiByteToWideChar(CP_ACP, 0, fileContent.c_str(), -1, NULL, 0);
    std::wstring wContent(wlen, 0); MultiByteToWideChar(CP_UTF8, 0, fileContent.c_str(), -1, &wContent[0], wlen);

    SetWindowText(hEditControl, wContent.c_str()); EnableDarkTitleBar(hTextWnd); ShowWindow(hTextWnd, SW_SHOW);
}

void SaveTextFile() {
    int len = GetWindowTextLength(hEditControl);
    if (len > 0) {
        std::vector<wchar_t> wBuf(len + 1); GetWindowText(hEditControl, wBuf.data(), len + 1);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, NULL, 0, NULL, NULL);
        std::vector<char> utf8Buf(utf8Len); WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, utf8Buf.data(), utf8Len, NULL, NULL);
        HANDLE hFile = CreateFile(textFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(hFile, utf8Buf.data(), utf8Len - 1, &written, NULL); CloseHandle(hFile); MessageBox(hTextWnd, L"Saved!", L"Obsidian", MB_OK); }
        else MessageBox(hTextWnd, L"Error saving file.", L"Error", MB_ICONERROR);
    }
}

LRESULT CALLBACK TextWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        hEditControl = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_CTRL, hInst, NULL);
        SendMessage(hEditControl, WM_SETFONT, (WPARAM)hFont, TRUE); SendMessage(hEditControl, EM_LIMITTEXT, 0, 0);
    } break;
    case WM_SIZE: { RECT rc; GetClientRect(hWnd, &rc); MoveWindow(hEditControl, 0, 0, rc.right, rc.bottom, TRUE); } break;
    case WM_COMMAND: if (LOWORD(wParam) == ID_TEXT_SAVE) SaveTextFile(); break;
    case WM_DESTROY: hTextWnd = NULL; break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    } return 0;
}

// ---------------------------------------------------------
// HEX EDITOR LOGIC
// ---------------------------------------------------------
void OpenHexEditor(std::wstring path) {
    hexFilePath = path; hexCurrentOffset = 0; hexCursorIdx = 0; isHexEditing = false; hexEditBuffer = L"";
    HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size; GetFileSizeEx(hFile, &size); hexFileSize = size.QuadPart; CloseHandle(hFile);
        if (hHexWnd && IsWindow(hHexWnd)) { ShowWindow(hHexWnd, SW_RESTORE); SetForegroundWindow(hHexWnd); SetWindowText(hHexWnd, (L"Obsidian Hex Editor - " + path).c_str()); InvalidateRect(hHexWnd, NULL, TRUE); }
        else {
            WNDCLASSEX wcHex = {0}; wcHex.cbSize = sizeof(WNDCLASSEX); wcHex.style = CS_HREDRAW | CS_VREDRAW;
            wcHex.lpfnWndProc = HexWndProc; wcHex.hInstance = hInst; wcHex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcHex.hbrBackground = CreateSolidBrush(COL_HEX_BG); wcHex.lpszClassName = L"ObsidianHexClass"; RegisterClassEx(&wcHex);
            hHexWnd = CreateWindowEx(0, L"ObsidianHexClass", (L"Obsidian Hex Editor - " + path).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 680, hMainWnd, NULL, hInst, NULL);
            EnableDarkTitleBar(hHexWnd); ShowWindow(hHexWnd, SW_SHOW);
        }
    }
}

void WriteByteToFile(long long offset, BYTE newVal) {
    HANDLE hFile = CreateFile(hexFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) { LARGE_INTEGER li; li.QuadPart = offset; if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) { DWORD written; WriteFile(hFile, &newVal, 1, &written, NULL); } CloseHandle(hFile); }
}

LRESULT CALLBACK HexWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(COL_HEX_BG); FillRect(hdc, &rc, bgBrush); DeleteObject(bgBrush);
        HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        SelectObject(hdc, hFont); SetBkMode(hdc, TRANSPARENT);
        
        int bytesToRead = HEX_ROWS * HEX_BYTES_PER_ROW; std::vector<BYTE> buffer(bytesToRead); DWORD bytesRead = 0;
        HANDLE hFile = CreateFile(hexFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) { LARGE_INTEGER li; li.QuadPart = hexCurrentOffset; SetFilePointerEx(hFile, li, NULL, FILE_BEGIN); ReadFile(hFile, buffer.data(), bytesToRead, &bytesRead, NULL); CloseHandle(hFile); }

        wchar_t textBuf[256]; int y = 10;
        SetTextColor(hdc, COL_FG); TextOut(hdc, 10, y, L"OFFSET   | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ASCII", 66); y += 40;

        for (int i = 0; i < (int)bytesRead; i += HEX_BYTES_PER_ROW) {
            SetTextColor(hdc, COL_HEX_FG); StringCchPrintf(textBuf, 256, L"%08llX | ", hexCurrentOffset + i); TextOut(hdc, 10, y, textBuf, lstrlenW(textBuf));
            int xHex = 100; int xAscii = 500;
            for (int j = 0; j < HEX_BYTES_PER_ROW; j++) {
                if (i + j < (int)bytesRead) {
                    BYTE b = buffer[i + j]; int currentIdx = i + j;
                    if (currentIdx == hexCursorIdx) { RECT rCursor = {xHex, y, xHex + 20, y + 18}; HBRUSH curBrush = CreateSolidBrush(isHexEditing ? RGB(200, 0, 0) : RGB(50, 50, 50)); FillRect(hdc, &rCursor, curBrush); DeleteObject(curBrush); SetTextColor(hdc, RGB(255, 255, 255)); } else SetTextColor(hdc, COL_HEX_FG);
                    if (currentIdx == hexCursorIdx && isHexEditing && hexEditBuffer.length() > 0) { std::wstring display = hexEditBuffer; if (display.length() < 2) display += L"_"; TextOut(hdc, xHex, y, display.c_str(), 2); } 
                    else { StringCchPrintf(textBuf, 256, L"%02X", b); TextOut(hdc, xHex, y, textBuf, 2); }
                    SetTextColor(hdc, COL_HEX_FG); wchar_t c = (b >= 32 && b <= 126) ? (wchar_t)b : L'.'; textBuf[0] = c; textBuf[1] = L'\0'; TextOut(hdc, xAscii + (j * 10), y, textBuf, 1);
                } xHex += 24;
            } y += 20;
        }
        y += 10; SetTextColor(hdc, RGB(255, 100, 100)); std::wstring statusMsg = isHexEditing ? L"EDITING (Type Hex)" : L"[F2] to Edit"; TextOut(hdc, 10, y, statusMsg.c_str(), statusMsg.length());
        DeleteObject(hFont); EndPaint(hWnd, &ps);
    } break;
    case WM_KEYDOWN:
        if (isHexEditing) { if (wParam == VK_ESCAPE) { isHexEditing = false; hexEditBuffer = L""; InvalidateRect(hWnd, NULL, TRUE); } }
        else {
            if (wParam == VK_RIGHT) { hexCursorIdx++; InvalidateRect(hWnd, NULL, TRUE); }
            if (wParam == VK_LEFT && hexCursorIdx > 0) { hexCursorIdx--; InvalidateRect(hWnd, NULL, TRUE); }
            if (wParam == VK_DOWN) { if (hexCursorIdx + 16 < HEX_ROWS * 16) hexCursorIdx += 16; else if (hexCurrentOffset + 16 < hexFileSize) hexCurrentOffset += 16; InvalidateRect(hWnd, NULL, TRUE); }
            if (wParam == VK_UP) { if (hexCursorIdx - 16 >= 0) hexCursorIdx -= 16; else if (hexCurrentOffset - 16 >= 0) hexCurrentOffset -= 16; InvalidateRect(hWnd, NULL, TRUE); }
            if (wParam == VK_F2) { isHexEditing = true; hexEditBuffer = L""; InvalidateRect(hWnd, NULL, TRUE); }
        } break;
    case WM_CHAR:
        if (isHexEditing) {
            wchar_t ch = (wchar_t)wParam; if (ch >= 'a' && ch <= 'f') ch = ch - 'a' + 'A';
            if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')) {
                hexEditBuffer += ch;
                if (hexEditBuffer.length() == 2) { int val; std::wstringstream ss; ss << std::hex << hexEditBuffer; ss >> val; WriteByteToFile(hexCurrentOffset + hexCursorIdx, (BYTE)val); isHexEditing = false; hexEditBuffer = L""; } InvalidateRect(hWnd, NULL, TRUE);
            }
        } break;
    case WM_MOUSEWHEEL: { short delta = GET_WHEEL_DELTA_WPARAM(wParam); if (delta < 0) hexCurrentOffset += 16; else if (hexCurrentOffset - 16 >= 0) hexCurrentOffset -= 16; InvalidateRect(hWnd, NULL, TRUE); } break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    } return 0;
}

// ---------------------------------------------------------
// OBSIDIAN EXPLORER CORE
// ---------------------------------------------------------
void EnableDarkTitleBar(HWND hwnd) { BOOL value = TRUE; DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value)); }
void EnableGodModePrivileges() { 
    HANDLE hToken; LUID luid; TOKEN_PRIVILEGES tkp;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid); tkp.PrivilegeCount = 1; tkp.Privileges[0].Luid = luid; tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), NULL, NULL); CloseHandle(hToken);
    }
}

void DeleteSelectedFile(std::wstring path) {
    if (MessageBox(hMainWnd, (L"Are you sure you want to delete:\n" + path).c_str(), L"Confirm Delete", MB_YESNO | MB_ICONWARNING) == IDYES) {
        if (DeleteFile(path.c_str())) Navigate(currentPath);
        else MessageBox(hMainWnd, L"Failed to delete file. Check permissions.", L"Error", MB_ICONERROR);
    }
}

void CompressToZip(std::wstring targetPath) {
    std::wstring fileName = targetPath.substr(targetPath.find_last_of(L"\\") + 1);
    SHELLEXECUTEINFO sei = {0}; sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_NOCLOSEPROCESS; sei.lpVerb = L"open"; sei.lpFile = L"tar.exe";
    std::wstring params = L"-a -c -f \"" + fileName + L".zip\" \"" + fileName + L"\"";
    sei.lpParameters = params.c_str(); sei.lpDirectory = currentPath.c_str(); sei.nShow = SW_HIDE;
    if (ShellExecuteEx(&sei)) { WaitForSingleObject(sei.hProcess, INFINITE); CloseHandle(sei.hProcess); MessageBox(hMainWnd, L"Done.", L"Zip", MB_OK); }
}

std::wstring previewFilePath = L""; bool isImagePreview = false;
LRESULT CALLBACK PreviewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 44, 52)); FillRect(hdc, &rc, bgBrush); DeleteObject(bgBrush);
        if (!previewFilePath.empty()) {
            Graphics graphics(hdc);
            if (isImagePreview) {
                Image image(previewFilePath.c_str());
                if (image.GetLastStatus() == Ok) { 
                    float r = min((float)(rc.right)/image.GetWidth(), (float)(rc.bottom)/image.GetHeight()); if(r>1)r=1; 
                    graphics.DrawImage(&image, 0, 0, (INT)(image.GetWidth()*r), (INT)(image.GetHeight()*r)); 
                }
            } else {
                std::wifstream file(previewFilePath); if (file.is_open()) { std::wstring c((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>()); Gdiplus::Font font(L"Consolas", 10); SolidBrush brush(Color(255, 220, 220, 220)); RectF l(10, 10, (REAL)(rc.right-20), (REAL)(rc.bottom-20)); graphics.DrawString(c.substr(0,500).c_str(), -1, &font, l, NULL, &brush); }
            }
        } EndPaint(hWnd, &ps);
    } return DefWindowProc(hWnd, message, wParam, lParam);
}
void UpdatePreview(std::wstring path) { previewFilePath = path; std::wstring ext = path.substr(path.find_last_of(L".") + 1); isImagePreview = (ext == L"jpg" || ext == L"png" || ext == L"bmp"); InvalidateRect(hPreviewWnd, NULL, TRUE); }

void Navigate(std::wstring path) {
    if (path.back() == '\\' && path.length() > 3) path.pop_back(); isInZipMode = false; currentPath = path; SetWindowText(hPathBox, currentPath.c_str()); ListView_DeleteAllItems(hListView);
    
    // FIX 1: Manually Add . and .. ALWAYS so we can navigate out of empty/system folders
    AddSpecialDir(L".");
    AddSpecialDir(L"..");

    WIN32_FIND_DATA fd; HANDLE hFind = FindFirstFile((path + L"\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // FIX 2: Skip system provided . and .. since we added them manually above
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            
            LVITEM lvi = {0}; lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM; lvi.iItem = ListView_GetItemCount(hListView); lvi.pszText = (LPWSTR)fd.cFileName;
            SHFILEINFO sfi; SHGetFileInfo((path + L"\\" + fd.cFileName).c_str(), fd.dwFileAttributes, &sfi, sizeof(sfi), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
            lvi.iImage = sfi.iIcon; lvi.lParam = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); ListView_InsertItem(hListView, &lvi);
            ListView_SetItemText(hListView, lvi.iItem, 1, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? (LPWSTR)L"<DIR>" : (LPWSTR)L"File");
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { std::wstring sz = std::to_wstring(fd.nFileSizeLow/1024) + L" KB"; ListView_SetItemText(hListView, lvi.iItem, 2, (LPWSTR)sz.c_str()); }
        } while (FindNextFile(hFind, &fd)); FindClose(hFind);
    }
}

void ShowContextMenu(POINT pt) {
    int i = ListView_GetNextItem(hListView, -1, LVNI_SELECTED); if (i == -1) return;
    wchar_t name[MAX_PATH]; ListView_GetItemText(hListView, i, 0, name, MAX_PATH);
    LVITEM lvi = {0}; lvi.iItem = i; lvi.mask = LVIF_PARAM; ListView_GetItem(hListView, &lvi);
    bool isDir = (bool)lvi.lParam;

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_OPEN, L"Open");
    AppendMenu(hMenu, MF_STRING, IDM_DELETE, L"Delete");
    if (!isDir) {
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, IDM_TEXT_EDIT, L"Edit (Text)");
        AppendMenu(hMenu, MF_STRING, IDM_HEX_EDIT, L"Edit (Hex)");
        AppendMenu(hMenu, MF_STRING, IDM_COMPRESS, L"Compress to ZIP");
    }
    int sel = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMainWnd, NULL); DestroyMenu(hMenu);
    
    std::wstring fullPath = currentPath + L"\\" + name;
    if (sel == IDM_OPEN) ShellExecute(NULL, L"open", fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    else if (sel == IDM_HEX_EDIT) OpenHexEditor(fullPath);
    else if (sel == IDM_TEXT_EDIT) OpenTextEditor(fullPath);
    else if (sel == IDM_DELETE) DeleteSelectedFile(fullPath);
    else if (sel == IDM_COMPRESS) { CompressToZip(fullPath); Navigate(currentPath); }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        hPathBox = CreateWindow(L"EDIT", L"C:\\", WS_CHILD|WS_VISIBLE|WS_BORDER, 0,0,0,0, hWnd, (HMENU)ID_PATHBOX, hInst, NULL);
        hListView = CreateWindow(WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHAREIMAGELISTS|LVS_SINGLESEL, 0,0,0,0, hWnd, (HMENU)ID_LISTVIEW, hInst, NULL);
        ListView_SetBkColor(hListView, COL_BG); ListView_SetTextBkColor(hListView, COL_BG); ListView_SetTextColor(hListView, COL_FG);
        LVCOLUMN c; c.mask=LVCF_TEXT|LVCF_WIDTH; c.pszText=(LPWSTR)L"Name"; c.cx=400; ListView_InsertColumn(hListView,0,&c); c.pszText=(LPWSTR)L"Type"; c.cx=100; ListView_InsertColumn(hListView,1,&c); c.pszText=(LPWSTR)L"Size"; c.cx=100; ListView_InsertColumn(hListView,2,&c);
        HIMAGELIST hImg = (HIMAGELIST)SHGetFileInfo(L"C:\\", 0, 0, 0, SHGFI_SYSICONINDEX | SHGFI_SMALLICON); ListView_SetImageList(hListView, hImg, LVSIL_SMALL);
        WNDCLASSEX wc = {0}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=PreviewWndProc; wc.hInstance=hInst; wc.lpszClassName=L"Prev"; RegisterClassEx(&wc);
        hPreviewWnd = CreateWindow(L"Prev", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, 0,0,0,0, hWnd, (HMENU)ID_PREVIEW, hInst, NULL);
        Navigate(L"C:\\");
    } break;
    case WM_SIZE: { RECT rc; GetClientRect(hWnd, &rc); MoveWindow(hPathBox, 5, 5, rc.right-10, 25, TRUE); MoveWindow(hListView, 0, 35, rc.right*0.7, rc.bottom-35, TRUE); MoveWindow(hPreviewWnd, rc.right*0.7, 35, rc.right*0.3, rc.bottom-35, TRUE); } break;
    case WM_NOTIFY: { LPNMHDR p=(LPNMHDR)lParam; if(p->idFrom==ID_LISTVIEW){ if(p->code==NM_DBLCLK){
        int i=ListView_GetNextItem(hListView, -1, LVNI_SELECTED); if(i!=-1){ wchar_t n[MAX_PATH]; ListView_GetItemText(hListView, i, 0, n, MAX_PATH); LVITEM l={0}; l.iItem=i; l.mask=LVIF_PARAM; ListView_GetItem(hListView, &l); if(l.lParam) Navigate(currentPath + L"\\" + n); else ShellExecute(NULL, L"open", (currentPath + L"\\" + n).c_str(), NULL, NULL, SW_SHOWNORMAL); }
    } else if(p->code==LVN_ITEMCHANGED) { LPNMLISTVIEW v=(LPNMLISTVIEW)lParam; if(v->uNewState&LVIS_SELECTED) { wchar_t n[MAX_PATH]; ListView_GetItemText(hListView, v->iItem, 0, n, MAX_PATH); UpdatePreview(currentPath + L"\\" + n); } }
      else if(p->code==NM_RCLICK) { POINT pt; GetCursorPos(&pt); ShowContextMenu(pt); } } } break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    } return 0;
}
int WINAPI wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int n) {
    hInst = h; InitCommonControls(); GdiplusStartupInput g; GdiplusStartup(&gdiplusToken, &g, NULL); EnableGodModePrivileges();
    WNDCLASSEX wc = {0}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=WndProc; wc.hInstance=h; wc.hCursor=LoadCursor(NULL, IDC_ARROW); wc.hbrBackground=(HBRUSH)GetStockObject(DKGRAY_BRUSH); wc.lpszClassName=L"ObsidianUlt"; RegisterClassEx(&wc);
    hMainWnd = CreateWindowEx(0, wc.lpszClassName, L"Obsidian Explorer - Ultimate Suite v3.2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800, NULL, NULL, h, NULL);
    EnableDarkTitleBar(hMainWnd); ShowWindow(hMainWnd, n); MSG msg; while(GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    GdiplusShutdown(gdiplusToken); return (int)msg.wParam;
}