#define CURL_STATICLIB

#include <curl.h>
#include <thread>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

// Function declarations
static void initializeNotifyIcon(HWND hwnd);
static void removeNotifyIcon();
static void exitAction();
static void createPopupMenu(HWND hwnd);
static void handleTrayIconEvent(HWND hwnd, LPARAM lParam);
static void processKeyCombinations();

// Path to config file
std::string configFilePath = "config.json";

// Global flag to signal the thread to exit
std::atomic<bool> exitFlag(false);

struct Shortcut {
    std::vector<int> keys;
    std::string url;
    std::string method;
    std::map<std::string, std::string> headers;
    bool alertOnError;
};

struct ShortcutConfig {
    std::vector<Shortcut> shortcuts;
};
ShortcutConfig shortcutConfig;

// Global variables
NOTIFYICONDATA notifyIcon = {};

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        exitAction();
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_USER + 1:
        handleTrayIconEvent(hwnd, lParam);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            exitAction();
        }
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Function to check if program already running
static bool isAnotherInstanceRunning() {
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"KeyWebHookMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return true;
	}
	return false;
}

static std::vector<int> parseKeys(const std::string& keys) {
    std::vector<int> parsedKeys;
    std::stringstream ss(keys);
    std::string item;
    while (std::getline(ss, item, '+')) {
        int keyInt;
        std::stringstream keyStream;
        keyStream << std::hex << item;
        keyStream >> keyInt;
        if (keyStream.fail()) {
            MessageBox(NULL, L"Failed to parse key. Please ensure the key is valid.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
            exit(1);
        }
        parsedKeys.push_back(keyInt);
    }
    if (parsedKeys.empty()) {
        MessageBox(NULL, L"No keys were parsed. The input string may be empty or invalid. Please provide at least one valid key.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
        exit(1);
    }
    return parsedKeys;
}

static ShortcutConfig parseConfig(const std::string& configFilePath) {
    std::ifstream configFile(configFilePath);
    if (!configFile) {
        MessageBox(NULL, L"Config file does not exist or is invalid.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
        exit(1);
    }
    json configJson;
    try {
        configFile >> configJson;
    }
    catch (json::parse_error& e) {
        MessageBox(NULL, L"Config file is invalid.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
        exit(1);
    }

    ShortcutConfig shortcutConfig;
    for (const auto& shortcut : configJson["shortcuts"]) {
        if (!shortcut.contains("keys") || !shortcut.contains("url")) {
            MessageBox(NULL, L"Config file is missing 'keys' or 'url'.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
            exit(1);
        }
        Shortcut s;
        s.keys = parseKeys(shortcut["keys"]);
        s.url = shortcut["url"];
        s.method = shortcut.value("method", "POST");
        if (shortcut.contains("headers")) {
            for (const auto& header : shortcut["headers"].items()) {
                s.headers[header.key()] = header.value();
            }
        }
        else {
            s.headers = {}; // Set to empty if headers are not set
        }
        s.alertOnError = shortcut.value("alertOnError", true);
        shortcutConfig.shortcuts.push_back(s);
    }

    return shortcutConfig;
}

// Entry point
static int CALLBACK wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine,
    int nCmdShow
) {
    if (isAnotherInstanceRunning()) {
        MessageBox(NULL, L"Another instance is already running.", L"KeyWebHook Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    FreeConsole();
    // Create a window class
    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.lpszClassName = L"KeyWebHookClass";
    RegisterClass(&windowClass);

    HWND hwnd = CreateWindowEx(
        0,
        windowClass.lpszClassName,
        L"KeyWebHook",
        0,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Initialize notify icon
    initializeNotifyIcon(hwnd);

    // Init keys 
    shortcutConfig = parseConfig(configFilePath);

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_ALL);

    // Create a separate thread for processing key combinations
    std::thread keyCombinationThread(processKeyCombinations);

    // Register exitAction to be called at program termination
    atexit(exitAction);

    // Message loop for handling other events
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Set the exit flag to signal the thread to exit
    exitFlag = true;

    // Cleanup and join the key combination thread
    keyCombinationThread.join();

    // Cleanup
    curl_global_cleanup();
    removeNotifyIcon();

    return 0;
}

// Function to initialize the notify icon
static void initializeNotifyIcon(HWND hwnd) {
    notifyIcon = {};
    notifyIcon.cbSize = sizeof(NOTIFYICONDATA);
    notifyIcon.hWnd = hwnd;
    notifyIcon.uID = 1;
    notifyIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyIcon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    notifyIcon.uCallbackMessage = WM_USER + 1; // Custom message ID

    // Set the tooltip text
    wcscpy_s(notifyIcon.szTip, L"KeyWebHook");

    Shell_NotifyIcon(NIM_ADD, &notifyIcon);
}

// Function to remove the notify icon
static void removeNotifyIcon() {
    Shell_NotifyIcon(NIM_DELETE, &notifyIcon);
}

// Function to handle the exit action
static void exitAction() {
    curl_global_cleanup();
    removeNotifyIcon();
    PostQuitMessage(0);
}

// Function to handle right-click menu messages
static void createPopupMenu(HWND hwnd) {
    HMENU hPopupMenu = CreatePopupMenu();
    AppendMenu(hPopupMenu, MF_STRING, 1, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hPopupMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hPopupMenu);
}

// Function to handle tray icon events
static void handleTrayIconEvent(HWND hwnd, LPARAM lParam) {
    if (LOWORD(lParam) == WM_RBUTTONUP) {
        createPopupMenu(hwnd);
    }
}

void performShortcutAction(const Shortcut& shortcut) {
    std::string url = shortcut.url;
    std::string method = shortcut.method;
    bool alertOnError = shortcut.alertOnError;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_slist* headers = NULL;
        for (const auto& header : shortcut.headers) {
            std::string headerString = header.first + ": " + header.second;
            headers = curl_slist_append(headers, headerString.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if ((res != CURLE_OK || (http_code != 200 && http_code != 0)) && alertOnError) {
            // print res to message box as human readable
            std::string resString = curl_easy_strerror(res);
            std::wstring resStringW(resString.begin(), resString.end());
            std::wstring errorString = L"";
            std::wstring httpCodeString = L"";

            if (http_code != 200 && http_code != 0) {
                httpCodeString = L"HTTP Error Code: " + std::to_wstring(http_code) + L"\n";
			}

            if (res != CURLE_OK) {
                errorString = L"Error: " + resStringW;
			}

            std::wstring message = httpCodeString + errorString;
            MessageBox(NULL, message.c_str(), L"KeyWebHook Error", MB_OK | MB_ICONERROR);
        }
        curl_easy_cleanup(curl);
    }
}

// Function to process key combinations
static void processKeyCombinations() {
    while (!exitFlag) {
        for (const auto& shortcut : shortcutConfig.shortcuts) {
            bool allKeysPressed = true;
            for (const auto& key : shortcut.keys) {
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    allKeysPressed = false;
                    break;
                }
            }
            if (allKeysPressed) {

                std::string url = shortcut.url;

                std::string method = shortcut.method;

                CURL* curl = curl_easy_init();
                if (curl) {
                    performShortcutAction(shortcut);
                }

                Sleep(1000);
            }
        }
        
        Sleep(10); // Sleep for 10 milliseconds
    }
}