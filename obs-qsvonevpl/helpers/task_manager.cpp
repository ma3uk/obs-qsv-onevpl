#include "task_manager.hpp"

//
//void QSV_VPL_TASK::QSV_VPL_TASK()
//	: TaskPoolSize(0),
//	BufferSize(),
//	TaskCodec(),
//	NextTaskID(),
//	TaskFlags(),
//	TaskPool()
//{
//	
//}
//
QSV_VPL_TASK::~QSV_VPL_TASK()
{
	Close();
}

void QSV_VPL_TASK::SetTaskFlags(mfxU16 Flags) {

	TaskFlags = Flags;
}

mfxStatus QSV_VPL_TASK::Init(mfxU16 PoolSize, mfxU32 BufferSize, mfxU32 Codec)
{
	if (PoolSize >= 0 || BufferSize >= 0 || Codec >= 0)
	{
		this->TaskPoolSize = PoolSize;
		this->BufferSize = BufferSize;
		this->TaskCodec = Codec;
	}
	else {
		this->TaskPoolSize = 0;
		this->BufferSize = 0;
		this->TaskCodec = MFX_CODEC_AVC;
	}

	TaskPool = new struct Task[TaskPoolSize];
	memset(TaskPool, 0, sizeof(Task) * TaskPoolSize);

	mfxU32 MaxLength = static_cast<mfxU32>(
		(BufferSize * 1000 * 100 * 3));

	for (int i = 0; i < TaskPoolSize; i++) {
		TaskPool[i].mfxBS = new mfxBitstream;
		memset(TaskPool[i].mfxBS, 0, sizeof(mfxBitstream));
		TaskPool[i].mfxBS->MaxLength = MaxLength;
		TaskPool[i].mfxBS->Data =
			new mfxU8[TaskPool[i].mfxBS->MaxLength];
		TaskPool[i].mfxBS->DataOffset = 0;
		TaskPool[i].mfxBS->DataLength = 0;
		TaskPool[i].mfxBS->CodecId = TaskCodec;
		TaskPool[i].mfxBS->DataFlag = TaskFlags;
		TaskPool[i].syncp = nullptr;

		if (!TaskPool[i].mfxBS->Data) {
			return MFX_ERR_MEMORY_ALLOC;
		}
	}

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_TASK::SetData(int TaskID, mfxU8* Data) {
	if (TaskPool != nullptr) {
		TaskPool[TaskID].mfxBS->Data = Data;

		return MFX_ERR_NONE;
	}

	return MFX_ERR_ABORTED;
}

mfxStatus QSV_VPL_TASK::Free(int TaskID) {
	if (TaskPool != nullptr) {
		TaskPool[TaskID].mfxBS->DataLength = 0;
		TaskPool[TaskID].mfxBS->DataOffset = 0;
		TaskPool[TaskID].syncp =
			static_cast<mfxSyncPoint>(nullptr);

		return MFX_ERR_NONE;
	}

	return MFX_ERR_ABORTED;
}

mfxStatus QSV_VPL_TASK::FreeAllTasks() {
	if (TaskPool != nullptr) {
		for (int i = 0; i < TaskPoolSize; i++) {
			TaskPool[i].mfxBS->DataLength = 0;
			TaskPool[i].mfxBS->DataOffset = 0;
			TaskPool[i].syncp =
				static_cast<mfxSyncPoint>(nullptr);
		}

		return MFX_ERR_NONE;
	}

	return MFX_ERR_ABORTED;
}

mfxStatus QSV_VPL_TASK::Close() {
	if (TaskPool != nullptr) {
		for (int i = 0; i < TaskPoolSize; i++) {
			delete TaskPool[i].mfxBS->Data;
			delete TaskPool[i].mfxBS;
		}
		delete[] TaskPool;
		TaskPool = nullptr;

		return MFX_ERR_NONE;
	}

	return MFX_ERR_ABORTED;
}

mfxStatus QSV_VPL_TASK::GetFreeTaskIndex(int* TaskID)
{
	if (TaskPool != nullptr) {
		for (int i = 0; i < TaskPoolSize; i++) {
			if (static_cast<mfxSyncPoint>(nullptr) ==
				TaskPool[i].syncp) {
				NextTaskID =
					(i + 1) % static_cast<int>(TaskPoolSize);
				*TaskID = i;
				return MFX_ERR_NONE;
			}
		}
	}

	return MFX_ERR_NOT_FOUND;
}

mfxBitstream QSV_VPL_TASK::*GetTaskBitstream(int TaskID)
{
	return *TaskPool[TaskID].mfxBS;
}

mfxSyncPoint QSV_VPL_TASK::*GetTaskSyncPoint(int TaskID)
{
	return TaskPool[TaskID].syncp;
}

mfxStatus QSV_VPL_TASK::GetNextTaskIndex(int* TaskID)
{
	if (TaskPool != nullptr)
	{
		*TaskID = NextTaskID;
		return MFX_ERR_NONE;
	}

	return MFX_ERR_NOT_FOUND;
}

mfxStatus QSV_VPL_TASK::Reconfigure(mfxU16 PoolSize, mfxU32 BufferSize, mfxU32 Codec)
{
	if (PoolSize == 0 || BufferSize == 0 || Codec == 0)
	{
		return MFX_ERR_ABORTED;
	}

	Close();

	TaskPoolSize = PoolSize;
	BufferSize = BufferSize;
	TaskCodec = Codec;

	return Init(mfxU16 PoolSize, mfxU32 BufferSize, mfxU32 Codec);
}
