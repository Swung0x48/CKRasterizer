#ifndef CKDX12RINGBUFFERBASE_H
#define CKDX12RINGBUFFERBASE_H

#include <deque>

#include "CKDX12RasterizerCommon.h"
class CKDX12RingBufferBase
{
public:
    using OffsetType = size_t;
    struct FrameTailAttribs
    {
        FrameTailAttribs(UINT64 fenceValue, OffsetType offset, OffsetType size) : FenceValue(fenceValue), Offset(offset), Size(size) {}
        // Fence value associated with the command list in which
        // the allocation could have been referenced last time
        UINT64 FenceValue;
        OffsetType Offset;
        OffsetType Size;
    };
    static const OffsetType InvalidOffset = static_cast<OffsetType>(-1);

    CKDX12RingBufferBase(OffsetType MaxSize) noexcept: m_MaxSize(MaxSize) {}
    CKDX12RingBufferBase(CKDX12RingBufferBase &&rhs) noexcept = default;
    CKDX12RingBufferBase &operator=(CKDX12RingBufferBase &&rhs) noexcept = default;

    CKDX12RingBufferBase(const CKDX12RingBufferBase &) = delete;
    CKDX12RingBufferBase &operator=(const CKDX12RingBufferBase &) = delete;

    ~CKDX12RingBufferBase() = default;

    OffsetType Allocate(OffsetType Size);

    void FinishCurrentFrame(UINT64 FenceValue);
    void ReleaseCompletedFrames(UINT64 CompletedFenceValue);

    OffsetType GetMaxSize() const { return m_MaxSize; }
    bool IsFull() const { return m_UsedSize == m_MaxSize; };
    bool IsEmpty() const { return m_UsedSize == 0; };
    OffsetType GetUsedSize() const { return m_UsedSize; }

private:
    std::deque<FrameTailAttribs> m_CompletedFrameTails;
    OffsetType m_Head = 0;
    OffsetType m_Tail = 0;
    OffsetType m_MaxSize = 0;
    OffsetType m_UsedSize = 0;
    OffsetType m_CurrFrameSize = 0;
};

#endif // CKDX12RINGBUFFERBASE_H
