#if !defined(CKBODYPART_H) || defined(CK_3DIMPLEMENTATION)
#define CKBODYPART_H
#ifndef CK_3DIMPLEMENTATION

#include "CK3dObject.h"
#include "CKAnimation.h"

#define CKIKJOINTLIMIT(axe) (CK_IKJOINT_LIMIT_X << (axe - 1))
#define CKIKJOINTACTIVE(axe) (CK_IKJOINT_ACTIVE_X << (axe - 1))
#define CKIKJOINTEASE(axe) (CK_IKJOINT_EASE_X << (axe - 1))

typedef enum CK_IKJOINT_FLAGS
{
    CK_IKJOINT_ACTIVE_X = 0x001,
    CK_IKJOINT_ACTIVE_Y = 0x002,
    CK_IKJOINT_ACTIVE_Z = 0x004,
    CK_IKJOINT_ACTIVE   = 0x007,

    CK_IKJOINT_LIMIT_X  = 0x010,
    CK_IKJOINT_LIMIT_Y  = 0x020,
    CK_IKJOINT_LIMIT_Z  = 0x040,
    CK_IKJOINT_LIMIT    = 0x070,

    CK_IKJOINT_EASE_X   = 0x100,
    CK_IKJOINT_EASE_Y   = 0x200,
    CK_IKJOINT_EASE_Z   = 0x400,
    CK_IKJOINT_EASE     = 0x700
} CK_IKJOINT_FLAGS;

/******************************************************
Name: CKIKJoint

Summary: Inverse Kinematic Joint Description

Structure describing joint angle activation and limits



See also: CKBodyPart,CKBodyPart::SetRotationJoint
*******************************************************/
typedef struct CKIkJoint
{
    CKDWORD m_Flags;		// CK_IKJOINT_FLAGS
    VxVector m_Min;		// Minimum range if rotations are limited
    VxVector m_Max;		// Maximum range if rotations are limited
    VxVector m_Damping; // damping factor for each axis
} CKIkJoint;

/*************************************************
{filename:CKBodyPart}
Name: CKBodyPart
Summary: Class for 3D objects being part of a character.

Remarks:
    + The CKBodyPart class is designed for characters parts. In addition to the virtual
    base class CK3dObject it provides functions to retrieve the character a body-part is part of, and
    functions to specified the IK joint data.

    + The class id of CKBodyPart is CKCID_BODYPART.
See also: CK3dObject,CKCharacter::AddBodyPart()
*************************************************/
class CKBodyPart : public CK3dObject
{
public:
#endif

    /*************************************************
    Summary: Returns the character to which this body-part belongs to.
    Return Value:
        A pointer to the owner CKCharacter.
    See also: CKCharacter
    *************************************************/
    virtual CKCharacter *GetCharacter() const = 0;

    /*************************************************
    Summary: Sets the only animation that can modify this body-part.

    Arguments:
        anim: A pointer to the new exclusive CKAnimation or NULL to remove any exclusive animation.
    Remarks:
        + Since a character can have several animations playing at the same moment, some animations
        may refer to the same body-part. This function forces one of these animations to be the only one
        that can modify an object.
    See Also:CKAnimation
    *************************************************/
    virtual void SetExclusiveAnimation(const CKAnimation *anim) = 0;

    virtual CKAnimation *GetExclusiveAnimation() const = 0;

    /*************************************************
    Summary: Gets the IK rotation joint data
    Remarks:
    + The CKIkJoint structure contains the axis activation and limit angles
    used when solving IK along a kinematic chain.
    + This information is only valid if the body-part has the flags CK_3DENTITY_IKJOINTVALID
    See Also : SetRotationJoint,CKIkJoint,CKKinematicChain,CK3dEntity::GetFlags
    *************************************************/
    virtual void GetRotationJoint(CKIkJoint *rotjoint) const = 0;

    /*************************************************
    Summary: Sets the rotation joint IK data
    Remarks:
    + The CKIkJoint structure contains the axis activation and limit angles
    used when solving IK along a kinematic chain.
    + This information is only used when the body-part has the flags CK_3DENTITY_IKJOINTVALID
    See Also : GetRotationJoint,CKIkJoint,CKKinematicChain
    *************************************************/
    virtual void SetRotationJoint(const CKIkJoint *rotjoint) = 0;

    /*************************************************
    Summary: Ensures the body-part fits its joint limits.
    Remarks:
        + If the rotation is valid and angle limits are given, this method ensure
        the body-part orientation is inside these limits.
    Return Value:
        CK_OK if successful, an error code otherwise.
    See Also:GetRotationJoint
    *************************************************/
    virtual CKERROR FitToJoint() = 0;

    /*************************************************
    Summary: Dynamic cast operator.
    Arguments:
        iO: A pointer to a CKObject to cast.
    Return Value:
        iO casted to the appropriate class or NULL if iO is not from the required class .
    Example:
          CKObject* Object;
          CKAnimation* anim = CKAnimation::Cast(Object);
    Remarks:

    *************************************************/
    static CKBodyPart *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_BODYPART) ? (CKBodyPart *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKBODYPART_H
