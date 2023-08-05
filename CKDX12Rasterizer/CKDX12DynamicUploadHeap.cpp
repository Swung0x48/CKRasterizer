#include "CKDX12DynamicUploadHeap.h"

CKDX12AllocatedResource CKDX12DynamicUploadHeap::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
{
    const size_t AlignmentMask = Alignment - 1;
    // Assert that it's a power of two.
    assert((AlignmentMask & Alignment) == 0);
    // Align the allocation
    const size_t AlignedSize = (SizeInBytes + AlignmentMask) & ~AlignmentMask;
    auto alloc = m_RingBuffers.back().Allocate(AlignedSize);
    if (!alloc.pBuffer)
    {
        // Create new buffer
        auto NewMaxSize = m_RingBuffers.back().GetMaxSize() * 2;
        // Make sure the buffer is large enough for the requested chunk
        while (NewMaxSize < SizeInBytes)
            NewMaxSize *= 2;
        m_RingBuffers.emplace_back(NewMaxSize, m_Device, m_IsCPUAccessible);
        alloc = m_RingBuffers.back().Allocate(AlignedSize);
    }
    return alloc;
}

void CKDX12DynamicUploadHeap::FinishFrame(UINT64 nextFenceValue, UINT64 lastCompletedFenceValue)
{
    size_t bufToDelete = 0;
    for (size_t i = 0; i < m_RingBuffers.size(); ++i)
    {
        auto &buffer = m_RingBuffers[i];
        buffer.FinishCurrentFrame(nextFenceValue);
        buffer.ReleaseCompletedFrames(lastCompletedFenceValue);
        if (bufToDelete == i && i < m_RingBuffers.size() - 1 && buffer.IsEmpty())
        {
            ++bufToDelete;
        }
    }

    if (bufToDelete)
        m_RingBuffers.erase(m_RingBuffers.begin(), m_RingBuffers.begin() + bufToDelete);
}