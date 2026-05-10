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
#include <vector>

#define XXH_INLINE_ALL
#include "xxhash.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "User32.lib")

std::vector<char> GetGameFile(const wchar_t* path)
{
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return {};


  LARGE_INTEGER li{};
  GetFileSizeEx(f, &li);

  std::vector<char> data(li.QuadPart);

  DWORD bytesRead{};
  ReadFile(f, data.data(), (DWORD)data.size(), &bytesRead, NULL);

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

uint32_t hash_data(const std::vector<char>& data)
{
    return XXH32(data.data(), data.size(), 0);
}

void decrypt_game(std::vector<char> gamefile, const wchar_t* key)
{
  char* data = gamefile.data();

  int pe_offset = read_int(data, 0x3C);

  IMAGE_NT_HEADERS32* pe_header = (IMAGE_NT_HEADERS32*)(data + pe_offset);

  IMAGE_SECTION_HEADER* segment_start = (IMAGE_SECTION_HEADER*)(data + pe_offset + 248);


  IMAGE_SECTION_HEADER text_segment{};

  for (int i = 0; i < pe_header->FileHeader.NumberOfSections; i++) {
    IMAGE_SECTION_HEADER header = segment_start[i];

    if (strcmp((const char*)header.Name, ".text") == 0) {
      text_segment = header;
      break;
    }
  }

  IID iid{};
  IIDFromString(key, &iid); // from resources of Luxor.exe

  unsigned int lcg_state = iid.Data1;

  char* text_start = gamefile.data() + text_segment.PointerToRawData;

  size_t entrypoint_offset = pe_header->OptionalHeader.AddressOfEntryPoint - text_segment.VirtualAddress;

  entrypoint_offset -= 0x10;

  for (size_t i = 0; i < text_segment.Misc.VirtualSize; i++) {
    if (i >= entrypoint_offset && i <= entrypoint_offset + 0x20) {
      continue;
    }

    lcg_state = 1103515245 * lcg_state + 12345;

    text_start[i] -= BYTE2(lcg_state) ^ __ROL1__(*((char*)&iid.Data1 + (i & 0xF)), i & 7);

  }

  HANDLE outf = CreateFileW(L"game_dec.dmg", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  DWORD byteswritten{};
  WriteFile(outf, gamefile.data(), gamefile.size(), &byteswritten, NULL);
  CloseHandle(outf);
}

#ifdef RELEASE
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
#endif

int main()
{
#ifdef RELEASE
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
#endif

  std::vector<char> gamefile = GetGameFile(L"game.dmg");

  const wchar_t* name = L"";
  const wchar_t* key = L"";
  const uint32_t game_hash = hash_data(gamefile);
  int app_ids[2] = {};

  switch (game_hash) {
    case 0x581E9069:
      name = L"Luxor";
      key = L"{0DDCE464-5839-4ABD-BF32-DA838B9DB604}";
	  app_ids[0] = 15970;
	  app_ids[1] = 15972;
      break;
    case 0xC7FFCD99:
      name = L"Luxor (French)";
      key = L"{70052C97-20C2-4124-9A57-2944D83F7E83}";
	  app_ids[0] = 15970;
	  app_ids[1] = 15972;
      break;
    case 0x1E1267D4:
      name = L"Luxor (German)";
      key = L"{CC7071A6-B663-49ED-94E8-7B5CB8D24B9D}";
	  app_ids[0] = 15970;
	  app_ids[1] = 15972;
      break;
    case 0x8BC5255E:
      name = L"Luxor (Swedish)";
      key = L"{EB308A84-66AF-42A4-A940-BDA13907DC1A}";
	  app_ids[0] = 15970;
	  app_ids[1] = 15972;
      break;
    case 0x044E7C8C:
      name = L"Luxor Amun Rising";
      key = L"{B107D9DA-82FD-46F1-A000-5175CD244980}";
	  app_ids[0] = 15910;
	  app_ids[1] = 15912;
      break;
    case 0x001D343A:
      name = L"Luxor Amun Rising (French)";
      key = L"{8A1B6391-E6D0-41B7-874E-8C6F9E153B13}";
	  app_ids[0] = 15910;
	  app_ids[1] = 15912;
      break;
    case 0xDEA924D6:
      name = L"Luxor Amun Rising (German)";
      key = L"{98160D16-0A32-4661-9AFB-9C2F41D3778D}";
	  app_ids[0] = 15910;
	  app_ids[1] = 15912;
      break;
    case 0x0A0EFEAB:
      name = L"Luxor Amun Rising (Swedish)";
      key = L"{91422428-53B3-4869-A20B-BEBDDC3FA95B}";
	  app_ids[0] = 15910;
	  app_ids[1] = 15912;
      break;
    case 0x2C96A170:
      name = L"Luxor 2";
      key = L"{6EAB4510-C0E6-4009-9D04-FA45EAF31D02}";
	  app_ids[0] = 15920;
	  app_ids[1] = 15922;
      break;
    case 0x21C5A0C7:
      name = L"Luxor 2 (French)";
      key = L"{A23460FC-5467-4B7E-A8E0-6403ADB9BA99}";
	  app_ids[0] = 15920;
	  app_ids[1] = 15922;
      break;
    case 0x9CBF6C9E:
      name = L"Luxor 2 (German)";
      key = L"{BFED3C63-EB97-4A5B-A9A8-7BE4E6A804B3}";
	  app_ids[0] = 15920;
	  app_ids[1] = 15922;
      break;
    case 0xADACD3ED:
      name = L"Luxor 3";
      key = L"{B1C36261-ED4E-4AE4-AD30-E687990F7216}";
	  app_ids[0] = 15930;
	  app_ids[1] = 15930;
      break;
    case 0x50E7AE4D:
      name = L"Luxor 4";
      key = L"{62FF8BE8-7F9E-446C-A57E-9E6B0ADF54E8}";
	  app_ids[0] = 16040;
	  app_ids[1] = 16042;
      break;
    case 0x144CE8BA:
      name = L"Luxor 5";
      key = L"{FEE36EEC-86FD-4D7D-9344-69227D70EBB2}";
	  app_ids[0] = 60340;
	  app_ids[1] = 60340; // NO demo for Luxor 5
      break;
    case 0x6393E8A9:
      name = L"Luxor Mahjong";
      key = L"{E5055545-CFDB-4DBC-B1D5-2BAB7158B843}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
    case 0x0FE73EA3:
      name = L"Luxor Mahjong (French)";
      key = L"{3572ABA5-1A04-4D6A-9986-C49039B4BF26}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
    case 0x7D950903:
      name = L"Luxor Mahjong (German)";
      key = L"{72628D8D-D2A7-448B-AB4A-59057E6C13AA}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
    case 0x9E5E428D:
      name = L"Luxor Mahjong (Italian)";
      key = L"{79719CB3-D982-44D9-94A0-A33726695E37}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
    case 0x58A3AA74:
      name = L"Luxor Mahjong (Spanish)";
      key = L"{7B4EF7F1-F86E-40A2-B73B-51ABFDD3C72F}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
    case 0x1F0A77D7:
      name = L"Luxor Mahjong (Swedish)";
      key = L"{01DDCCA2-11AE-4E17-ACFA-12953818F331}";
	  app_ids[0] = 32110;
	  app_ids[1] = 32117;
      break;
  }

#ifdef RELEASE
  printf("Checking ownership of %d or %d\n", app_ids[0], app_ids[1]);
  if (!(OwnsAppId(app_ids[0]) || OwnsAppId(app_ids[1]))) {
    printf("Does not own game\n");
    SteamCleanup(&err);
    return 0;
  }
#else
  wprintf(L"decrypting game %ls with key %ls\n", name, key);
#endif

  decrypt_game(gamefile, key);

  if (wcscmp(name, L"Luxor 2") == 0) { // Luxor 2 has a >Win98 check that fails
    std::vector<char> platform = GetGameFile(L"platform.dll");

    const char search[24]  = "\x6A\x05\xE8\xBF\xF7\xFF";
    const char replace[24] = "\x6A\x00\xE8\xBF\xF7\xFF";

    bool didpatch = false;
    for (size_t i = 0x20FF0; i < 0x21020; i++)
    {
        if (memcmp(&platform.data()[i], search, 6) == 0)
        {
            memcpy(&platform.data()[i], replace, 6);
            didpatch = true;
        }
    }

    if (didpatch) {
      printf("Found Windows 98 check in platform.dll, patching...\n");
      MoveFileW(L"platform.dll", L"platform_unpatched.dll");

      HANDLE outf = CreateFileW(L"platform.dll", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

      DWORD byteswritten{};
      WriteFile(outf, platform.data(), platform.size(), &byteswritten, NULL);
      CloseHandle(outf);
    }

  }

  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(L"game_dec.dmg", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
#ifdef RELEASE
    SteamCleanup(&err);
#endif
    return 0;
  }

  CloseHandle(pi.hThread);

  while (true) {
    DWORD result = WaitForSingleObject(pi.hProcess, 0xEA60);
    if (result != WAIT_TIMEOUT)
      break;

  }

  CloseHandle(pi.hProcess);

  // if (wcscmp(name, L"Luxor 2") == 0) {
    // ReplaceFileW(L"platform.dll", L"platform_unpatched.dll", NULL, 0, NULL, NULL);
    // DeleteFileW(L"platform_unpatched.dll");
  // }

#ifdef RELEASE
  DeleteFileW(L"game_dec.dmg");
  SteamCleanup(&err);
#endif

  return 0;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
  exit(main());
}
