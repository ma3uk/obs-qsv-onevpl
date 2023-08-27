#include "../obs-qsvonevpl/helpers/common_utils.hpp"

struct Task {
	mfxBitstream* mfxBS;
	mfxSyncPoint syncp;
};

class QSV_VPL_TASK {
public:
	//QSV_VPL_TASK();
	~QSV_VPL_TASK();

	mfxStatus Init(mfxU16 PoolSize, mfxU32 BufferSize, mfxU32 Codec);
	mfxStatus SetData(int TaskID, mfxU8* Data);
	mfxStatus GetFreeTaskIndex(int* TaskID);
	//mfxStatus GetFreeTask(struct Task Task);
	mfxStatus GetNextTaskIndex(int* TaskID);
	void SetTaskFlags(mfxU16 Flags);
	mfxStatus Free(int TaskID);
	mfxStatus FreeAllTasks();
	mfxStatus Close();
	mfxStatus Reconfigure(mfxU16 PoolSize, mfxU32 BufferSize, mfxU32 Codec);

	mfxBitstream *GetTaskBitstream(int TaskID);
	mfxSyncPoint *GetTaskSyncPoint(int TaskID);

protected:
	mfxU16 TaskPoolSize;

	mfxU32 BufferSize;

	mfxU16 TaskID;
	mfxU16 NextTaskID;

	mfxU32 TaskCodec;

	mfxU16 TaskFlags;
private:
	Task TaskPool;
};