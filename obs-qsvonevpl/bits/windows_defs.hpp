#include <Windows.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define MSDK_FOPEN(FH, FN, M)           { fopen_s(&FH, FN, M); }
#define MSDK_SLEEP(X)                   { Sleep(X); }

typedef LARGE_INTEGER mfxTime;
