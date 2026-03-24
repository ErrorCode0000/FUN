#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm>
#include <stdio.h>
#include <sstream>

using namespace Gdiplus;

#define ID_EDIT 101
#define ID_BUTTON 102
#define ID_LISTVIEW 103
#define ID_STATUSBAR 104

#define ID_MENU_OPEN 1001
#define ID_MENU_EXPLORE 1002
#define ID_MENU_CMD 1003
#define ID_MENU_PROPERTIES 1004
#define ID_MENU_COPY_PATH 1005
#define ID_MENU_DELETE 1006
#define ID_MENU_EXPORT 1007

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

// Global Değişkenler
HWND hEdit, hButton, hListView, hStatus, hMainWindow;
std::vector<SearchResult> searchResults;
std::mutex resultsMutex;
ULONGLONG searchStartTime = 0;
std::atomic<int> currentSearchGen(0);

int currentSortCol = -1;
bool sortAscending = true;
bool isAlwaysOnTop = false;

int spinnerFrame = 0; 
const wchar_t spinnerChars[] = { L'|', L'/', L'-', L'\\' };

HFONT hUIFont;
HBRUSH hDarkBrush;
COLORREF bgColor = RGB(25, 25, 25);
COLORREF textColor = RGB(0, 255, 255);
NOTIFYICONDATAW nid;

std::vector<std::wstring> SplitPatterns(const std::wstring& str, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstringstream wss(str);
    std::wstring token;
    while (std::getline(wss, token, delimiter)) {
        if (!token.empty()) {
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
        if (wParam == VK_RETURN) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_BUTTON, BN_CLICKED), (LPARAM)hButton);
            return 0;
        }
        if (wParam == VK_F11) {
            isAlwaysOnTop = !isAlwaysOnTop;
            SetWindowPos(GetParent(hwnd), isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            MessageBoxW(hwnd, isAlwaysOnTop ? L"Always on Top: ENABLED" : L"Always on Top: DISABLED", L"Window Mode", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
    }
    return CallWindowProcW(OldEditProc, hwnd, uMsg, wParam, lParam);
}

void RunProcess(const std::wstring& commandLine) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;
    std::wstring cmd = commandLine;
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
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

void FastSearchThread(std::wstring directory, const std::vector<std::wstring>& searchPatterns, int myGen) {
    std::wstring separator = (directory.back() == L'\\') ? L"" : L"\\";
    std::wstring searchPath = directory + separator + L"*";
    
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &findFileData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (currentSearchGen != myGen) {
                FindClose(hFind);
                return; 
            }

            std::wstring fileName = findFileData.cFileName;
            if (fileName == L"." || fileName == L"..") continue;

            bool isMatch = false;
            for (const auto& pat : searchPatterns) {
                if (PathMatchSpecW(fileName.c_str(), pat.c_str())) {
                    isMatch = true;
                    break;
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
                    sizeStr = L"";
                    typeStr = L"Folder";
                    rSize = 0; 
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

                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) typeStr += L" [Hidden]";

                FILETIME ftLocal;
                SYSTEMTIME st;
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
            }

            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
               !(findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                FastSearchThread(directory + separator + fileName, searchPatterns, myGen);
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
}

void SearchWrapper(HWND hwnd, std::vector<std::wstring> searchPatterns, int myGen) {
    if (currentSearchGen != myGen) return; 
    
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (currentSearchGen != myGen) break;
        if (drives & (1 << i)) {
            wchar_t root[4] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            UINT type = GetDriveTypeW(root);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                FastSearchThread(root, searchPatterns, myGen);
            }
        }
    }
    
    if (currentSearchGen == myGen) {
        PostMessageW(hwnd, WM_SEARCH_FINISHED, myGen, 0);
    }
}

void HandleContextMenu(HWND hwnd, int x, int y) {
    int index = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    HMENU hMenu = CreatePopupMenu();
    
    if (index != -1) {
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_OPEN, L"Open");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_EXPLORE, L"Open File Location");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_CMD, L"Open Command Prompt Here");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_PROPERTIES, L"Properties");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_COPY_PATH, L"Copy Path");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_DELETE, L"Delete");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    }
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_MENU_EXPORT, L"Export Results to CSV");

    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    if (selection == ID_MENU_EXPORT) { ExportResultsToCsv(hwnd); return; }
    if (index == -1) return;

    std::wstring pathStr;
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        if (index < searchResults.size()) pathStr = searchResults[index].path;
    }
    
    if (selection == ID_MENU_OPEN) RunProcess(L"\"" + pathStr + L"\"");
    else if (selection == ID_MENU_EXPLORE) OpenFolderAndSelect(pathStr);
    else if (selection == ID_MENU_CMD) {
        std::wstring dir = pathStr.substr(0, pathStr.find_last_of(L"\\"));
        RunProcess(L"cmd.exe /K cd /d \"" + dir + L"\"");
    }
    else if (selection == ID_MENU_PROPERTIES) ShowFileProperties(hwnd, pathStr);
    else if (selection == ID_MENU_COPY_PATH) CopyToClipboard(hwnd, pathStr);
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

            ListView_SetBkColor(hListView, bgColor);
            ListView_SetTextBkColor(hListView, bgColor);

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
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Graphics graphics(hdc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            RECT rc; GetClientRect(hwnd, &rc);
            
            SolidBrush bkgBrush(Color(255, 25, 25, 25));
            graphics.FillRectangle(&bkgBrush, 0, 0, rc.right, rc.bottom);

            LinearGradientBrush linGrBrush(Point(0, 0), Point(rc.right, 50), Color(255, 0, 150, 255), Color(255, 0, 255, 200));
            graphics.FillRectangle(&linGrBrush, 0, 0, rc.right, 50);

            FontFamily fontFamily(L"Segoe UI");
            Font font(&fontFamily, 16, FontStyleBold, UnitPoint);
            SolidBrush textBrush(Color(255, 255, 255, 255));
            
            graphics.DrawString(L"CYBER SEARCH PRO", -1, &font, PointF(10.0f, 12.0f), &textBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlID == ID_BUTTON) {
                SetBkMode(pdis->hDC, TRANSPARENT);
                HBRUSH btnBrush = CreateSolidBrush(pdis->itemState & ODS_SELECTED ? RGB(0, 150, 150) : RGB(40, 40, 40));
                FillRect(pdis->hDC, &pdis->rcItem, btnBrush);
                DeleteObject(btnBrush);
                
                HBRUSH borderBrush = CreateSolidBrush(RGB(0, 255, 255));
                FrameRect(pdis->hDC, &pdis->rcItem, borderBrush);
                DeleteObject(borderBrush);

                SetTextColor(pdis->hDC, RGB(0, 255, 255));
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
                    
                    std::thread(SearchWrapper, hwnd, patterns, newGen).detach();
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
                std::wstring statusMsg = L"[" + std::wstring(1, spinnerChar) + L"] Scanning all drives... Found: " + std::to_wstring(currentCount);
                SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)statusMsg.c_str());
            }
            break;

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == ID_LISTVIEW) {
                
                // CRASH (ÇÖKME) ÇÖZÜMÜ BURADA BAŞLIYOR
                if (nmhdr->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (currentSortCol == pnmv->iSubItem) sortAscending = !sortAscending;
                    else { currentSortCol = pnmv->iSubItem; sortAscending = true; }
                    
                    // BUG FIX 1: ListView odak çökmesini engellemek için seçimleri temizle
                    ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    if (searchResults.empty()) break; // Liste boşsa işlemi kes

                    // BUG FIX 2: Strict Weak Ordering hatasını engelleyen std::stable_sort ve StrCmpLogicalW mantığı
                    std::stable_sort(searchResults.begin(), searchResults.end(), [&](const SearchResult& a, const SearchResult& b) {
                        if (currentSortCol == 0) {
                            int cmp = StrCmpLogicalW(a.path.c_str(), b.path.c_str());
                            return sortAscending ? (cmp < 0) : (cmp > 0);
                        }
                        if (currentSortCol == 1) {
                            return sortAscending ? (a.rawSize < b.rawSize) : (a.rawSize > b.rawSize);
                        }
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
                    
                    // YENİ ÖZELLİK 2: Sıralama sonrası sütunları otomatik daralt/genişlet
                    for(int i=0; i<4; i++) ListView_SetColumnWidth(hListView, i, LVSCW_AUTOSIZE);
                }
                else if (nmhdr->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
                    if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                    else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                        if (pLVCD->nmcd.dwItemSpec % 2 == 0) pLVCD->clrTextBk = RGB(35, 35, 35);
                        else pLVCD->clrTextBk = RGB(25, 25, 25);
                        
                        std::wstring type = L"";
                        {
                            std::lock_guard<std::mutex> lock(resultsMutex);
                            if (pLVCD->nmcd.dwItemSpec < searchResults.size()) {
                                type = searchResults[pLVCD->nmcd.dwItemSpec].typeStr;
                            }
                        }

                        // YENİ ÖZELLİK 1: Dosya Türü Renklendirmesi (Type Color Coding)
                        if (type == L"Folder") pLVCD->clrText = RGB(0, 200, 255); // Mavi/Camgöbeği
                        else if (type.find(L".exe") != std::wstring::npos || type.find(L".dll") != std::wstring::npos) pLVCD->clrText = RGB(255, 100, 100); // Kırmızı
                        else if (type.find(L".txt") != std::wstring::npos || type.find(L".log") != std::wstring::npos || type.find(L".docx") != std::wstring::npos) pLVCD->clrText = RGB(100, 255, 100); // Yeşil
                        else if (type.find(L".jpg") != std::wstring::npos || type.find(L".png") != std::wstring::npos || type.find(L".mp4") != std::wstring::npos) pLVCD->clrText = RGB(200, 100, 255); // Mor
                        else pLVCD->clrText = textColor; // Varsayılan Camgöbeği
                        
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
                
                // YENİ ÖZELLİK 2 DEVAMI: Arama bittiğinde sütunları mükemmel boyuta getir
                for(int i=0; i<4; i++) ListView_SetColumnWidth(hListView, i, LVSCW_AUTOSIZE);

                ULONGLONG elapsedStr = GetTickCount64() - searchStartTime;
                double seconds = elapsedStr / 1000.0;
                
                wchar_t statusFinal[256];
                swprintf(statusFinal, 256, L"Search completed in %.2f seconds. Total found: %zu", seconds, searchResults.size());
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
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(bgColor);
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