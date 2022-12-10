#ifndef CKSCENEOBJECTDESC_H
#define CKSCENEOBJECTDESC_H

#include "CKObject.h"

/////////////////////////////////////////////////////
// All private

class CKSceneObjectDesc
{
public:
    CKSceneObjectDesc()
    {
        m_Object = 0;
        m_InitialValue = NULL;
        m_Global = 0;
    };
    /////////////////////////////////////////
    // Virtual functions

    CKERROR ReadState(CKStateChunk *chunk);
    int GetSize();
    void Clear();
    void Init(CKObject *obj = NULL);

    CKDWORD ActiveAtStart() { return m_Flags & CK_SCENEOBJECT_START_ACTIVATE; }
    CKDWORD DeActiveAtStart() { return m_Flags & CK_SCENEOBJECT_START_DEACTIVATE; }
    CKDWORD NothingAtStart() { return m_Flags & CK_SCENEOBJECT_START_LEAVE; }
    CKDWORD ResetAtStart() { return m_Flags & CK_SCENEOBJECT_START_RESET; }
    CKDWORD IsActive() { return m_Flags & CK_SCENEOBJECT_ACTIVE; }

public:
    CK_ID m_Object;
    CKStateChunk *m_InitialValue;
    union
    {
        CKDWORD m_Global;
        CKDWORD m_Flags;
    };
};

#endif // CKSCENEOBJECTDESC_H
