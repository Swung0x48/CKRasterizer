#include "CKDX12RingBufferBase.h"

void CKDX12RingBufferBase::FinishCurrentFrame(UINT64 FenceValue)
{
    m_CompletedFrameTails.emplace_back(FenceValue, m_Tail, m_CurrFrameSize);
    m_CurrFrameSize = 0;
}

void CKDX12RingBufferBase::ReleaseCompletedFrames(UINT64 CompletedFenceValue)
{
    // We can release all tails whose associated fence value is less
    // than or equal to CompletedFenceValue
    while (!m_CompletedFrameTails.empty() && m_CompletedFrameTails.front().FenceValue <= CompletedFenceValue)
    {
        const auto &OldestFrameTail = m_CompletedFrameTails.front();
        assert(OldestFrameTail.Size <= m_UsedSize);
        m_UsedSize -= OldestFrameTail.Size;
        m_Head = OldestFrameTail.Offset;
        m_CompletedFrameTails.pop_front();
    }
}

CKDX12RingBufferBase::OffsetType CKDX12RingBufferBase::Allocate(OffsetType Size)
{
    if (IsFull())
    {
        return InvalidOffset;
    }

    if (m_Tail >= m_Head)
    {
        //                     Head             Tail     MaxSize
        //                     |                |        |
        //  [                  xxxxxxxxxxxxxxxxx         ]
        //
        //
        if (m_Tail + Size <= m_MaxSize)
        {
            auto Offset = m_Tail;
            m_Tail += Size;
            m_UsedSize += Size;
            m_CurrFrameSize += Size;
            return Offset;
        }
        else if (Size <= m_Head)
        {
            // Allocate from the beginning of the buffer
            OffsetType AddSize = (m_MaxSize - m_Tail) + Size;
            m_UsedSize += AddSize;
            m_CurrFrameSize += AddSize;
            m_Tail = Size;
            return 0;
        }
    }
    else if (m_Tail + Size <= m_Head)
    {
        //
        //       Tail          Head
        //       |             |
        //  [xxxx              xxxxxxxxxxxxxxxxxxxxxxxxxx]
        //
        auto Offset = m_Tail;
        m_Tail += Size;
        m_UsedSize += Size;
        m_CurrFrameSize += Size;
        return Offset;
    }

    return InvalidOffset;
}