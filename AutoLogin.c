
#define UNICODE
#include <windows.h>
#include <stdio.h>

#define MAX_INFO	(1024)
#define MAX_RETRIES	(60)

#define IDC_PATH	101
#define IDC_LOGIN	102
#define IDC_NAME	103
#define IDC_PASS	104
#define IDC_ADD		105
#define IDC_DEL		106
#define IDC_LIST	107

typedef struct {
	SLIST_ENTRY ItemEntry;
	WCHAR name[MAX_INFO];
	WCHAR pass[MAX_INFO];
} ACCOUNT, *PACCOUNT;

typedef struct {
	WCHAR path[MAX_PATH];
	WCHAR name[MAX_INFO];
	WCHAR pass[MAX_INFO];
	BOOL login;
	DWORD processId;
	HWND hwnd;
} INFO, *PINFO;

HFONT defaultFont;
WNDPROC pListProc;

HWND hwnd, hwndStatic1, hwndStatic2,
	hwndPath, hwndLogin, hwndName, hwndPass,
	hwndAdd, hwndDelete, hwndList;

HWND hwndLastFocus;

PSLIST_HEADER pListHead;
PSLIST_HEADER pTmpHead;


void AL_Key(HANDLE hWnd, WORD vk, BOOL press)
{
	INPUT ip;
	ZeroMemory(&ip, sizeof(INPUT));

	ip.type = INPUT_KEYBOARD;
	ip.ki.wVk = vk;
	ip.ki.dwFlags = (press ? 0 : KEYEVENTF_KEYUP);
	SendInput(1, &ip, sizeof(INPUT));
}

void AL_SendKey(HANDLE hWnd, char key)
{
	WORD vk = VkKeyScan(key);
	BOOL isUpper = IsCharUpper(key);

	if (isUpper) {
		AL_Key(hWnd, VK_LSHIFT, TRUE);
	}

	AL_Key(hWnd, vk, TRUE);
	AL_Key(hWnd, vk, FALSE);

	if (isUpper) {
		AL_Key(hWnd, VK_LSHIFT, FALSE);
	}
}

BOOL SendInfo(PINFO info)
{
	WCHAR *pKey = info->name;
	while (*pKey != L'\0') {
		AL_SendKey(info, *pKey);
		pKey++;
	}

	AL_SendKey(info, L'\t');

	pKey = info->pass;
	while (*pKey != L'\0') {
		AL_SendKey(info, *pKey);
		pKey++;
	}

	if (info->login) {
		AL_SendKey(info, L'\n');
	}

	return TRUE;
}

BOOL CALLBACK Window_Callback(HWND hWnd, LPARAM lParam) {
	PINFO info = (PINFO)lParam;
	if (IsWindowVisible(hWnd)) {
		DWORD testId;
		GetWindowThreadProcessId(hWnd, &testId);
		if (testId == info->processId) {
			info->hwnd = hWnd;
			return FALSE;
		}
	}
	return TRUE;
}

DWORD WINAPI StartGame_Process(LPVOID lpParameter)
{
	PINFO info;
	INT retries;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	info = (PINFO)lpParameter;

	ZeroMemory(&si, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	BOOL bResult = CreateProcessW(info->path,
		info->path,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&si,
		&pi);

	if (!bResult) {
		MessageBoxW(
			hwnd,
			L"Please make sure you have entered the correct path to the WoW executable.",
			L"Failed to start WoW",
			MB_ICONWARNING
		);
		return 1;
	}

	info->processId = pi.dwProcessId;
	info->hwnd = NULL;

	retries = 0;
	while (info->hwnd == NULL && retries < MAX_RETRIES) {
		Sleep(500);
		EnumWindows(Window_Callback, (LPARAM)info);
		++retries;
	}

	if (retries >= MAX_RETRIES) {
		MessageBoxW(hwnd,
			L"WoW was started but the AutoLogin cannot find the game window.",
			L"Failed to start WoW",
			MB_ICONWARNING);
		goto close;
	}

	retries = 0;
	while (SetForegroundWindow(info->hwnd) == FALSE && retries < MAX_RETRIES) {
		Sleep(500);
		++retries;
	}

	if (retries >= MAX_RETRIES) {
		MessageBoxW(hwnd,
			L"Unable to bring the WoW window to the foreground to enter the account information.",
			L"Failed to start WoW",
			MB_ICONWARNING);
		goto close;
	}

	SendInfo(info);

close:
	CloseHandle(pi.hProcess);

	return 0;
}
void ListMoveToTmp(void)
{
	InterlockedFlushSList(pTmpHead);
	for (;;) {
		PSLIST_ENTRY pListEntry = InterlockedPopEntrySList(pListHead);
		if (pListEntry == NULL) {
			return;
		}
		InterlockedPushEntrySList(pTmpHead, &(((PACCOUNT)pListEntry)->ItemEntry));
	}
}
void StartGame_Account(LPWSTR path, BOOL login, PACCOUNT account)
{
	PINFO info = (PINFO)malloc(sizeof(INFO));
	CopyMemory(info->path, path, MAX_PATH);
	CopyMemory(info->name, account->name, MAX_INFO);
	CopyMemory(info->pass, account->pass, MAX_INFO);
	info->login = login;
	CreateThread(NULL, 0, StartGame_Process, info, 0, NULL);
}
void StartGame(LPWSTR path, BOOL login, int accNumber)
{
	int i = 0;
	ListMoveToTmp();
	for (;;) {
		PSLIST_ENTRY pListEntry = InterlockedPopEntrySList(pTmpHead);
		PACCOUNT account = (PACCOUNT)pListEntry;
		if (pListEntry == NULL) {
			return;
		}
		InterlockedPushEntrySList(pListHead, &(account->ItemEntry));
		if (i == accNumber) {
			StartGame_Account(path, login, account);
		}
		++i;
	}
}

PSLIST_HEADER ListInitialize(void)
{
	PSLIST_HEADER pHead = (PSLIST_HEADER)_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
	InitializeSListHead(pHead);
	return pHead;
}
void ListCreateLists(void)
{
	pListHead = ListInitialize();
	pTmpHead = ListInitialize();
}
PACCOUNT ListAddAccount(void)
{
	PACCOUNT account = (PACCOUNT)_aligned_malloc(sizeof(ACCOUNT), MEMORY_ALLOCATION_ALIGNMENT);
	InterlockedPushEntrySList(pListHead, &(account->ItemEntry));
	return account;
}
void ShowAccount(PACCOUNT account)
{
	SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM) account->name);
}
BOOL ListDeleteAccount(accNumber)
{
	int i = 0;
	ListMoveToTmp();
	for (;;) {
		PSLIST_ENTRY pListEntry = InterlockedPopEntrySList(pTmpHead);
		PACCOUNT account = (PACCOUNT)pListEntry;
		if (pListEntry == NULL) {
			return;
		}
		if (i == accNumber) {
			_aligned_free(account);
			SendMessageW(hwndList, LB_DELETESTRING, i, 0);
		} else {
			InterlockedPushEntrySList(pListHead, &(account->ItemEntry));
		}
		++i;
	}
}
void LoadSettings(LPWSTR iniFile)
{
	WCHAR path[MAX_PATH];
	UINT i, numAccounts;
	BOOL login;

	ListCreateLists();

	GetPrivateProfileStringW(L"Settings", L"Path", L"", path, MAX_PATH, iniFile);
	SetWindowTextW(hwndPath, path);

	login = GetPrivateProfileIntW(L"Settings", L"Login", 0, iniFile);
	SendMessageW(hwndLogin, BM_SETCHECK, login, 0);

	numAccounts = GetPrivateProfileIntW(L"Settings", L"NumAccounts", 0, iniFile);

	for (i = 0; i < numAccounts; i++) {
		WCHAR iniString[30];
		PACCOUNT account;

		swprintf_s(iniString, 30, L"A%d", i);
		account = ListAddAccount();
		GetPrivateProfileStringW(L"Accounts", iniString, L"", account->name, MAX_INFO, iniFile);
		GetPrivateProfileStringW(L"Passwords", iniString, L"", account->pass, MAX_INFO, iniFile);
		ShowAccount(account);
	}
}
void SaveSettings(LPWSTR iniFile, LPWSTR iniFileBak)
{
	WCHAR path[MAX_PATH];
	WCHAR tmp[30];
	INT i;
	USHORT numAccounts;
	BOOL login;

	MoveFileExW(iniFile, iniFileBak,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);

	SendMessageW(hwndPath, WM_GETTEXT,
		sizeof(path) / sizeof(path[0]), (LPARAM)path);
	WritePrivateProfileStringW(L"Settings", L"Path", path, iniFile);

	login = IsDlgButtonChecked(hwnd, IDC_LOGIN);
	swprintf_s(tmp, 30, L"%d", login ? 1 : 0);
	WritePrivateProfileStringW(L"Settings", L"Login", tmp, iniFile);

	numAccounts = QueryDepthSList(pListHead) & 0xFFFF;
	swprintf_s(tmp, 30, L"%d", numAccounts);
	WritePrivateProfileStringW(L"Settings", L"NumAccounts", tmp, iniFile);

	for (i = numAccounts - 1; i >= 0; i--)
	{
		WCHAR iniString[30];
		PACCOUNT account;

		swprintf_s(iniString, 30, L"A%d", i);
		account = (PACCOUNT)InterlockedPopEntrySList(pListHead);
		WritePrivateProfileStringW(L"Accounts", iniString, account->name, iniFile);
		WritePrivateProfileStringW(L"Passwords", iniString, account->pass, iniFile);
		_aligned_free(account);
	}
}
void OnAddPressed(void)
{
	PACCOUNT account = ListAddAccount();
	SendMessageW(hwndName, WM_GETTEXT,
		MAX_INFO / sizeof(WCHAR), (LPARAM)account->name);
	SendMessageW(hwndPass, WM_GETTEXT,
		MAX_INFO / sizeof(WCHAR), (LPARAM)account->pass);
	ShowAccount(account);
}
void OnDeletePressed(void)
{
	LRESULT sel, newSel, count;

	sel = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
	if (sel != LB_ERR) {
		ListDeleteAccount(sel);
		count = SendMessageW(hwndList, LB_GETCOUNT, 0, 0);
		if (count > 0) {
			newSel = (sel == count ? sel - 1 : sel);
			SendMessageW(hwndList, LB_SETCURSEL, newSel, 0);
		}
	}
}

LRESULT CALLBACK ListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_KEYDOWN) {
		if (wParam == VK_DELETE || wParam == VK_BACK) {
			OnDeletePressed();
		}
	}
	return CallWindowProcW(pListProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CREATE:

			hwndStatic1 = CreateWindowW(L"static", L"WoW Path:",
				WS_CHILD | WS_VISIBLE,
				0, 0, 100, 20, hwnd, 0, GetModuleHandleW(NULL), NULL);

			hwndPath = CreateWindowW(L"edit", 0,
				WS_CHILD | WS_BORDER | WS_VISIBLE | WS_TABSTOP,
				100, 0, 200, 20, hwnd, (HMENU)IDC_PATH, GetModuleHandleW(NULL), NULL);

			hwndStatic2 = CreateWindowW(L"static", L"Add Account:",
				WS_CHILD | WS_VISIBLE,
				0, 20, 100, 20, hwnd, 0, GetModuleHandleW(NULL), NULL);

			hwndName = CreateWindowW(L"edit", 0,
				WS_CHILD | WS_BORDER | WS_VISIBLE | WS_TABSTOP,
				100, 20, 100, 20, hwnd, (HMENU)IDC_NAME, GetModuleHandleW(NULL), NULL);

			hwndPass = CreateWindowW(L"edit", 0,
				WS_CHILD | WS_BORDER | WS_VISIBLE | WS_TABSTOP,
				200, 20, 100, 20, hwnd, (HMENU)IDC_PASS, GetModuleHandleW(NULL), NULL);

			hwndLogin = CreateWindowW(L"button", L"Login",
				WS_CHILD | WS_VISIBLE | BS_CHECKBOX | WS_TABSTOP,
				0, 40, 100, 20, hwnd, (HMENU)IDC_LOGIN, GetModuleHandleW(NULL), NULL);

			hwndAdd = CreateWindowW(L"button", L"Add",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				100, 40, 100, 20, hwnd, (HMENU)IDC_ADD, GetModuleHandleW(NULL), NULL);

			hwndDelete =  CreateWindowW(L"button", L"Delete",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				200, 40, 100, 20, hwnd, (HMENU)IDC_DEL, GetModuleHandleW(NULL), NULL);

			hwndList = CreateWindowW(L"listbox", NULL,
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
				0, 60, 300, 360, hwnd, (HMENU)IDC_LIST, GetModuleHandleW(NULL), NULL);

			pListProc = (WNDPROC)SetWindowLong(hwndList, GWL_WNDPROC, (LONG)&ListProc);

			defaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessageW(hwndStatic1,	WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndPath,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndStatic2,	WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndName,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndPass,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndLogin,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndAdd,		WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndDelete,	WM_SETFONT, (WPARAM)defaultFont, TRUE);
			SendMessageW(hwndList,		WM_SETFONT, (WPARAM)defaultFont, TRUE);

			LoadSettings(L".\\AutoLogin.ini");

			break;

		case WM_ACTIVATE:
			if (wParam == WA_INACTIVE) {
				hwndLastFocus = GetFocus();
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK: {
					HWND focus = GetFocus();
					if (focus == hwndPath) {
						SetFocus(hwndName);
					} else if (focus == hwndName) {
						SetFocus(hwndPass);
					} else if (focus == hwndPass) {
						OnAddPressed();
						SetFocus(hwndName);
					}
					break;
				}
				case IDC_LOGIN: {
					CheckDlgButton(hwnd, IDC_LOGIN,
						IsDlgButtonChecked(hwnd, IDC_LOGIN) ? BST_UNCHECKED : BST_CHECKED);
					break;
				}
				case IDC_ADD: {
					OnAddPressed();
					break;
				}
				case IDC_DEL: {
					OnDeletePressed();
					break;
				}
				case IDC_LIST: {
					if (HIWORD(wParam) == LBN_DBLCLK) {
						LRESULT sel = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
						if (sel != LB_ERR) {
							WCHAR path[MAX_PATH];
							BOOL login;
							SendMessageW(hwndPath, WM_GETTEXT,
								sizeof(path) / sizeof(path[0]), (LPARAM)path);
							login = IsDlgButtonChecked(hwnd, IDC_LOGIN);
							StartGame(path, login, sel);
						}
					}
					break;
				}
			}
			break;
		case WM_DESTROY:
			SaveSettings(L".\\AutoLogin.ini", L".\\AutoLogin.ini.bak");
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
{
	WNDCLASSW wc;
	MSG msg;
	NONCLIENTMETRICS ncm;

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.lpszClassName	= L"AutoLogin";
	wc.hInstance		= hInstance;
	wc.hbrBackground	= GetSysColorBrush(COLOR_3DFACE);
	wc.lpszMenuName		= NULL;
	wc.lpfnWndProc		= WndProc;
	wc.hCursor		= LoadCursorW(NULL, IDC_ARROW);
	wc.hIcon		= LoadIconW(NULL, IDI_APPLICATION);

	RegisterClassW(&wc);
	hwnd = CreateWindowW(wc.lpszClassName, L"AutoLogin",
		(WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME) | WS_VISIBLE,
		100, 100, 307, 442, NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessageW(&msg, NULL, 0, 0)) {
		if (!IsDialogMessageW(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return (int) msg.wParam;
}
