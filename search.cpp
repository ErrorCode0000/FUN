#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <psapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm>
#include <stdio.h>
#include <sstream>
#include <map>
#include <regex> 

using namespace Gdiplus;

// UI IDs
#define ID_EDIT 101
#define ID_BUTTON 102
#define ID_LISTVIEW 103
#define ID_STATUSBAR 104

// Context Menu IDs
#define ID_MENU_OPEN 1001
#define ID_MENU_EXPLORE 1002
#define ID_MENU_CMD 1003
#define ID_MENU_PROPERTIES 1004
#define ID_MENU_COPY_PATH 1005
#define ID_MENU_DELETE 1006
#define ID_MENU_EXPORT 1007
#define ID_MENU_EXPORT_JSON 1008  
#define ID_MENU_SHREDDER 1009     
#define ID_MENU_RUN_ADMIN 1010    

#define WM_SEARCH_FINISHED (WM_USER + 1)
#define WM_TRAYICON (WM_USER + 2)
#define TIMER_UPDATE_UI 1

#ifndef FIND_FIRST_EX_LARGE_FETCH
#define FIND_FIRST_EX_LARGE_FETCH 2
#endif

struct SearchResult {
    std::wstring path;
    std::wstring sizeStr;
    std::wstring dateStr;
    std::wstring typeStr;
    int iconIndex;
    ULONGLONG rawSize;
    FILETIME rawDate;
};

// Global Variables
HWND hEdit, hButton, hListView, hStatus, hMainWindow;
std::vector<SearchResult> searchResults;
std::mutex resultsMutex;
ULONGLONG searchStartTime = 0;
std::atomic<int> currentSearchGen(0);
int currentSortCol = -1;
bool sortAscending = true;
bool isAlwaysOnTop = false;

// SECRET MODE FLAGS
bool isMKTMode = false;
bool isCBAMode = false;
bool isWaitingForCheat = false;

// Drive Capacities Map for Dynamic Ratio
std::map<wchar_t, ULONGLONG> driveCapacities; 
int spinnerFrame = 0; 
const wchar_t spinnerChars[] = { L'|', L'/', L'-', L'\\' };

// UI Styling
HFONT hUIFont;
HBRUSH hDarkBrush;
COLORREF bgColor = RGB(25, 25, 25);
COLORREF textColor = RGB(0, 255, 255);
COLORREF gradientStart = RGB(0, 150, 255);
COLORREF gradientEnd = RGB(0, 255, 200);
NOTIFYICONDATAW nid;

void ActivateMKTMode() {
    isMKTMode = true;
    bgColor = RGB(0, 0, 0);       
    textColor = RGB(0, 255, 0);   
    gradientStart = RGB(0, 50, 0);
    gradientEnd = RGB(0, 200, 0);
    hDarkBrush = CreateSolidBrush(bgColor);
    
    // Set transparency to 90%
    SetWindowLongW(hMainWindow, GWL_EXSTYLE, GetWindowLongW(hMainWindow, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hMainWindow, 0, 230, LWA_ALPHA); 
    
    // Force Topmost
    SetWindowPos(hMainWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowTextW(hMainWindow, L"MUSTAFA KEMAL TASTEKIN ARCHITECTURE ACTIVE - HACKER MODE");
    InvalidateRect(hMainWindow, NULL, TRUE);
    MessageBeep(MB_ICONWARNING);
}

void ActivateCBAMode() {
    isMKTMode = true; // CBA inherits MKT features
    isCBAMode = true;
    bgColor = RGB(40, 0, 0);        
    textColor = RGB(255, 215, 0);   
    gradientStart = RGB(150, 0, 0);
    gradientEnd = RGB(255, 50, 50);
    hDarkBrush = CreateSolidBrush(bgColor);
    
    // Set transparency to 80%
    SetWindowLongW(hMainWindow, GWL_EXSTYLE, GetWindowLongW(hMainWindow, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hMainWindow, 0, 200, LWA_ALPHA); 
    
    // Force Topmost
    SetWindowPos(hMainWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowTextW(hMainWindow, L"CBA - OMEGA LEVEL DEVELOPER MODE");
    InvalidateRect(hMainWindow, NULL, TRUE);
    MessageBeep(MB_ICONASTERISK);
}

// Multi-pattern Wildcard Parser
std::vector<std::wstring> SplitPatterns(const std::wstring& str, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstringstream wss(str);
    std::wstring token;
    while (std::getline(wss, token, delimiter)) {
        if (!token.empty()) {
            // Auto-append '*' if no wildcard is provided by the user
            if (token.find(L'*') == std::wstring::npos && token.find(L'?') == std::wstring::npos) {
                tokens.push_back(L"*" + token + L"*");
            } else {
                tokens.push_back(token);
            }
        }
    }
    return tokens;
}

WNDPROC OldEditProc;
LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        // SECRET CODE TRIGGER: Ctrl + U
        if (wParam == 'U' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            isWaitingForCheat = true;
            SetWindowTextW(GetParent(hwnd), L"--- ENTER SECRET CHEAT CODE ---");
            SetWindowTextW(hwnd, L"");
            SendMessageW(hwnd, EM_SETPASSWORDCHAR, (WPARAM)L'*', 0); // Hide input
            return 0;
        }

        if (wParam == VK_RETURN) {
            if (isWaitingForCheat) {
                wchar_t buffer[256]; GetWindowTextW(hwnd, buffer, 256);
                std::wstring code(buffer);
                
                if (code == L"mkt") ActivateMKTMode();
                else if (code == L"cba") ActivateCBAMode();
                else {
                    SetWindowTextW(GetParent(hwnd), L"Cyber Search Pro");
                    MessageBeep(MB_ICONERROR);
                }
                
                isWaitingForCheat = false;
                SendMessageW(hwnd, EM_SETPASSWORDCHAR, 0, 0); // Restore normal input
                SetWindowTextW(hwnd, L"");
                return 0;
            }
            
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_BUTTON, BN_CLICKED), (LPARAM)hButton);
            return 0;
        }
        if (wParam == VK_F11) {
            isAlwaysOnTop = !isAlwaysOnTop;
            SetWindowPos(GetParent(hwnd), isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return 0;
        }
    }
    return CallWindowProcW(OldEditProc, hwnd, uMsg, wParam, lParam);
}

void RunProcess(const std::wstring& commandLine, bool asAdmin = false) {
    if (asAdmin) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = commandLine.c_str();
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
    } else {
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi;
        std::wstring cmd = commandLine;
        if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    }
}

void OpenFolderAndSelect(const std::wstring& targetPath) {
    wchar_t explorerPath[MAX_PATH];
    ExpandEnvironmentStringsW(L"%WINDIR%\\explorer.exe", explorerPath, MAX_PATH);
    std::wstring cmdLine = L"\"" + std::wstring(explorerPath) + L"\" /select,\"" + targetPath + L"\"";
    RunProcess(cmdLine);
}

void CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* memData = (wchar_t*)GlobalLock(hMem);
            memcpy(memData, text.c_str(), (text.length() + 1) * sizeof(wchar_t));
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

void ShowFileProperties(HWND hwnd, const std::wstring& path) {
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.hwnd = hwnd;
    sei.lpVerb = L"properties";
    sei.lpFile = path.c_str();
    ShellExecuteExW(&sei);
}

void ExportResultsToCsv(HWND hwnd) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    if (searchResults.empty()) return;
    FILE* f;
    if (_wfopen_s(&f, L"CyberSearch_Report.csv", L"w, ccs=UTF-8") == 0) {
        fwprintf(f, L"\xFEFF");
        fwprintf(f, L"\"Location\",\"Size\",\"Date Modified\",\"Type\"\n");
        for (const auto& res : searchResults) {
            fwprintf(f, L"\"%s\",\"%s\",\"%s\",\"%s\"\n", res.path.c_str(), res.sizeStr.c_str(), res.dateStr.c_str(), res.typeStr.c_str());
        }
        fclose(f);
        MessageBoxW(hwnd, L"Results exported to 'CyberSearch_Report.csv'.", L"Export Success", MB_OK | MB_ICONINFORMATION);
    }
}

// MKT EXCLUSIVE: JSON Export
void ExportResultsToJson(HWND hwnd) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    if (searchResults.empty()) return;
    FILE* f;
    if (_wfopen_s(&f, L"CyberSearch_Dump.json", L"w, ccs=UTF-8") == 0) {
        fwprintf(f, L"{\n  \"results\": [\n");
        for (size_t i = 0; i < searchResults.size(); ++i) {
            std::wstring p = searchResults[i].path;
            size_t pos = 0;
            while ((pos = p.find(L"\\", pos)) != std::wstring::npos) { p.replace(pos, 1, L"\\\\"); pos += 2; }
            fwprintf(f, L"    {\"path\": \"%s\", \"size\": \"%s\"}%s\n", p.c_str(), searchResults[i].sizeStr.c_str(), (i == searchResults.size()-1) ? L"" : L",");
        }
        fwprintf(f, L"  ]\n}\n");
        fclose(f);
        MessageBoxW(hwnd, L"MKT Protocol: Data dumped to JSON.", L"Export Success", MB_OK | MB_ICONINFORMATION);
    }
}

// MKT EXCLUSIVE: File Shredder
void ShredFile(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char zeroData[1024] = {0};
        DWORD bytesWritten;
        for(int i=0; i<10; i++) WriteFile(hFile, zeroData, sizeof(zeroData), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    DeleteFileW(path.c_str());
}

// Core Search Function
void FastSearchThread(std::wstring directory, const std::vector<std::wstring>& searchPatterns, std::wstring rawPattern, int myGen) {
    std::wstring separator = (directory.back() == L'\\') ? L"" : L"\\";
    std::wstring searchPath = directory + separator + L"*";
    
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &findFileData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    bool isRegex = false;
    std::wregex regexPattern;
    if (isCBAMode && rawPattern.length() > 1 && rawPattern[0] == L'>') {
        isRegex = true;
        try { regexPattern = std::wregex(rawPattern.substr(1), std::regex_constants::icase); } catch(...) { isRegex = false; }
    }

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (currentSearchGen != myGen) { FindClose(hFind); return; }

            std::wstring fileName = findFileData.cFileName;
            if (fileName == L"." || fileName == L"..") continue;

            bool isMatch = false;
            if (isRegex) {
                if (std::regex_search(fileName, regexPattern)) isMatch = true;
            } else {
                for (const auto& pat : searchPatterns) {
                    if (PathMatchSpecW(fileName.c_str(), pat.c_str())) { isMatch = true; break; }
                }
            }

            if (isMatch) {
                std::wstring fullPath = directory + separator + fileName;
                std::wstring sizeStr = L"";
                std::wstring typeStr = L"";
                std::wstring dateStr = L"";
                ULONGLONG rSize = 0;
                int iconIdx = 0;

                bool isDir = (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

                if (isDir) {
                    sizeStr = L""; typeStr = L"Folder"; rSize = 0; 
                } else {
                    ULARGE_INTEGER fileSize;
                    fileSize.HighPart = findFileData.nFileSizeHigh;
                    fileSize.LowPart = findFileData.nFileSizeLow;
                    rSize = fileSize.QuadPart;
                    double kb = rSize / 1024.0;
                    double mb = kb / 1024.0;
                    wchar_t buf[64];
                    if (mb >= 1.0) swprintf(buf, 64, L"%.2f MB", mb);
                    else swprintf(buf, 64, L"%.0f KB", kb);
                    sizeStr = buf;
                    
                    LPCWSTR ext = PathFindExtensionW(fileName.c_str());
                    if (ext) typeStr = ext;
                }

                // MKT EXCLUSIVE: Highlight hidden files
                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                    typeStr += (isMKTMode ? L" [TOP SECRET]" : L" [Hidden]");
                }

                FILETIME ftLocal; SYSTEMTIME st;
                FileTimeToLocalFileTime(&findFileData.ftLastWriteTime, &ftLocal);
                FileTimeToSystemTime(&ftLocal, &st);
                wchar_t dateBuf[64];
                swprintf(dateBuf, 64, L"%02d.%02d.%04d %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
                dateStr = dateBuf;

                SHFILEINFOW sfi = {0};
                SHGetFileInfoW(fileName.c_str(), findFileData.dwFileAttributes, &sfi, sizeof(sfi), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
                iconIdx = sfi.iIcon;

                std::lock_guard<std::mutex> lock(resultsMutex);
                searchResults.push_back({fullPath, sizeStr, dateStr, typeStr, iconIdx, rSize, findFileData.ftLastWriteTime});
                
                // MKT EXCLUSIVE: Copy first match to clipboard
                if (isMKTMode && searchResults.size() == 1) CopyToClipboard(hMainWindow, fullPath);
            }

            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
               !(findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                FastSearchThread(directory + separator + fileName, searchPatterns, rawPattern, myGen);
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
}

void SearchWrapper(HWND hwnd, std::vector<std::wstring> searchPatterns, std::wstring rawPattern, int myGen) {
    if (currentSearchGen != myGen) return; 
    
    // CBA EXCLUSIVE: Prevent sleep mode and set max priority
    if (isCBAMode) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }
    
    driveCapacities.clear(); 
    DWORD drives = GetLogicalDrives();
    
    for (int i = 0; i < 26; i++) {
        if (currentSearchGen != myGen) break;
        if (drives & (1 << i)) {
            wchar_t root[4] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            UINT type = GetDriveTypeW(root);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                ULARGE_INTEGER f, t, tf;
                if (GetDiskFreeSpaceExW(root, &f, &t, &tf)) driveCapacities[(wchar_t)(L'A' + i)] = t.QuadPart;
                FastSearchThread(root, searchPatterns, rawPattern, myGen);
            }
        }
    }
    
    // Remove sleep prevention
    if (isCBAMode) SetThreadExecutionState(ES_CONTINUOUS); 
    
    if (currentSearchGen == myGen) {
        PostMessageW(hwnd, WM_SEARCH_FINISHED, myGen, 0);
    }
}

void HandleContextMenu(HWND hwnd, int x, int y) {
    int index = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    HMENU hMenu = CreatePopupMenu();
    
    if (index != -1) {
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_OPEN, L"Open");
        if (isCBAMode) InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_RUN_ADMIN, L"[CBA] Run as Administrator");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_EXPLORE, L"Open File Location");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_CMD, L"Open Command Prompt Here");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_PROPERTIES, L"Properties");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_COPY_PATH, L"Copy Path");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        if (isMKTMode) InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_SHREDDER, L"[MKT] Shred File (Secure Erase)");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_DELETE, L"Delete");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    }
    
    if (isMKTMode) InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_EXPORT_JSON, L"[MKT] Export to JSON");
    else InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_EXPORT, L"Export Results to CSV");

    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    if (selection == ID_MENU_EXPORT) { ExportResultsToCsv(hwnd); return; }
    if (selection == ID_MENU_EXPORT_JSON) { ExportResultsToJson(hwnd); return; }
    if (index == -1) return;

    std::wstring pathStr;
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        if (index < searchResults.size()) pathStr = searchResults[index].path;
    }
    
    if (selection == ID_MENU_OPEN) RunProcess(L"\"" + pathStr + L"\"");
    else if (selection == ID_MENU_RUN_ADMIN) RunProcess(pathStr, true);
    else if (selection == ID_MENU_EXPLORE) OpenFolderAndSelect(pathStr);
    else if (selection == ID_MENU_CMD) {
        std::wstring dir = pathStr.substr(0, pathStr.find_last_of(L"\\"));
        RunProcess(L"cmd.exe /K cd /d \"" + dir + L"\"");
    }
    else if (selection == ID_MENU_PROPERTIES) ShowFileProperties(hwnd, pathStr);
    else if (selection == ID_MENU_COPY_PATH) CopyToClipboard(hwnd, pathStr);
    else if (selection == ID_MENU_SHREDDER) {
        if (MessageBoxW(hwnd, L"File will be overwritten with zeros and destroyed forever. Proceed?", L"MKT Protocol", MB_ICONWARNING | MB_YESNO) == IDYES) {
            ShredFile(pathStr);
            std::lock_guard<std::mutex> lock(resultsMutex);
            searchResults.erase(searchResults.begin() + index);
            ListView_SetItemCountEx(hListView, searchResults.size(), LVSICF_NOINVALIDATEALL);
        }
    }
    else if (selection == ID_MENU_DELETE) {
        if (MessageBoxW(hwnd, L"Permanently delete?", L"Warning", MB_ICONWARNING | MB_YESNO) == IDYES) {
            if (DeleteFileW(pathStr.c_str())) {
                std::lock_guard<std::mutex> lock(resultsMutex);
                searchResults.erase(searchResults.begin() + index);
                ListView_SetItemCountEx(hListView, searchResults.size(), LVSICF_NOINVALIDATEALL);
            }
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            hMainWindow = hwnd;
            InitCommonControls();
            hDarkBrush = CreateSolidBrush(bgColor);
            hUIFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

            hEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 60, 400, 25, hwnd, (HMENU)ID_EDIT, NULL, NULL);
            OldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);

            hButton = CreateWindowW(L"BUTTON", L"SEARCH", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 420, 60, 100, 25, hwnd, (HMENU)ID_BUTTON, NULL, NULL);
            hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 10, 95, 760, 300, hwnd, (HMENU)ID_LISTVIEW, NULL, NULL);
            hStatus = CreateWindowW(STATUSCLASSNAMEW, L"Ready. (Press F11 for Always on Top)", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd, (HMENU)ID_STATUSBAR, NULL, NULL);
            
            ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hUIFont, TRUE);
            SendMessageW(hListView, WM_SETFONT, (WPARAM)hUIFont, TRUE);

            SHFILEINFOW sfi;
            HIMAGELIST himl = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
            ListView_SetImageList(hListView, himl, LVSIL_SMALL);

            LVCOLUMNW lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            
            lvc.pszText = (LPWSTR)L"Name & Location"; lvc.cx = 400; ListView_InsertColumn(hListView, 0, &lvc);
            lvc.pszText = (LPWSTR)L"Size"; lvc.cx = 80; ListView_InsertColumn(hListView, 1, &lvc);
            lvc.pszText = (LPWSTR)L"Date Modified"; lvc.cx = 140; ListView_InsertColumn(hListView, 2, &lvc);
            lvc.pszText = (LPWSTR)L"Type"; lvc.cx = 120; ListView_InsertColumn(hListView, 3, &lvc);

            memset(&nid, 0, sizeof(NOTIFYICONDATAW));
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hwnd;
            nid.uID = 5000;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
            wcscpy_s(nid.szTip, L"CyberSearch Pro");
            Shell_NotifyIconW(NIM_ADD, &nid);
            break;
        }

        case WM_TRAYICON:
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) { ShowWindow(hwnd, SW_HIDE); return 0; }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Graphics graphics(hdc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            RECT rc; GetClientRect(hwnd, &rc);
            
            SolidBrush bkgBrush(Color(255, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor)));
            graphics.FillRectangle(&bkgBrush, 0, 0, rc.right, rc.bottom);

            LinearGradientBrush linGrBrush(Point(0, 0), Point(rc.right, 50), 
                Color(255, GetRValue(gradientStart), GetGValue(gradientStart), GetBValue(gradientStart)), 
                Color(255, GetRValue(gradientEnd), GetGValue(gradientEnd), GetBValue(gradientEnd)));
            graphics.FillRectangle(&linGrBrush, 0, 0, rc.right, 50);

            FontFamily fontFamily(L"Segoe UI");
            Font font(&fontFamily, 16, FontStyleBold, UnitPoint);
            SolidBrush textBrush(Color(255, 255, 255, 255));
            
            wchar_t titleBuf[256];
            GetWindowTextW(hwnd, titleBuf, 256);
            graphics.DrawString(titleBuf, -1, &font, PointF(10.0f, 12.0f), &textBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlID == ID_BUTTON) {
                SetBkMode(pdis->hDC, TRANSPARENT);
                HBRUSH btnBrush = CreateSolidBrush(pdis->itemState & ODS_SELECTED ? gradientStart : RGB(40, 40, 40));
                FillRect(pdis->hDC, &pdis->rcItem, btnBrush);
                DeleteObject(btnBrush);
                
                HBRUSH borderBrush = CreateSolidBrush(textColor);
                FrameRect(pdis->hDC, &pdis->rcItem, borderBrush);
                DeleteObject(borderBrush);

                SetTextColor(pdis->hDC, textColor);
                SelectObject(pdis->hDC, hUIFont);
                DrawTextW(pdis->hDC, L"SEARCH", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            SetTextColor((HDC)wParam, textColor);
            SetBkColor((HDC)wParam, bgColor);
            return (INT_PTR)hDarkBrush;

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            SendMessageW(hStatus, WM_SIZE, 0, 0);
            RECT statusRect; GetWindowRect(hStatus, &statusRect);
            int sHeight = statusRect.bottom - statusRect.top;

            MoveWindow(hEdit, 10, 60, width - 130, 25, TRUE);
            MoveWindow(hButton, width - 110, 60, 100, 25, TRUE);
            MoveWindow(hListView, 10, 95, width - 20, height - 105 - sHeight, TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BUTTON) {
                int newGen = ++currentSearchGen; 
                
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    searchResults.clear();
                }
                ListView_SetItemCountEx(hListView, 0, LVSICF_NOINVALIDATEALL);

                wchar_t buffer[256]; GetWindowTextW(hEdit, buffer, 256);
                std::wstring rawInput(buffer);

                if (!rawInput.empty()) {
                    std::vector<std::wstring> patterns = SplitPatterns(rawInput, L';');
                    spinnerFrame = 0;
                    searchStartTime = GetTickCount64();
                    
                    std::thread(SearchWrapper, hwnd, patterns, rawInput, newGen).detach();
                    SetTimer(hwnd, TIMER_UPDATE_UI, 100, NULL);
                }
            }
            break;

        case WM_TIMER:
            if (wParam == TIMER_UPDATE_UI) {
                size_t currentCount = 0;
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    currentCount = searchResults.size();
                }
                ListView_SetItemCountEx(hListView, currentCount, LVSICF_NOINVALIDATEALL);
                
                wchar_t spinnerChar = spinnerChars[spinnerFrame % 4];
                spinnerFrame++;
                
                std::wstring ramUsage = L"";
                if (isCBAMode) {
                    PROCESS_MEMORY_COUNTERS pmc;
                    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                        ramUsage = L" | RAM: " + std::to_wstring(pmc.WorkingSetSize / 1024 / 1024) + L" MB";
                    }
                }

                std::wstring statusMsg = L"[" + std::wstring(1, spinnerChar) + L"] Scanning drives... Found: " + std::to_wstring(currentCount) + ramUsage;
                if (isMKTMode && !isCBAMode) statusMsg += L" | MKT SYSTEM ACTIVE";
                
                SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)statusMsg.c_str());
            }
            break;

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == ID_LISTVIEW) {
                if (nmhdr->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (currentSortCol == pnmv->iSubItem) sortAscending = !sortAscending;
                    else { currentSortCol = pnmv->iSubItem; sortAscending = true; }
                    
                    ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    if (searchResults.empty()) break; 

                    std::stable_sort(searchResults.begin(), searchResults.end(), [&](const SearchResult& a, const SearchResult& b) {
                        if (currentSortCol == 0) {
                            int cmp = StrCmpLogicalW(a.path.c_str(), b.path.c_str());
                            return sortAscending ? (cmp < 0) : (cmp > 0);
                        }
                        if (currentSortCol == 1) return sortAscending ? (a.rawSize < b.rawSize) : (a.rawSize > b.rawSize);
                        if (currentSortCol == 2) {
                            LONG cmp = CompareFileTime(&a.rawDate, &b.rawDate);
                            return sortAscending ? (cmp < 0) : (cmp > 0);
                        }
                        if (currentSortCol == 3) {
                            int cmp = StrCmpLogicalW(a.typeStr.c_str(), b.typeStr.c_str());
                            return sortAscending ? (cmp < 0) : (cmp > 0);
                        }
                        return false;
                    });
                    
                    ListView_RedrawItems(hListView, 0, searchResults.size() - 1);
                }
                else if (nmhdr->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
                    if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                    else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                        if (isMKTMode) {
                            pLVCD->clrTextBk = (pLVCD->nmcd.dwItemSpec % 2 == 0) ? RGB(0, 30, 0) : RGB(0, 10, 0);
                        } else {
                            pLVCD->clrTextBk = (pLVCD->nmcd.dwItemSpec % 2 == 0) ? RGB(35, 35, 35) : RGB(25, 25, 25);
                        }
                        
                        std::wstring type = L"";
                        {
                            std::lock_guard<std::mutex> lock(resultsMutex);
                            if (pLVCD->nmcd.dwItemSpec < searchResults.size()) type = searchResults[pLVCD->nmcd.dwItemSpec].typeStr;
                        }

                        if (type.find(L"Folder") != std::wstring::npos) pLVCD->clrText = isCBAMode ? RGB(255,200,100) : RGB(0, 200, 255); 
                        else if (type.find(L".exe") != std::wstring::npos || type.find(L".dll") != std::wstring::npos) pLVCD->clrText = isMKTMode ? RGB(255, 0, 0) : RGB(255, 100, 100); 
                        else if (type.find(L".txt") != std::wstring::npos || type.find(L".log") != std::wstring::npos) pLVCD->clrText = isMKTMode ? RGB(0, 255, 100) : RGB(100, 255, 100); 
                        else pLVCD->clrText = textColor; 
                        
                        // Dynamic Ratio Calculation
                        ULONGLONG size = 0;
                        wchar_t driveLetter = L'C';
                        {
                            std::lock_guard<std::mutex> lock(resultsMutex);
                            if (pLVCD->nmcd.dwItemSpec < searchResults.size()) {
                                size = searchResults[pLVCD->nmcd.dwItemSpec].rawSize;
                                if (!searchResults[pLVCD->nmcd.dwItemSpec].path.empty()) {
                                    driveLetter = towupper(searchResults[pLVCD->nmcd.dwItemSpec].path[0]);
                                }
                            }
                        }
                        
                        double ratio = 0.0;
                        if (driveCapacities.count(driveLetter) > 0 && driveCapacities[driveLetter] > 0) {
                            ratio = (double)size / driveCapacities[driveLetter] * 100.0;
                        }

                        if (ratio > 5.0) pLVCD->clrText = RGB(255, 80, 80); 
                        else if (ratio > 1.0) pLVCD->clrText = RGB(255, 180, 0); 
                        
                        return CDRF_NEWFONT;
                    }
                }
                else if (nmhdr->code == LVN_GETDISPINFOW) {
                    NMLVDISPINFOW* plvdi = (NMLVDISPINFOW*)lParam;
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    if (plvdi->item.iItem < searchResults.size()) {
                        if (plvdi->item.mask & LVIF_TEXT) {
                            if (plvdi->item.iSubItem == 0) lstrcpynW(plvdi->item.pszText, searchResults[plvdi->item.iItem].path.c_str(), plvdi->item.cchTextMax);
                            else if (plvdi->item.iSubItem == 1) lstrcpynW(plvdi->item.pszText, searchResults[plvdi->item.iItem].sizeStr.c_str(), plvdi->item.cchTextMax);
                            else if (plvdi->item.iSubItem == 2) lstrcpynW(plvdi->item.pszText, searchResults[plvdi->item.iItem].dateStr.c_str(), plvdi->item.cchTextMax);
                            else if (plvdi->item.iSubItem == 3) lstrcpynW(plvdi->item.pszText, searchResults[plvdi->item.iItem].typeStr.c_str(), plvdi->item.cchTextMax);
                        }
                        if (plvdi->item.mask & LVIF_IMAGE) {
                            plvdi->item.iImage = searchResults[plvdi->item.iItem].iconIndex;
                        }
                    }
                }
                else if (nmhdr->code == NM_DBLCLK) {
                    int index = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                    if (index != -1) {
                        std::wstring pathStr;
                        {
                            std::lock_guard<std::mutex> lock(resultsMutex);
                            pathStr = searchResults[index].path;
                        }
                        RunProcess(L"\"" + pathStr + L"\"");
                    }
                }
            }
            break;
        }

        case WM_CONTEXTMENU:
            if ((HWND)wParam == hListView) HandleContextMenu(hwnd, LOWORD(lParam), HIWORD(lParam));
            break;

        case WM_SEARCH_FINISHED: {
            if (wParam == currentSearchGen) {
                KillTimer(hwnd, TIMER_UPDATE_UI);
                ListView_SetItemCountEx(hListView, searchResults.size(), LVSICF_NOINVALIDATEALL);
                
                // CBA EXCLUSIVE: Auto Column Alignment
                if (isCBAMode) {
                    for(int i=0; i<4; i++) ListView_SetColumnWidth(hListView, i, LVSCW_AUTOSIZE);
                }

                ULONGLONG elapsedStr = GetTickCount64() - searchStartTime;
                double seconds = elapsedStr / 1000.0;
                
                wchar_t statusFinal[256];
                swprintf(statusFinal, 256, L"Search completed in %.2f seconds. Total found: %zu", seconds, searchResults.size());
                
                if (isMKTMode && !isCBAMode) MessageBeep(MB_OK); 

                SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)statusFinal);
            }
            break;
        }

        case WM_DESTROY:
            currentSearchGen++; 
            Shell_NotifyIconW(NIM_DELETE, &nid); 
            KillTimer(hwnd, TIMER_UPDATE_UI);
            SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)OldEditProc);
            DeleteObject(hDarkBrush);
            DeleteObject(hUIFont);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CyberSearchClass";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"CyberSearchClass", L"Cyber Search Pro", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 500, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}