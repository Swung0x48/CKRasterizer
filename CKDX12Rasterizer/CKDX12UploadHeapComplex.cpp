#include "CKDX12UploadHeapComplex.h"

CKDX12AllocatedResource CKDX12UploadHeapComplex::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
{
    auto staging_alloc = m_StagingHeap.Allocate(SizeInBytes, Alignment);
    auto default_alloc = m_DefaultHeap.Allocate(SizeInBytes, Alignment);
    // TODO...
    //CKDX12AllocatedResource alloc;
    return staging_alloc;
}

void CKDX12UploadHeapComplex::FinishFrame(UINT64 nextFenceValue, UINT64 lastCompletedFenceValue)
{

}