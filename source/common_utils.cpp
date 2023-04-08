#include "common_utils.h"
#include "QSV_Encoder.h"
#include <string>

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//

void PrintErrString(int err, const char *filestr, int line)
{
	switch (err) {
	case 0:
		printf("\n No error.\n");
		break;
	case -1:
		printf("\n Unknown error: %s %d\n", filestr, line);
		break;
	case -2:
		printf("\n Null pointer.  Check filename/path + permissions? %s %d\n",
		       filestr, line);
		break;
	case -3:
		printf("\n Unsupported feature/library load error. %s %d\n",
		       filestr, line);
		break;
	case -4:
		printf("\n Could not allocate memory. %s %d\n", filestr, line);
		break;
	case -5:
		printf("\n Insufficient IO buffers. %s %d\n", filestr, line);
		break;
	case -6:
		printf("\n Invalid handle. %s %d\n", filestr, line);
		break;
	case -7:
		printf("\n Memory lock failure. %s %d\n", filestr, line);
		break;
	case -8:
		printf("\n Function called before initialization. %s %d\n",
		       filestr, line);
		break;
	case -9:
		printf("\n Specified object not found. %s %d\n", filestr, line);
		break;
	case -10:
		printf("\n More input data expected. %s %d\n", filestr, line);
		break;
	case -11:
		printf("\n More output surfaces expected. %s %d\n", filestr,
		       line);
		break;
	case -12:
		printf("\n Operation aborted. %s %d\n", filestr, line);
		break;
	case -13:
		printf("\n HW device lost. %s %d\n", filestr, line);
		break;
	case -14:
		printf("\n Incompatible video parameters. %s %d\n", filestr,
		       line);
		break;
	case -15:
		printf("\n Invalid video parameters. %s %d\n", filestr, line);
		break;
	case -16:
		printf("\n Undefined behavior. %s %d\n", filestr, line);
		break;
	case -17:
		printf("\n Device operation failure. %s %d\n", filestr, line);
		break;
	case -18:
		printf("\n More bitstream data expected. %s %d\n", filestr,
		       line);
		break;
	case -19:
		printf("\n Incompatible audio parameters. %s %d\n", filestr,
		       line);
		break;
	case -20:
		printf("\n Invalid audio parameters. %s %d\n", filestr, line);
		break;
	default:
		printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
	}
}

mfxStatus ReadPlaneData(mfxU16 w, mfxU16 h, mfxU8 *buf, mfxU8 *ptr,
			mfxU16 pitch, mfxU16 offset, FILE *fSource)
{
	mfxU32 nBytesRead;
	for (mfxU16 i = 0; i < h; i++) {
		nBytesRead = (mfxU32)fread(buf, 1, w, fSource);
		if (w != nBytesRead)
			return MFX_ERR_MORE_DATA;
		for (mfxU16 j = 0; j < w; j++)
			ptr[i * pitch + j * 2 + offset] = buf[j];
	}
	return MFX_ERR_NONE;
}

mfxStatus LoadRawFrame(mfxFrameSurface1 *pSurface, FILE *fSource)
{
	if (!fSource) {
		// Simulate instantaneous access to 1000 "empty" frames.
		static int frameCount = 0;
		if (1000 == frameCount++)
			return MFX_ERR_MORE_DATA;
		else
			return MFX_ERR_NONE;
	}

	mfxStatus sts = MFX_ERR_NONE;
	mfxU32 nBytesRead;
	mfxU16 w, h, i, pitch;
	mfxU8 *ptr;
	mfxFrameInfo *pInfo = &pSurface->Info;
	mfxFrameData *pData = &pSurface->Data;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	} else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	pitch = pData->Pitch;
	ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

	// read luminance plane
	for (i = 0; i < h; i++) {
		nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, fSource);
		if (w != nBytesRead)
			return MFX_ERR_MORE_DATA;
	}

	mfxU8 buf[2048]; // maximum supported chroma width for nv12
	w /= 2;
	h /= 2;
	ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;
	if (w > 2048)
		return MFX_ERR_UNSUPPORTED;

	// load U
	sts = ReadPlaneData(w, h, buf, ptr, pitch, 0, fSource);
	if (MFX_ERR_NONE != sts)
		return sts;
	// load V
	sts = ReadPlaneData(w, h, buf, ptr, pitch, 1, fSource);
	if (MFX_ERR_NONE != sts)
		return sts;

	return MFX_ERR_NONE;
}

mfxStatus LoadRawRGBFrame(mfxFrameSurface1 *pSurface, FILE *fSource)
{
	if (!fSource) {
		// Simulate instantaneous access to 1000 "empty" frames.
		static int frameCount = 0;
		if (1000 == frameCount++)
			return MFX_ERR_MORE_DATA;
		else
			return MFX_ERR_NONE;
	}

	size_t nBytesRead;
	mfxU16 w, h;
	mfxFrameInfo *pInfo = &pSurface->Info;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	} else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	for (mfxU16 i = 0; i < h; i++) {
		nBytesRead = fread(pSurface->Data.B + i * pSurface->Data.Pitch,
				   1, w * 4, fSource);
		if ((size_t)(w * 4) != nBytesRead)
			return MFX_ERR_MORE_DATA;
	}

	return MFX_ERR_NONE;
}

mfxStatus WriteBitStreamFrame(mfxBitstream *pMfxBitstream, FILE *fSink)
{
	mfxU32 nBytesWritten =
		(mfxU32)fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset,
			       1, pMfxBitstream->DataLength, fSink);
	if (nBytesWritten != pMfxBitstream->DataLength)
		return MFX_ERR_UNDEFINED_BEHAVIOR;

	pMfxBitstream->DataLength = 0;

	return MFX_ERR_NONE;
}

mfxStatus ReadBitStreamData(mfxBitstream *pBS, FILE *fSource)
{
	memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
	pBS->DataOffset = 0;

	mfxU32 nBytesRead = (mfxU32)fread(pBS->Data + pBS->DataLength, 1,
					  pBS->MaxLength - pBS->DataLength,
					  fSource);

	if (0 == nBytesRead)
		return MFX_ERR_MORE_DATA;

	pBS->DataLength += nBytesRead;

	return MFX_ERR_NONE;
}

mfxStatus WriteSection(mfxU8 *plane, mfxU16 factor, mfxU16 chunksize,
		       mfxFrameInfo *pInfo, mfxFrameData *pData, mfxU32 i,
		       mfxU32 j, FILE *fSink)
{
	if (chunksize != fwrite(plane + (pInfo->CropY * pData->Pitch / factor +
					 pInfo->CropX) +
					i * pData->Pitch + j,
				1, chunksize, fSink))
		return MFX_ERR_UNDEFINED_BEHAVIOR;
	return MFX_ERR_NONE;
}

mfxStatus WriteRawFrame(mfxFrameSurface1 *pSurface, FILE *fSink)
{
	mfxFrameInfo *pInfo = &pSurface->Info;
	mfxFrameData *pData = &pSurface->Data;
	mfxU32 i, j, h, w;
	mfxStatus sts = MFX_ERR_NONE;

	for (i = 0; i < pInfo->CropH; i++)
		sts = WriteSection(pData->Y, 1, pInfo->CropW, pInfo, pData, i,
				   0, fSink);

	h = pInfo->CropH / 2;
	w = pInfo->CropW;
	for (i = 0; i < h; i++)
		for (j = 0; j < w; j += 2)
			sts = WriteSection(pData->UV, 2, 1, pInfo, pData, i, j,
					   fSink);
	for (i = 0; i < h; i++)
		for (j = 1; j < w; j += 2)
			sts = WriteSection(pData->UV, 2, 1, pInfo, pData, i, j,
					   fSink);

	return sts;
}

int GetFreeTaskIndex(Task *pTaskPool, mfxU16 nPoolSize)
{
	if (pTaskPool)
		for (int i = 0; i < nPoolSize; i++)
			if (!pTaskPool[i].syncp)
				return i;
	return MFX_ERR_NOT_FOUND;
}

void ClearYUVSurfaceSysMem(mfxFrameSurface1 *pSfc, mfxU16 width, mfxU16 height)
{
	// In case simulating direct access to frames we initialize the allocated surfaces with default pattern
	memset(pSfc->Data.Y, 100, width * height);      // Y plane
	memset(pSfc->Data.U, 50, (width * height) / 2); // UV plane
}

// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1 **pSurfacesPool, mfxU16 nPoolSize)
{
	if (pSurfacesPool)
		for (mfxU16 i = 0; i < nPoolSize; i++)
			if (0 == pSurfacesPool[i]->Data.Locked)
				return i;
	return MFX_ERR_NOT_FOUND;
}

char mfxFrameTypeString(mfxU16 FrameType)
{
	mfxU8 FrameTmp = FrameType & 0xF;
	char FrameTypeOut;
	switch (FrameTmp) {
	case MFX_FRAMETYPE_I:
		FrameTypeOut = 'I';
		break;
	case MFX_FRAMETYPE_P:
		FrameTypeOut = 'P';
		break;
	case MFX_FRAMETYPE_B:
		FrameTypeOut = 'B';
		break;
	default:
		FrameTypeOut = '*';
	}
	return FrameTypeOut;
}

enum qsv_cpu_platform qsv_get_cpu_platform()
{
	using std::string;

	int cpuInfo[4];
	util_cpuid(cpuInfo, 0);

	string vendor;
	vendor += string((char*)&cpuInfo[1], 4);
	vendor += string((char*)&cpuInfo[3], 4);
	vendor += string((char*)&cpuInfo[2], 4);

	if (vendor != "GenuineIntel")
		return QSV_CPU_PLATFORM_UNKNOWN;

	util_cpuid(cpuInfo, 1);
	uint8_t model = ((cpuInfo[0] >> 4) & 0xF) + ((cpuInfo[0] >> 12) & 0xF0);
	uint8_t family =
		((cpuInfo[0] >> 8) & 0xF) + ((cpuInfo[0] >> 20) & 0xFF);

	// See Intel 64 and IA-32 Architectures Software Developer's Manual,
	// Vol 3C Table 35-1
	if (family != 6)
		return QSV_CPU_PLATFORM_UNKNOWN;

	switch (model) {
	case 0x1C:
	case 0x26:
	case 0x27:
	case 0x35:
	case 0x36:
		return QSV_CPU_PLATFORM_BNL;

	case 0x2a:
	case 0x2d:
		return QSV_CPU_PLATFORM_SNB;

	case 0x3a:
	case 0x3e:
		return QSV_CPU_PLATFORM_IVB;

	case 0x37:
	case 0x4A:
	case 0x4D:
	case 0x5A:
	case 0x5D:
		return QSV_CPU_PLATFORM_SLM;

	case 0x4C:
		return QSV_CPU_PLATFORM_CHT;

	case 0x3c:
	case 0x3f:
	case 0x45:
	case 0x46:
		return QSV_CPU_PLATFORM_HSW;
	case 0x3d:
	case 0x47:
	case 0x4f:
	case 0x56:
		return QSV_CPU_PLATFORM_BDW;

	case 0x4e:
	case 0x5e:
		return QSV_CPU_PLATFORM_SKL;
	case 0x5c:
		return QSV_CPU_PLATFORM_APL;
	case 0x8e:
	case 0x9e:
		return QSV_CPU_PLATFORM_KBL;
	case 0x7a:
		return QSV_CPU_PLATFORM_GLK;
	case 0x66:
		return QSV_CPU_PLATFORM_CNL;
	case 0x7d:
	case 0x7e:
		return QSV_CPU_PLATFORM_ICL;
	case 0x8D:
		return QSV_CPU_PLATFORM_TGL;
	case 0x8C:
		return QSV_CPU_PLATFORM_TGL;
	case 0xA7:
		return QSV_CPU_PLATFORM_RKL;
	case 0x9A:
		return QSV_CPU_PLATFORM_ADL;
	case 0x97:
		return QSV_CPU_PLATFORM_ADL;
	case 0xB7:
		return QSV_CPU_PLATFORM_RPL;
	}

	//assume newer revisions are at least as capable as Haswell
	return QSV_CPU_PLATFORM_INTEL;
}
