// installer.c - SDLite Windows bootstrap installer (pure C17, MinGW-friendly)
// Build (exact):
// gcc -std=c17 -O2 -s -municode SDLiteInstaller.c -o SDLiteSetup.exe ^
//   -static-libgcc ^
//   -lwinhttp -lole32 -loleaut32 -lshell32 -lcomctl32 -luuid -lshlwapi -lbcrypt -lgdi32
//
// Notes:
// - Pure C (no C++)
// - Uses WinHTTP for downloads (handles redirects, validates status)
// - Uses Shell.Application COM ZIP support for extraction (no PowerShell)
// - Handles async extraction by waiting for directory to become stable
// - Safely unwraps GitHub repo double-wrappers
// - Flattens SDL wrapper folders until expected headers exist
// - Never creates a directory using a full file path (prevents README.md -> folder bug)
// - Cleans up temp directories at the end

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <stdint.h>
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

// ========================= CONFIG =========================
// URLs will be provided/edited by you:
static const wchar_t *REPO_ZIP_URL =
    L"https://github.com/therealpixeles/SDLite/archive/refs/heads/main.zip";

static const wchar_t *SDL2_ZIP_URL =
    L"https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip";

static const wchar_t *SDL2_IMAGE_ZIP_URL =
    L"https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.8/SDL2_image-devel-2.8.8-mingw.zip";

// Creates this subfolder inside chosen folder:
static const wchar_t *INSTALL_SUBFOLDER = L"SDLite";

// Repo root markers:
static const wchar_t *ROOT_MARKERS[] = { L"include", L"src", L"res" };

// Expected SDL header markers:
static const wchar_t *SDL2_MARKER_REL       = L"external\\SDL2\\include\\SDL2\\SDL.h";
static const wchar_t *SDL2_IMAGE_MARKER_REL = L"external\\SDL2_image\\include\\SDL2\\SDL_image.h";

// =========================================================

static void fatalf(const wchar_t *fmt, ...) {
    wchar_t buf[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, ARRAYSIZE(buf), fmt, ap);
    va_end(ap);
    buf[ARRAYSIZE(buf)-1] = 0;
    MessageBoxW(NULL, buf, L"SDLite Setup", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void warnf(const wchar_t *fmt, ...) {
    wchar_t buf[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, ARRAYSIZE(buf), fmt, ap);
    va_end(ap);
    buf[ARRAYSIZE(buf)-1] = 0;
    MessageBoxW(NULL, buf, L"SDLite Setup", MB_OK | MB_ICONWARNING);
}

static int path_exists(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES;
}

static int is_dir(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int is_file(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void path_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b) {
    if (!a || !b) { if (cap) out[0] = 0; return; }
    size_t la = wcslen(a);
    if (la && (a[la-1] == L'\\' || a[la-1] == L'/')) {
        _snwprintf(out, cap, L"%s%s", a, b);
    } else {
        _snwprintf(out, cap, L"%s\\%s", a, b);
    }
    out[cap-1] = 0;
}

static void normalize_slashes(wchar_t *s) {
    for (; s && *s; ++s) if (*s == L'/') *s = L'\\';
}

// Creates directories along a directory path (NOT for full file paths).
static void ensure_dir_recursive(const wchar_t *dirPath) {
    if (!dirPath || !dirPath[0]) return;

    wchar_t tmp[4096];
    wcsncpy(tmp, dirPath, ARRAYSIZE(tmp));
    tmp[ARRAYSIZE(tmp)-1] = 0;
    normalize_slashes(tmp);

    // Skip UNC prefix handling and just attempt progressive creation.
    for (wchar_t *p = tmp; *p; ++p) {
        if (*p == L'\\') {
            wchar_t c = *p;
            *p = 0;
            if (wcslen(tmp) > 0) {
                // Avoid creating "C:" as folder
                if (!(wcslen(tmp) == 2 && tmp[1] == L':')) {
                    CreateDirectoryW(tmp, NULL);
                }
            }
            *p = c;
        }
    }
    CreateDirectoryW(tmp, NULL);
}

// Creates parent dirs for a file path (THIS prevents README.md -> folder bug).
static void ensure_parent_dirs_for_file(const wchar_t *filePath) {
    wchar_t tmp[4096];
    wcsncpy(tmp, filePath, ARRAYSIZE(tmp));
    tmp[ARRAYSIZE(tmp)-1] = 0;
    normalize_slashes(tmp);
    if (!PathRemoveFileSpecW(tmp)) return; // no parent
    if (tmp[0]) ensure_dir_recursive(tmp);
}

static uint64_t file_size_u64(const wchar_t *p) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fad)) return 0;
    ULARGE_INTEGER ul;
    ul.HighPart = fad.nFileSizeHigh;
    ul.LowPart  = fad.nFileSizeLow;
    return (uint64_t)ul.QuadPart;
}

static void format_bytes(wchar_t *out, size_t cap, uint64_t bytes) {
    const wchar_t *suffix[] = { L"B", L"KB", L"MB", L"GB" };
    double v = (double)bytes;
    int idx = 0;
    while (v >= 1024.0 && idx < 3) { v /= 1024.0; idx++; }
    _snwprintf(out, cap, (idx==0) ? L"%.0f %s" : L"%.2f %s", v, suffix[idx]);
    out[cap-1] = 0;
}

// ========================= UI =========================

typedef struct {
    HWND wnd;
    HWND title;
    HWND subtitle;
    HWND status;
    HWND bar;
    HWND log;
    HFONT fontTitle;
    HFONT fontBody;
    int installing;
} UI;

static UI g_ui;

static void ui_pump(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void ui_logf(const wchar_t *fmt, ...) {
    wchar_t line[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(line, ARRAYSIZE(line), fmt, ap);
    va_end(ap);
    line[ARRAYSIZE(line)-1] = 0;

    int len = GetWindowTextLengthW(g_ui.log);
    SendMessageW(g_ui.log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_ui.log, EM_REPLACESEL, 0, (LPARAM)line);
    SendMessageW(g_ui.log, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
    ui_pump();
}

static void ui_set_status(const wchar_t *t) {
    SetWindowTextW(g_ui.status, t ? t : L"");
    ui_pump();
}

static void ui_set_pct(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    // stop marquee if active
    SendMessageW(g_ui.bar, PBM_SETMARQUEE, (WPARAM)FALSE, 0);
    SendMessageW(g_ui.bar, PBM_SETPOS, (WPARAM)pct, 0);
    ui_pump();
}

static void ui_set_marquee(int on) {
    LONG_PTR style = GetWindowLongPtrW(g_ui.bar, GWL_STYLE);
    if (on) {
        if (!(style & PBS_MARQUEE)) {
            SetWindowLongPtrW(g_ui.bar, GWL_STYLE, style | PBS_MARQUEE);
        }
        SendMessageW(g_ui.bar, PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30);
    } else {
        SendMessageW(g_ui.bar, PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        if (style & PBS_MARQUEE) {
            SetWindowLongPtrW(g_ui.bar, GWL_STYLE, style & ~PBS_MARQUEE);
        }
    }
    ui_pump();
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CLOSE:
        if (g_ui.installing) {
            MessageBeep(MB_ICONWARNING);
            return 0;
        }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

static HFONT make_font(int height, int weight) {
    return CreateFontW(
        height, 0, 0, 0,
        weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

static void ui_create(HINSTANCE inst) {
    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&ic);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SDLiteSetupWnd_C17";
    RegisterClassW(&wc);

    const int W = 820, H = 500;
    g_ui.wnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"SDLite Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, W, H,
        NULL, NULL, inst, NULL
    );
    if (!g_ui.wnd) fatalf(L"Failed to create window.");

    g_ui.fontTitle = make_font(-22, FW_SEMIBOLD);
    g_ui.fontBody  = make_font(-15, FW_NORMAL);

    g_ui.title = CreateWindowExW(
        0, L"STATIC", L"SDLite Setup",
        WS_CHILD | WS_VISIBLE,
        18, 14, W - 36, 28,
        g_ui.wnd, NULL, inst, NULL
    );
    SendMessageW(g_ui.title, WM_SETFONT, (WPARAM)g_ui.fontTitle, TRUE);

    g_ui.subtitle = CreateWindowExW(
        0, L"STATIC", L"Downloads SDLite + SDL2 + SDL2_image and lays out a ready-to-build folder tree.",
        WS_CHILD | WS_VISIBLE,
        18, 44, W - 36, 20,
        g_ui.wnd, NULL, inst, NULL
    );
    SendMessageW(g_ui.subtitle, WM_SETFONT, (WPARAM)g_ui.fontBody, TRUE);

    g_ui.status = CreateWindowExW(
        0, L"STATIC", L"Starting...",
        WS_CHILD | WS_VISIBLE,
        18, 74, W - 36, 18,
        g_ui.wnd, NULL, inst, NULL
    );
    SendMessageW(g_ui.status, WM_SETFONT, (WPARAM)g_ui.fontBody, TRUE);

    g_ui.bar = CreateWindowExW(
        0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE,
        18, 98, W - 36, 18,
        g_ui.wnd, NULL, inst, NULL
    );
    SendMessageW(g_ui.bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g_ui.bar, PBM_SETPOS, 0, 0);

    g_ui.log = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        18, 128, W - 36, H - 170,
        g_ui.wnd, NULL, inst, NULL
    );
    SendMessageW(g_ui.log, WM_SETFONT, (WPARAM)g_ui.fontBody, TRUE);

    ShowWindow(g_ui.wnd, SW_SHOW);
    UpdateWindow(g_ui.wnd);
    ui_pump();
}

static void ui_destroy_fonts(void) {
    if (g_ui.fontTitle) { DeleteObject(g_ui.fontTitle); g_ui.fontTitle = NULL; }
    if (g_ui.fontBody)  { DeleteObject(g_ui.fontBody);  g_ui.fontBody  = NULL; }
}

// ========================= Folder picker (IFileDialog) =========================

typedef struct IFileDialog IFileDialog;

static int pick_folder_vista(wchar_t *out, size_t cap) {
    // Uses IFileDialog for nicer folder selection on Win10/11
    HRESULT hr;
    IFileDialog *pfd = NULL;
    IShellItem *psi = NULL;

    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileDialog, (void**)&pfd);
    if (FAILED(hr) || !pfd) return 0;

    DWORD opts = 0;
    pfd->lpVtbl->GetOptions(pfd, &opts);
    opts |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    pfd->lpVtbl->SetOptions(pfd, opts);
    pfd->lpVtbl->SetTitle(pfd, L"Choose install location for SDLite");

    hr = pfd->lpVtbl->Show(pfd, g_ui.wnd);
    if (FAILED(hr)) { pfd->lpVtbl->Release(pfd); return 0; }

    hr = pfd->lpVtbl->GetResult(pfd, &psi);
    if (FAILED(hr) || !psi) { pfd->lpVtbl->Release(pfd); return 0; }

    PWSTR psz = NULL;
    hr = psi->lpVtbl->GetDisplayName(psi, SIGDN_FILESYSPATH, &psz);
    if (SUCCEEDED(hr) && psz) {
        wcsncpy(out, psz, cap);
        out[cap-1] = 0;
        CoTaskMemFree(psz);
        psi->lpVtbl->Release(psi);
        pfd->lpVtbl->Release(pfd);
        return 1;
    }

    if (psz) CoTaskMemFree(psz);
    psi->lpVtbl->Release(psi);
    pfd->lpVtbl->Release(pfd);
    return 0;
}

static int pick_folder_legacy(wchar_t *out, size_t cap) {
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_ui.wnd;
    bi.lpszTitle = L"Choose install location for SDLite";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return 0;

    BOOL ok = SHGetPathFromIDListW(pidl, out);
    CoTaskMemFree(pidl);
    if (!ok) return 0;

    out[cap-1] = 0;
    return 1;
}

static int pick_install_folder(wchar_t *out, size_t cap) {
    if (pick_folder_vista(out, cap)) return 1;
    return pick_folder_legacy(out, cap);
}

// ========================= WinHTTP download =========================

typedef struct {
    wchar_t host[512];
    wchar_t path[3072];
    INTERNET_PORT port;
    int isHttps;
} ParsedUrl;

static void parse_url_or_die(const wchar_t *url, ParsedUrl *pu) {
    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);

    wchar_t hostBuf[512];
    wchar_t pathBuf[3072];
    hostBuf[0] = 0;
    pathBuf[0] = 0;

    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = ARRAYSIZE(hostBuf);
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = ARRAYSIZE(pathBuf);

    if (!WinHttpCrackUrl(url, 0, 0, &uc)) {
        fatalf(L"Invalid URL:\n%s", url);
    }

    wcsncpy(pu->host, hostBuf, ARRAYSIZE(pu->host));
    pu->host[ARRAYSIZE(pu->host)-1] = 0;

    wcsncpy(pu->path, pathBuf, ARRAYSIZE(pu->path));
    pu->path[ARRAYSIZE(pu->path)-1] = 0;

    pu->port = uc.nPort;
    pu->isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
}

static void set_winhttp_timeouts(HINTERNET h) {
    int t = 30000; // 30s each
    WinHttpSetTimeouts(h, t, t, t, t);
}

static int winhttp_query_status(HINTERNET hReq, DWORD *outStatus) {
    DWORD status = 0, sz = sizeof(status);
    if (!WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            NULL, &status, &sz, NULL)) {
        return 0;
    }
    *outStatus = status;
    return 1;
}

static int winhttp_query_location(HINTERNET hReq, wchar_t *out, DWORD capWchars) {
    DWORD szBytes = capWchars * (DWORD)sizeof(wchar_t);
    out[0] = 0;
    if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_LOCATION, NULL, out, &szBytes, NULL)) {
        out[(capWchars > 0) ? (capWchars - 1) : 0] = 0;
        return 1;
    }
    return 0;
}

static uint64_t winhttp_query_content_length(HINTERNET hReq, int *hasLen) {
    wchar_t buf[64];
    DWORD sz = sizeof(buf);
    buf[0] = 0;
    if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH, NULL, buf, &sz, NULL)) {
        *hasLen = 1;
        return (uint64_t)_wtoi64(buf);
    }
    *hasLen = 0;
    return 0;
}

static void download_with_progress(const wchar_t *url0, const wchar_t *dstPath, const wchar_t *label) {
    ui_set_status(label);
    ui_set_pct(0);
    ui_logf(L"Downloading: %s", url0);

    ensure_parent_dirs_for_file(dstPath);

    // Create/overwrite destination file early (so failures are obvious)
    HANDLE outFile = CreateFileW(dstPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (outFile == INVALID_HANDLE_VALUE) {
        fatalf(L"Failed to create file:\n%s", dstPath);
    }

    HINTERNET hSession = WinHttpOpen(L"SDLiteSetup/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!hSession) {
        CloseHandle(outFile);
        fatalf(L"WinHttpOpen failed.");
    }
    set_winhttp_timeouts(hSession);

    wchar_t url[4096];
    wcsncpy(url, url0, ARRAYSIZE(url));
    url[ARRAYSIZE(url)-1] = 0;

    uint64_t totalGot = 0;
    int usedMarquee = 0;

    for (int hop = 0; hop < 6; ++hop) {
        ParsedUrl pu;
        parse_url_or_die(url, &pu);

        HINTERNET hConnect = WinHttpConnect(hSession, pu.host, pu.port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            CloseHandle(outFile);
            fatalf(L"WinHttpConnect failed.");
        }

        DWORD flags = WINHTTP_FLAG_REFRESH;
        if (pu.isHttps) flags |= WINHTTP_FLAG_SECURE;

        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", pu.path,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
        if (!hReq) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            CloseHandle(outFile);
            fatalf(L"WinHttpOpenRequest failed.");
        }

        // Some servers (rarely) reject unknown UA or need redirects; also add Accept.
        WinHttpAddRequestHeaders(hReq,
            L"Accept: */*\r\n",
            (DWORD)-1L,
            WINHTTP_ADDREQ_FLAG_ADD);

        BOOL ok = WinHttpSendRequest(hReq,
                                     WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0,
                                     0, 0);
        if (!ok || !WinHttpReceiveResponse(hReq, NULL)) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            CloseHandle(outFile);
            fatalf(L"Download failed while requesting:\n%s", url);
        }

        DWORD status = 0;
        if (!winhttp_query_status(hReq, &status)) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            CloseHandle(outFile);
            fatalf(L"Failed to read HTTP status for:\n%s", url);
        }

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            wchar_t loc[4096];
            if (!winhttp_query_location(hReq, loc, ARRAYSIZE(loc)) || !loc[0]) {
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                CloseHandle(outFile);
                fatalf(L"HTTP redirect without Location header:\n%s", url);
            }
            ui_logf(L"Redirect (%lu) -> %s", status, loc);

            // Restart download at new URL: reset file to empty.
            SetFilePointer(outFile, 0, NULL, FILE_BEGIN);
            SetEndOfFile(outFile);
            totalGot = 0;
            if (usedMarquee) { ui_set_marquee(0); usedMarquee = 0; }
            ui_set_pct(0);

            wcsncpy(url, loc, ARRAYSIZE(url));
            url[ARRAYSIZE(url)-1] = 0;

            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            ui_pump();
            continue;
        }

        if (status != 200) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            CloseHandle(outFile);
            fatalf(L"HTTP %lu while downloading:\n%s", status, url);
        }

        int hasLen = 0;
        uint64_t totalLen = winhttp_query_content_length(hReq, &hasLen);
        if (!hasLen || totalLen == 0) {
            ui_set_marquee(1);
            usedMarquee = 1;
        } else {
            ui_set_marquee(0);
            usedMarquee = 0;
        }

        // Download loop
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hReq, &avail)) {
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                CloseHandle(outFile);
                fatalf(L"WinHttpQueryDataAvailable failed.");
            }
            if (avail == 0) break;

            BYTE buf[64 * 1024];
            DWORD toRead = avail;
            if (toRead > (DWORD)sizeof(buf)) toRead = (DWORD)sizeof(buf);

            DWORD read = 0;
            if (!WinHttpReadData(hReq, buf, toRead, &read)) {
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                CloseHandle(outFile);
                fatalf(L"WinHttpReadData failed.");
            }
            if (read == 0) break;

            DWORD wrote = 0;
            if (!WriteFile(outFile, buf, read, &wrote, NULL) || wrote != read) {
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                CloseHandle(outFile);
                fatalf(L"WriteFile failed for:\n%s", dstPath);
            }

            totalGot += read;
            if (hasLen && totalLen > 0) {
                int pct = (int)((totalGot * 100ULL) / totalLen);
                ui_set_pct(pct);
            } else {
                ui_pump();
            }
        }

        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
        break; // success
    }

    FlushFileBuffers(outFile);
    CloseHandle(outFile);
    WinHttpCloseHandle(hSession);

    ui_set_marquee(0);
    ui_set_pct(100);

    uint64_t sz = file_size_u64(dstPath);
    wchar_t szStr[64];
    format_bytes(szStr, ARRAYSIZE(szStr), sz);
    ui_logf(L"Saved: %s (%s)", dstPath, szStr);

    // Never fail just because it's small. Only warn if extremely tiny.
    if (sz < 1024) {
        ui_logf(L"Warning: download is very small (%llu bytes). Continuing anyway.", (unsigned long long)sz);
    }
}

// ========================= Shell ZIP extraction =========================
//
// Uses IDispatch late-binding, no C++ headers.

static HRESULT disp_get_id(IDispatch *disp, const wchar_t *name, DISPID *out) {
    OLECHAR *n = (OLECHAR*)name;
    return disp->lpVtbl->GetIDsOfNames(disp, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, out);
}

static HRESULT disp_call1(IDispatch *disp, DISPID id, VARIANTARG *arg, VARIANT *ret) {
    DISPPARAMS dp;
    ZeroMemory(&dp, sizeof(dp));
    dp.cArgs = 1;
    dp.rgvarg = arg;
    return disp->lpVtbl->Invoke(disp, id, &IID_NULL, LOCALE_USER_DEFAULT,
                                DISPATCH_METHOD, &dp, ret, NULL, NULL);
}

static HRESULT shell_namespace(IDispatch *shellApp, DISPID idNameSpace, const wchar_t *path, IDispatch **outFolder) {
    if (!outFolder) return E_INVALIDARG;
    *outFolder = NULL;

    VARIANTARG v;
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(path);
    if (!v.bstrVal) return E_OUTOFMEMORY;

    VARIANT ret;
    VariantInit(&ret);

    HRESULT hr = disp_call1(shellApp, idNameSpace, &v, &ret);

    VariantClear(&v);
    if (FAILED(hr)) { VariantClear(&ret); return hr; }
    if (ret.vt != VT_DISPATCH || !ret.pdispVal) { VariantClear(&ret); return E_FAIL; }

    *outFolder = ret.pdispVal; // caller owns
    return S_OK;
}

static HRESULT shell_folder_items(IDispatch *folder, IDispatch **outItems) {
    if (!outItems) return E_INVALIDARG;
    *outItems = NULL;

    DISPID idItems = 0;
    HRESULT hr = disp_get_id(folder, L"Items", &idItems);
    if (FAILED(hr)) return hr;

    DISPPARAMS dp;
    ZeroMemory(&dp, sizeof(dp));

    VARIANT ret;
    VariantInit(&ret);

    hr = folder->lpVtbl->Invoke(folder, idItems, &IID_NULL, LOCALE_USER_DEFAULT,
                                DISPATCH_METHOD, &dp, &ret, NULL, NULL);
    if (FAILED(hr)) { VariantClear(&ret); return hr; }
    if (ret.vt != VT_DISPATCH || !ret.pdispVal) { VariantClear(&ret); return E_FAIL; }

    *outItems = ret.pdispVal;
    return S_OK;
}

static long shell_items_count(IDispatch *items) {
    DISPID idCount = 0;
    if (FAILED(disp_get_id(items, L"Count", &idCount))) return -1;

    DISPPARAMS dp;
    ZeroMemory(&dp, sizeof(dp));

    VARIANT ret;
    VariantInit(&ret);
    HRESULT hr = items->lpVtbl->Invoke(items, idCount, &IID_NULL, LOCALE_USER_DEFAULT,
                                       DISPATCH_PROPERTYGET, &dp, &ret, NULL, NULL);
    if (FAILED(hr)) { VariantClear(&ret); return -1; }

    long c = -1;
    if (ret.vt == VT_I4) c = ret.lVal;
    else if (ret.vt == VT_I2) c = ret.iVal;
    VariantClear(&ret);
    return c;
}

static HRESULT shell_copyhere(IDispatch *dstFolder, IDispatch *items, long flags) {
    DISPID idCopyHere = 0;
    HRESULT hr = disp_get_id(dstFolder, L"CopyHere", &idCopyHere);
    if (FAILED(hr)) return hr;

    VARIANTARG args[2];
    VariantInit(&args[0]);
    VariantInit(&args[1]);

    args[1].vt = VT_DISPATCH;
    args[1].pdispVal = items; // borrowed
    args[0].vt = VT_I4;
    args[0].lVal = flags;

    DISPPARAMS dp;
    ZeroMemory(&dp, sizeof(dp));
    dp.cArgs = 2;
    dp.rgvarg = args;

    VARIANT ret;
    VariantInit(&ret);
    hr = dstFolder->lpVtbl->Invoke(dstFolder, idCopyHere, &IID_NULL, LOCALE_USER_DEFAULT,
                                   DISPATCH_METHOD, &dp, &ret, NULL, NULL);
    VariantClear(&ret);
    return hr;
}

// Recursively count entries and total bytes, for "stability" waiting.
typedef struct {
    uint64_t files;
    uint64_t dirs;
    uint64_t bytes;
} DirStats;

static void dir_stats_recurse(const wchar_t *dir, DirStats *st) {
    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", dir);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        wchar_t p[4096];
        _snwprintf(p, ARRAYSIZE(p), L"%s\\%s", dir, fd.cFileName);
        p[ARRAYSIZE(p)-1] = 0;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            st->dirs++;
            dir_stats_recurse(p, st);
        } else {
            st->files++;
            ULARGE_INTEGER ul;
            ul.HighPart = fd.nFileSizeHigh;
            ul.LowPart  = fd.nFileSizeLow;
            st->bytes += (uint64_t)ul.QuadPart;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void wait_dir_stable_nonempty(const wchar_t *dir, int timeoutSeconds, const wchar_t *what) {
    ui_logf(L"Waiting for extraction to finish: %s", what);

    DWORD start = GetTickCount();
    DirStats prev;
    ZeroMemory(&prev, sizeof(prev));
    int stableTicks = 0;

    for (;;) {
        DirStats cur;
        ZeroMemory(&cur, sizeof(cur));
        dir_stats_recurse(dir, &cur);

        // "Nonempty" means at least 1 file or dir.
        int nonempty = (cur.files + cur.dirs) > 0;

        // "Stable" means counts and bytes stop changing.
        int stable = (cur.files == prev.files) && (cur.dirs == prev.dirs) && (cur.bytes == prev.bytes);

        prev = cur;

        if (nonempty && stable) stableTicks++;
        else stableTicks = 0;

        if (stableTicks >= 6) { // ~6 * 200ms = 1.2s stable
            return;
        }

        ui_pump();
        Sleep(200);

        DWORD now = GetTickCount();
        if ((int)((now - start) / 1000) > timeoutSeconds) {
            fatalf(L"Timed out waiting for extraction.\n\nWhat: %s\nFolder: %s", what, dir);
        }
    }
}

static HRESULT shell_extract_zip(const wchar_t *zipPath, const wchar_t *destDir) {
    ensure_dir_recursive(destDir);

    CLSID clsidShell;
    HRESULT hr = CLSIDFromProgID(L"Shell.Application", &clsidShell);
    if (FAILED(hr)) return hr;

    IDispatch *shellApp = NULL;
    hr = CoCreateInstance(&clsidShell, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void**)&shellApp);
    if (FAILED(hr) || !shellApp) return hr;

    DISPID idNameSpace = 0;
    hr = disp_get_id(shellApp, L"NameSpace", &idNameSpace);
    if (FAILED(hr)) { shellApp->lpVtbl->Release(shellApp); return hr; }

    IDispatch *zipFolder = NULL;
    IDispatch *dstFolder = NULL;

    hr = shell_namespace(shellApp, idNameSpace, zipPath, &zipFolder);
    if (FAILED(hr) || !zipFolder) { shellApp->lpVtbl->Release(shellApp); return hr; }

    hr = shell_namespace(shellApp, idNameSpace, destDir, &dstFolder);
    if (FAILED(hr) || !dstFolder) {
        zipFolder->lpVtbl->Release(zipFolder);
        shellApp->lpVtbl->Release(shellApp);
        return hr;
    }

    IDispatch *items = NULL;
    hr = shell_folder_items(zipFolder, &items);
    if (FAILED(hr) || !items) {
        dstFolder->lpVtbl->Release(dstFolder);
        zipFolder->lpVtbl->Release(zipFolder);
        shellApp->lpVtbl->Release(shellApp);
        return hr;
    }

    // flags: No progress UI (0x4), No confirmation (0x10), No error UI (0x400), No overwrite prompts (0x200)
    // Keep it quiet:
    long flags = 0x4 | 0x10 | 0x200 | 0x400;

    hr = shell_copyhere(dstFolder, items, flags);

    items->lpVtbl->Release(items);
    dstFolder->lpVtbl->Release(dstFolder);
    zipFolder->lpVtbl->Release(zipFolder);
    shellApp->lpVtbl->Release(shellApp);

    return hr;
}

// ========================= Safe moving / flattening =========================

// Move a single file src->dst, making parent dirs only.
static void move_file_safe(const wchar_t *src, const wchar_t *dst) {
    ensure_parent_dirs_for_file(dst);

    // Overwrite if exists:
    DeleteFileW(dst);

    if (!MoveFileExW(src, dst, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // Fallback copy+delete
        if (!CopyFileW(src, dst, FALSE)) {
            fatalf(L"Failed to copy file:\n%s\n->\n%s", src, dst);
        }
        DeleteFileW(src);
    }
}

// Recursively move tree (dir or file) into dst path.
static void move_tree_safe(const wchar_t *src, const wchar_t *dst) {
    DWORD attr = GetFileAttributesW(src);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        move_file_safe(src, dst);
        return;
    }

    ensure_dir_recursive(dst);

    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", src);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        wchar_t s[4096], d[4096];
        _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", src, fd.cFileName);
        _snwprintf(d, ARRAYSIZE(d), L"%s\\%s", dst, fd.cFileName);
        s[ARRAYSIZE(s)-1] = 0; d[ARRAYSIZE(d)-1] = 0;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            move_tree_safe(s, d);
            RemoveDirectoryW(s);
        } else {
            move_file_safe(s, d);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// Count immediate children: dirs and files; also returns the single dir name if dirCount==1.
static void count_children_one_level(const wchar_t *dir, int *outDirs, int *outFiles, wchar_t *singleDirName, size_t singleCap) {
    if (outDirs) *outDirs = 0;
    if (outFiles) *outFiles = 0;
    if (singleDirName && singleCap) singleDirName[0] = 0;

    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", dir);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    int dirs = 0, files = 0;
    wchar_t only[260];
    only[0] = 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dirs++;
            wcsncpy(only, fd.cFileName, ARRAYSIZE(only));
            only[ARRAYSIZE(only)-1] = 0;
        } else {
            files++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    if (outDirs) *outDirs = dirs;
    if (outFiles) *outFiles = files;
    if (singleDirName && singleCap && dirs == 1) {
        wcsncpy(singleDirName, only, singleCap);
        singleDirName[singleCap-1] = 0;
    }
}

static int looks_like_project_root(const wchar_t *dir) {
    int hits = 0;
    for (int i = 0; i < (int)ARRAYSIZE(ROOT_MARKERS); ++i) {
        wchar_t p[4096];
        _snwprintf(p, ARRAYSIZE(p), L"%s\\%s", dir, ROOT_MARKERS[i]);
        p[ARRAYSIZE(p)-1] = 0;
        if (is_dir(p)) hits++;
    }
    return hits >= 2;
}

// Search for a likely project root within a small depth from start.
static int find_project_root_near(const wchar_t *startDir, wchar_t *outDir, size_t cap) {
    // Strategy:
    // 1) unwrap single-folder chains (even if the wrapper has no files)
    // 2) then try direct, then 1-level, then 2-level directories.
    wchar_t cur[4096];
    wcsncpy(cur, startDir, ARRAYSIZE(cur));
    cur[ARRAYSIZE(cur)-1] = 0;

    for (int depth = 0; depth < 10; ++depth) {
        if (looks_like_project_root(cur)) {
            wcsncpy(outDir, cur, cap);
            outDir[cap-1] = 0;
            return 1;
        }

        int d = 0, f = 0;
        wchar_t only[260];
        count_children_one_level(cur, &d, &f, only, ARRAYSIZE(only));

        // Only unwrap if it's truly a wrapper: exactly one dir and no files
        if (d == 1 && f == 0 && only[0]) {
            wchar_t next[4096];
            _snwprintf(next, ARRAYSIZE(next), L"%s\\%s", cur, only);
            next[ARRAYSIZE(next)-1] = 0;
            wcsncpy(cur, next, ARRAYSIZE(cur));
            cur[ARRAYSIZE(cur)-1] = 0;
            continue;
        }
        break;
    }

    // Direct check again
    if (looks_like_project_root(cur)) {
        wcsncpy(outDir, cur, cap); outDir[cap-1] = 0;
        return 1;
    }

    // One-level search
    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", cur);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    wchar_t candidates[32][4096];
    int candCount = 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

        wchar_t p[4096];
        _snwprintf(p, ARRAYSIZE(p), L"%s\\%s", cur, fd.cFileName);
        p[ARRAYSIZE(p)-1] = 0;

        if (looks_like_project_root(p)) {
            FindClose(h);
            wcsncpy(outDir, p, cap);
            outDir[cap-1] = 0;
            return 1;
        }

        if (candCount < (int)ARRAYSIZE(candidates)) {
            wcsncpy(candidates[candCount], p, ARRAYSIZE(candidates[candCount]));
            candidates[candCount][ARRAYSIZE(candidates[candCount])-1] = 0;
            candCount++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    // Two-level search (best-effort, limited)
    for (int i = 0; i < candCount; ++i) {
        wchar_t pat2[4096];
        _snwprintf(pat2, ARRAYSIZE(pat2), L"%s\\*", candidates[i]);
        pat2[ARRAYSIZE(pat2)-1] = 0;

        HANDLE h2 = FindFirstFileW(pat2, &fd);
        if (h2 == INVALID_HANDLE_VALUE) continue;

        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

            wchar_t p[4096];
            _snwprintf(p, ARRAYSIZE(p), L"%s\\%s", candidates[i], fd.cFileName);
            p[ARRAYSIZE(p)-1] = 0;

            if (looks_like_project_root(p)) {
                FindClose(h2);
                wcsncpy(outDir, p, cap);
                outDir[cap-1] = 0;
                return 1;
            }
        } while (FindNextFileW(h2, &fd));
        FindClose(h2);
    }

    return 0;
}

// Flatten a directory if it contains exactly one directory and no files: move inner/* up into dir.
static int flatten_single_dir_wrapper(const wchar_t *dir) {
    int d = 0, f = 0;
    wchar_t only[260];
    count_children_one_level(dir, &d, &f, only, ARRAYSIZE(only));
    if (!(d == 1 && f == 0 && only[0])) return 0;

    wchar_t inner[4096];
    _snwprintf(inner, ARRAYSIZE(inner), L"%s\\%s", dir, only);
    inner[ARRAYSIZE(inner)-1] = 0;

    // Move contents of inner into dir
    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", inner);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        wchar_t s[4096], d2[4096];
        _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", inner, fd.cFileName);
        _snwprintf(d2, ARRAYSIZE(d2), L"%s\\%s", dir, fd.cFileName);
        s[ARRAYSIZE(s)-1] = 0; d2[ARRAYSIZE(d2)-1] = 0;

        move_tree_safe(s, d2);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    RemoveDirectoryW(inner);
    return 1;
}

// ========================= Cleanup (recursive delete) =========================

static void delete_tree(const wchar_t *path) {
    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path);
        return;
    }

    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", path);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            wchar_t p[4096];
            _snwprintf(p, ARRAYSIZE(p), L"%s\\%s", path, fd.cFileName);
            p[ARRAYSIZE(p)-1] = 0;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                delete_tree(p);
                RemoveDirectoryW(p);
            } else {
                SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(p);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(path);
}

// ========================= Install layout helpers =========================

static void ensure_install_structure(const wchar_t *installDir) {
    wchar_t p[4096];

    path_join(p, ARRAYSIZE(p), installDir, L"include"); ensure_dir_recursive(p);
    path_join(p, ARRAYSIZE(p), installDir, L"src");     ensure_dir_recursive(p);
    path_join(p, ARRAYSIZE(p), installDir, L"res");     ensure_dir_recursive(p);

    path_join(p, ARRAYSIZE(p), installDir, L"external\\SDL2");        ensure_dir_recursive(p);
    path_join(p, ARRAYSIZE(p), installDir, L"external\\SDL2_image");  ensure_dir_recursive(p);

    path_join(p, ARRAYSIZE(p), installDir, L"bin\\debug");   ensure_dir_recursive(p);
    path_join(p, ARRAYSIZE(p), installDir, L"bin\\release"); ensure_dir_recursive(p);
}

static void validate_and_log(const wchar_t *installDir) {
    ui_set_status(L"Validating install...");
    ui_set_pct(100);

    wchar_t p[4096];

    path_join(p, ARRAYSIZE(p), installDir, L"include");
    ui_logf(is_dir(p) ? L"OK: include/" : L"WARNING: include/ missing");

    path_join(p, ARRAYSIZE(p), installDir, L"src");
    ui_logf(is_dir(p) ? L"OK: src/" : L"WARNING: src/ missing");

    path_join(p, ARRAYSIZE(p), installDir, L"res");
    ui_logf(is_dir(p) ? L"OK: res/" : L"WARNING: res/ missing");

    path_join(p, ARRAYSIZE(p), installDir, SDL2_MARKER_REL);
    ui_logf(is_file(p) ? L"OK: SDL2 headers detected" : L"WARNING: SDL2 headers missing");

    path_join(p, ARRAYSIZE(p), installDir, SDL2_IMAGE_MARKER_REL);
    ui_logf(is_file(p) ? L"OK: SDL2_image headers detected" : L"WARNING: SDL2_image headers missing");
}

// ========================= Main install steps =========================

static void flatten_sdl_toolchain_folder(const wchar_t *rootDir) {
    wchar_t toolchain[4096];
    _snwprintf(toolchain, ARRAYSIZE(toolchain), L"%s\\x86_64-w64-mingw32", rootDir);
    toolchain[ARRAYSIZE(toolchain)-1] = 0;

    if (!is_dir(toolchain)) return;

    ui_logf(L"Detected SDL MinGW toolchain folder, flattening...");

    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", toolchain);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        wchar_t s[4096], d[4096];
        _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", toolchain, fd.cFileName);
        _snwprintf(d, ARRAYSIZE(d), L"%s\\%s", rootDir, fd.cFileName);

        move_tree_safe(s, d);
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    RemoveDirectoryW(toolchain);
}

static void install_run(void) {
    g_ui.installing = 1;

    // Basic URL placeholder check (optional)
    if (!REPO_ZIP_URL || !SDL2_ZIP_URL || !SDL2_IMAGE_ZIP_URL) {
        fatalf(L"Missing download URLs in CONFIG.");
    }

    wchar_t chosen[4096];
    ui_set_status(L"Choose an install folder...");
    ui_logf(L"Select where to create the SDLite folder.");

    if (!pick_install_folder(chosen, ARRAYSIZE(chosen))) {
        ui_logf(L"Cancelled by user.");
        g_ui.installing = 0;
        return;
    }
    chosen[ARRAYSIZE(chosen)-1] = 0;

    wchar_t installDir[4096];
    _snwprintf(installDir, ARRAYSIZE(installDir), L"%s\\%s", chosen, INSTALL_SUBFOLDER);
    installDir[ARRAYSIZE(installDir)-1] = 0;

    ensure_dir_recursive(installDir);
    ui_logf(L"Install directory: %s", installDir);

    // Temp folders inside installDir (and cleaned at end)
    wchar_t dlDir[4096], tmpRepo[4096], tmpSDL[4096], tmpIMG[4096];
    path_join(dlDir,  ARRAYSIZE(dlDir),  installDir, L".downloads");
    path_join(tmpRepo,ARRAYSIZE(tmpRepo),installDir, L".tmp_repo");
    path_join(tmpSDL, ARRAYSIZE(tmpSDL), installDir, L".tmp_sdl2");
    path_join(tmpIMG, ARRAYSIZE(tmpIMG), installDir, L".tmp_sdl2_image");

    ensure_dir_recursive(dlDir);
    ensure_dir_recursive(tmpRepo);
    ensure_dir_recursive(tmpSDL);
    ensure_dir_recursive(tmpIMG);

    // Download target zip paths
    wchar_t repoZip[4096], sdlZip[4096], imgZip[4096];
    path_join(repoZip, ARRAYSIZE(repoZip), dlDir, L"repo.zip");
    path_join(sdlZip,  ARRAYSIZE(sdlZip),  dlDir, L"sdl2.zip");
    path_join(imgZip,  ARRAYSIZE(imgZip),  dlDir, L"sdl2_image.zip");

    // ---- Download
    ui_set_status(L"Downloading files...");
    ui_set_pct(0);

    download_with_progress(REPO_ZIP_URL, repoZip, L"Downloading SDLite (repo)...");
    ui_set_pct(10);

    download_with_progress(SDL2_ZIP_URL, sdlZip, L"Downloading SDL2...");
    ui_set_pct(20);

    download_with_progress(SDL2_IMAGE_ZIP_URL, imgZip, L"Downloading SDL2_image...");
    ui_set_pct(30);

    // ---- Extract repo
    ui_set_status(L"Extracting SDLite repo...");
    ui_set_pct(32);
    ui_logf(L"Extracting repo ZIP -> %s", tmpRepo);

    HRESULT hr = shell_extract_zip(repoZip, tmpRepo);
    if (FAILED(hr)) {
        fatalf(L"Repo extraction failed (HRESULT 0x%08lx).", (unsigned long)hr);
    }
    wait_dir_stable_nonempty(tmpRepo, 60, L"SDLite repo");
    ui_set_pct(40);

    // Find project root inside extracted area
    wchar_t repoRoot[4096];
    if (!find_project_root_near(tmpRepo, repoRoot, ARRAYSIZE(repoRoot))) {
        ui_logf(L"WARNING: Could not confidently detect repo root by include/src/res markers.");
        // fallback: unwrap as much as possible and use that
        wcsncpy(repoRoot, tmpRepo, ARRAYSIZE(repoRoot));
        repoRoot[ARRAYSIZE(repoRoot)-1] = 0;
        for (int i = 0; i < 6; ++i) {
            if (!flatten_single_dir_wrapper(repoRoot)) break;
        }
    }
    ui_logf(L"Repo root selected: %s", repoRoot);

    // ---- Move repo contents into installDir (skip our temp folders)
    ui_set_status(L"Applying project layout...");
    ui_set_pct(45);

    wchar_t pat[4096];
    _snwprintf(pat, ARRAYSIZE(pat), L"%s\\*", repoRoot);
    pat[ARRAYSIZE(pat)-1] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        fatalf(L"Repo root appears empty:\n%s", repoRoot);
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        // Avoid copying installer temp dirs if they somehow exist in repo
        if (wcscmp(fd.cFileName, L".downloads") == 0) continue;
        if (wcscmp(fd.cFileName, L".tmp_repo") == 0) continue;
        if (wcscmp(fd.cFileName, L".tmp_sdl2") == 0) continue;
        if (wcscmp(fd.cFileName, L".tmp_sdl2_image") == 0) continue;

        wchar_t s[4096], d[4096];
        _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", repoRoot, fd.cFileName);
        _snwprintf(d, ARRAYSIZE(d), L"%s\\%s", installDir, fd.cFileName);
        s[ARRAYSIZE(s)-1] = 0; d[ARRAYSIZE(d)-1] = 0;

        move_tree_safe(s, d);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    ui_logf(L"Repo files copied into install directory.");
    ui_set_pct(55);

    // Ensure required structure exists (creates missing dirs but does not touch files)
    ensure_install_structure(installDir);

    // ---- Extract SDL2 into temp, flatten, then move into external\SDL2
    ui_set_status(L"Extracting SDL2...");
    ui_set_pct(58);
    ui_logf(L"Extracting SDL2 ZIP -> %s", tmpSDL);

    hr = shell_extract_zip(sdlZip, tmpSDL);
    if (FAILED(hr)) fatalf(L"SDL2 extraction failed (HRESULT 0x%08lx).", (unsigned long)hr);
    wait_dir_stable_nonempty(tmpSDL, 60, L"SDL2");

    // Flatten wrappers until marker exists under a candidate root that we will move
    // We want final: installDir\external\SDL2\include\SDL2\SDL.h
    // So tmpSDL after flatten should contain include\SDL2\SDL.h at its root.
    ui_logf(L"Flattening SDL2 wrapper folders (if needed)...");
    for (int i = 0; i < 12; ++i) {
        wchar_t marker[4096];
        _snwprintf(marker, ARRAYSIZE(marker), L"%s\\include\\SDL2\\SDL.h", tmpSDL);
        marker[ARRAYSIZE(marker)-1] = 0;
        if (is_file(marker)) break;
        if (!flatten_single_dir_wrapper(tmpSDL)) break;
        ui_pump();
    }
    
    flatten_sdl_toolchain_folder(tmpSDL);


    // Move into external\SDL2 (replace existing)
    {
        wchar_t dstSDL[4096];
        path_join(dstSDL, ARRAYSIZE(dstSDL), installDir, L"external\\SDL2");

        // Clean destination then move contents in
        delete_tree(dstSDL);
        ensure_dir_recursive(dstSDL);

        ui_logf(L"Placing SDL2 into: %s", dstSDL);

        wchar_t pat2[4096];
        _snwprintf(pat2, ARRAYSIZE(pat2), L"%s\\*", tmpSDL);
        pat2[ARRAYSIZE(pat2)-1] = 0;

        HANDLE h2 = FindFirstFileW(pat2, &fd);
        if (h2 != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                wchar_t s[4096], d[4096];
                _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", tmpSDL, fd.cFileName);
                _snwprintf(d, ARRAYSIZE(d), L"%s\\%s", dstSDL, fd.cFileName);
                s[ARRAYSIZE(s)-1] = 0; d[ARRAYSIZE(d)-1] = 0;
                move_tree_safe(s, d);
            } while (FindNextFileW(h2, &fd));
            FindClose(h2);
        }
    }
    ui_set_pct(72);

    // ---- Extract SDL2_image into temp, flatten, then move into external\SDL2_image
    ui_set_status(L"Extracting SDL2_image...");
    ui_set_pct(74);
    ui_logf(L"Extracting SDL2_image ZIP -> %s", tmpIMG);

    hr = shell_extract_zip(imgZip, tmpIMG);
    if (FAILED(hr)) fatalf(L"SDL2_image extraction failed (HRESULT 0x%08lx).", (unsigned long)hr);
    wait_dir_stable_nonempty(tmpIMG, 60, L"SDL2_image");

    ui_logf(L"Flattening SDL2_image wrapper folders (if needed)...");
    for (int i = 0; i < 12; ++i) {
        wchar_t marker[4096];
        _snwprintf(marker, ARRAYSIZE(marker), L"%s\\include\\SDL2\\SDL_image.h", tmpIMG);
        marker[ARRAYSIZE(marker)-1] = 0;
        if (is_file(marker)) break;
        if (!flatten_single_dir_wrapper(tmpIMG)) break;
        ui_pump();
    }

    flatten_sdl_toolchain_folder(tmpIMG);

    {
        wchar_t dstIMG[4096];
        path_join(dstIMG, ARRAYSIZE(dstIMG), installDir, L"external\\SDL2_image");

        delete_tree(dstIMG);
        ensure_dir_recursive(dstIMG);

        ui_logf(L"Placing SDL2_image into: %s", dstIMG);

        wchar_t pat2[4096];
        _snwprintf(pat2, ARRAYSIZE(pat2), L"%s\\*", tmpIMG);
        pat2[ARRAYSIZE(pat2)-1] = 0;

        HANDLE h2 = FindFirstFileW(pat2, &fd);
        if (h2 != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                wchar_t s[4096], d[4096];
                _snwprintf(s, ARRAYSIZE(s), L"%s\\%s", tmpIMG, fd.cFileName);
                _snwprintf(d, ARRAYSIZE(d), L"%s\\%s", dstIMG, fd.cFileName);
                s[ARRAYSIZE(s)-1] = 0; d[ARRAYSIZE(d)-1] = 0;
                move_tree_safe(s, d);
            } while (FindNextFileW(h2, &fd));
            FindClose(h2);
        }
    }
    ui_set_pct(90);

    // Ensure final required folders exist (bin/debug, bin/release, etc.)
    ensure_install_structure(installDir);

    // ---- Cleanup temp dirs
    ui_set_status(L"Cleaning up...");
    ui_set_pct(94);

    ui_logf(L"Removing temporary folders...");
    delete_tree(tmpRepo);
    delete_tree(tmpSDL);
    delete_tree(tmpIMG);
    delete_tree(dlDir);

    ui_set_pct(98);

    // ---- Validate
    validate_and_log(installDir);

    ui_set_status(L"Done!");
    ui_set_pct(100);

    g_ui.installing = 0;

    MessageBoxW(
        g_ui.wnd,
        L"SDLite setup completed.\n\n"
        L"Your folder is ready:\n"
        L"- include/\n"
        L"- src/\n"
        L"- res/\n"
        L"- external/SDL2\n"
        L"- external/SDL2_image\n"
        L"- bin/debug and bin/release\n\n"
        L"If you see warnings in the log, double-check the ZIP URLs.",
        L"SDLite Setup",
        MB_OK | MB_ICONINFORMATION
    );
}

// ========================= Entry =========================

int wmain(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        fatalf(L"COM initialization failed.");
    }

    HINSTANCE inst = GetModuleHandleW(NULL);
    ui_create(inst);

    ui_logf(L"SDLite Setup starting...");
    ui_logf(L"Tip: If a download URL changes, paste the new URL into CONFIG and rebuild.");

    // Run install (single-threaded, but we pump messages during operations)
    install_run();

    // Allow closing now (install_run sets this too, but keep it safe)
    g_ui.installing = 0;

    ui_set_status(L"You can close this window.");
    ui_logf(L"Finished.");

    // Standard message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ui_destroy_fonts();
    CoUninitialize();
    return 0;
}
