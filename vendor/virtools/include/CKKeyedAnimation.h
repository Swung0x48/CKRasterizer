#if !defined(CKKEYEDANIMATION_H) || defined(CK_3DIMPLEMENTATION)
#define CKKEYEDANIMATION_H
#ifndef CK_3DIMPLEMENTATION

#include "CKAnimation.h"

/***********************************************************************
{filename:CKKeyedAnimation}
Name: CKKeyedAnimation
Summary: Group of object animations.
Remarks:
    + A KeyedAnimation is a group of CKObjectAnimation. Each of this CKObjectAnimation applies
    to a CKdEntity and contain keyframes (Time,Position,Orientation,Scale or Morph) that give the entity
    movement along time.

    + The class id of CKKeyedAnimation is CKCID_KEYEDANIMATION.



See also: CKObjectAnimation,CKCharacter
***************************************************************************/
class CKKeyedAnimation : public CKAnimation
{
public:
#endif

    //------------------------------------------------------------
    // Object Animations Elements

    /*************************************************
    Summary: Adds an Object Animation .
    Arguments:
        anim:  A Pointer to CKObjectAnimation to add.
    Return Value:
        CK_OK if successful, error code otherwise.
    See also: CKObjectAnimation,CKCharacter
    *************************************************/
    virtual CKERROR AddAnimation(CKObjectAnimation *anim) = 0;

    /*************************************************
    Summary: Removes an Object Animation.
    Arguments:
        anim:  A Pointer to the CKObjectAnimation to remove.
    Return Value:
        CK_OK if successful, error code otherwise.
    See also: AddAnimation,GetAnimationCount,GetAnimation
    *************************************************/
    virtual CKERROR RemoveAnimation(CKObjectAnimation *anim) = 0;

    /*************************************************
    Summary: Gets the number of Object Animations.
    Return Value:
        Number of ObjectAnimations that are currently available.
    See also: GetAnimation,AddAnimation
    *************************************************/
    virtual int GetAnimationCount() = 0;

    /*************************************************
    Summary: Retrieves an ObjectAnimation entry from its index or entity.
    Arguments:
        ent: A Pointer to 3d entity from which the ObjectAnimation is to be retrieve.
        index: Index of the ObjectAnimation to be obtained.
    Return Value:
        A Pointer to the CKObjectAnimation or NULL if none could be found.
    Remarks:
        + Object Animation info contains keyframes data such as Time,Position,Orientation
        and Scale.
    See also: GetAnimationCount,AddAnimation
    *************************************************/
    virtual CKObjectAnimation *GetAnimation(CK3dEntity *ent) = 0;
    virtual CKObjectAnimation *GetAnimation(int index) = 0;

    /*************************************************
    Summary: Removes all ObjectAnimations.
    See also: RemoveAnimation
    *************************************************/
    virtual void Clear() = 0;

    // Dynamic Cast method (returns NULL if the object can't be cast)
    static CKKeyedAnimation *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_KEYEDANIMATION) ? (CKKeyedAnimation *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKKEYEDANIMATION_H
