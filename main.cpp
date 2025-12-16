///
/// The normal Luxor launcher works by launching game.dmg suspended, then uses "GetThreadSelectorEntry" to get the TEB.
/// It then uses the TEB to get the PEB then to get the PE header of game.dmg, it then reads all of .text, does some XOR decryption with a GUID key in the launcher resources.
/// It then writes it all back to the exe and unsuspends it.
/// 
/// "GetThreadSelectorEntry" is a x86 only function with no viable replacement on modern systems (Windows 8 and later).
/// So this replacement launcher is needed to be able to play the games.
///

#include <Windows.h>

#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "User32.lib")

char* GetGameFile(const wchar_t* path, size_t& size)
{
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return 0;


  LARGE_INTEGER li{};
  GetFileSizeEx(f, &li);

  size = li.QuadPart;
  char* data = new char[li.QuadPart] {};

  DWORD bytesRead{};
  ReadFile(f, data, li.QuadPart, &bytesRead, NULL);

  CloseHandle(f);

  return data;
}

int read_int(const char* data, size_t position)
{
  return *(int*)(data + position);
}

void read_bytes(const char* data, size_t position, char* out, size_t size)
{
  memcpy(out, data + position, size);
}

template<class T> T __ROL__(T value, int count)
{
  const unsigned int nbits = sizeof(T) * 8;

  if (count > 0) {
    count %= nbits;
    T high = value >> (nbits - count);
    if (T(-1) < 0) // signed value
      high &= ~((T(-1) << count));
    value <<= count;
    value |= high;
  }
  else {
    count = -count % nbits;
    T low = value << (nbits - count);
    value >>= count;
    value |= low;
  }
  return value;
}

inline char __ROL1__(char value, int count) { return __ROL__((char)value, count); }
#define BYTEn(x, n)   (*((char*)&(x)+n))
#define BYTE2(x)   BYTEn(x,  2)

#define GAME_LUXOR 0
#define GAME_LUXOR2 1
#define GAME_LUXORAR 2
int get_game()
{
  WCHAR exename[2048]{};
  GetModuleFileNameW(NULL, exename, sizeof(exename) / sizeof(WCHAR));

  if (wcsstr(exename, L"Luxor.exe") != NULL) return GAME_LUXOR;
  if (wcsstr(exename, L"Luxor2.exe") != NULL) return GAME_LUXOR2;
  if (wcsstr(exename, L"Luxor AR.exe") != NULL) return GAME_LUXORAR;

  return GAME_LUXOR;
}

void decrypt_game()
{
  size_t gamesize = 0;

  char* gamefile = GetGameFile(L"game.dmg", gamesize);

  int pe_offset = read_int(gamefile, 0x3C);

  IMAGE_NT_HEADERS32* pe_header = (IMAGE_NT_HEADERS32*)(gamefile + pe_offset);

  IMAGE_SECTION_HEADER* segment_start = (IMAGE_SECTION_HEADER*)(gamefile + pe_offset + 248);


  IMAGE_SECTION_HEADER text_segment{};

  for (int i = 0; i < pe_header->FileHeader.NumberOfSections; i++) {
    IMAGE_SECTION_HEADER header = segment_start[i];

    if (strcmp((const char*)header.Name, ".text") == 0) {
      text_segment = header;
      break;
    }
  }


  // Luxor: {0DDCE464-5839-4ABD-BF32-DA838B9DB604}
  // Luxor 2: {6EAB4510-C0E6-4009-9D04-FA45EAF31D02}
  // Luxor AR: {B107D9DA-82FD-46F1-A000-5175CD244980}
  const wchar_t* key = L"";

  switch (get_game()) {
  case GAME_LUXOR:
    key = L"{0DDCE464-5839-4ABD-BF32-DA838B9DB604}";
    break;
  case GAME_LUXOR2:
    key = L"{6EAB4510-C0E6-4009-9D04-FA45EAF31D02}";
    break;
  case GAME_LUXORAR:
    key = L"{B107D9DA-82FD-46F1-A000-5175CD244980}";
  }

  IID iid{};
  IIDFromString(key, &iid); // from resources of Luxor.exe


  unsigned int lcg_state = iid.Data1;

  char* text_start = gamefile + text_segment.PointerToRawData;

  size_t entrypoint_offset = pe_header->OptionalHeader.AddressOfEntryPoint - text_segment.VirtualAddress;

  entrypoint_offset -= 0x10;

  for (int i = 0; i < text_segment.Misc.VirtualSize; i++) {
    if (i >= entrypoint_offset && i <= entrypoint_offset + 0x20) {
      continue;
    }

    lcg_state = 1103515245 * lcg_state + 12345;

    text_start[i] -= BYTE2(lcg_state) ^ __ROL1__(*((char*)&iid.Data1 + (i & 0xF)), i & 7);

  }

  HANDLE outf = CreateFileW(L"game_dec.dmg", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  DWORD byteswritten{};
  WriteFile(outf, gamefile, gamesize, &byteswritten, NULL);
  CloseHandle(outf);

  delete[] gamefile;
}

struct TSteamError {
  int eSteamError;
  int eDetailedErrorType;
  int nDetailedErrorCode;
  char szDesc[255];
};

int(__cdecl* SteamStartup)(unsigned int uUsingMask, TSteamError* pError);
int(__cdecl* SteamIsAppSubscribed)(unsigned int uAppId, int* pbIsAppSubscribed, int* pbIsSubscriptionPending, TSteamError* pError);
int(__cdecl* SteamCleanup)(TSteamError* pError);

bool OwnsAppId(unsigned int appId)
{
  int isSubscribed = 0;
  int isSubscriptionPending = 0;
  TSteamError err{};
  SteamIsAppSubscribed(appId, &isSubscribed, &isSubscriptionPending, &err);

  return isSubscribed == 1;
}

int main()
{
  HMODULE steamdll = LoadLibraryA("Steam.dll");

  if (steamdll == NULL) {
    MessageBoxA(NULL, "Could not load Steam.dll", "Error opening game", MB_ICONERROR);
    return 1;
  }

  SteamStartup = (decltype(SteamStartup))GetProcAddress(steamdll, "SteamStartup");
  SteamIsAppSubscribed = (decltype(SteamIsAppSubscribed))GetProcAddress(steamdll, "SteamIsAppSubscribed");
  SteamCleanup = (decltype(SteamCleanup))GetProcAddress(steamdll, "SteamCleanup");

  TSteamError err{};
  if (!SteamStartup(14, &err)) {
    printf("%s\n", err.szDesc);
    return 0;
  }

  if (!(
    OwnsAppId(15970) || OwnsAppId(15972) || // Luxor & Luxor Demo
    OwnsAppId(15920) || OwnsAppId(15922) || // Luxor 2 & Luxor 2 Demo
    OwnsAppId(15910) || OwnsAppId(15912) // Luxor Anum Rising & Luxor Anum Rising Demo
    )) {
    printf("Does not own game\n");
    SteamCleanup(&err);
    return 0;
  }

  decrypt_game();

  if (get_game() == GAME_LUXOR2) { // Luxor 2 has a >Win98 check that fails
    size_t platformsize = 0;
    char* platform = GetGameFile(L"platform.dll", platformsize);

    // 0x2101A: 6A 05 push 5


    bool didpatch = false;
    if (platformsize > 0x2101B && platform[0x2101A] == 0x6A && platform[0x2101B] == 5) {
      platform[0x2101B] = 0;
      didpatch = true;
    }

    if (didpatch) {
      MoveFileW(L"platform.dll", L"platform_unpatched.dll");

      HANDLE outf = CreateFileW(L"platform.dll", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

      DWORD byteswritten{};
      WriteFile(outf, platform, platformsize, &byteswritten, NULL);
      CloseHandle(outf);
    }

    delete[] platform;

  }

  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(L"game_dec.dmg", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    SteamCleanup(&err);
    return 0;
  }

  CloseHandle(pi.hThread);

  while (true) {
    DWORD result = WaitForSingleObject(pi.hProcess, 0xEA60);
    if (result != WAIT_TIMEOUT)
      break;

  }

  CloseHandle(pi.hProcess);

  DeleteFileW(L"game_dec.dmg");

  if (get_game() == GAME_LUXOR2) {
    ReplaceFileW(L"platform.dll", L"platform_unpatched.dll", NULL, 0, NULL, NULL);
    DeleteFileW(L"platform_unpatched.dll");
  }

  SteamCleanup(&err);
  return 0;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
  exit(main());
}
