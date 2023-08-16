#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MSDK_FOPEN(FH, FN, M)           { fopen_s(&FH, FN, M); }
#define MSDK_SLEEP(X)                   { Sleep(X); }

typedef LARGE_INTEGER mfxTime;
