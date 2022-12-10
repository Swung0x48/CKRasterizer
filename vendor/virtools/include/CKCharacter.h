#if !defined(CKCHARACTER_H) || defined(CK_3DIMPLEMENTATION)
#define CKCHARACTER_H
#ifndef CK_3DIMPLEMENTATION

#include "CK3dEntity.h"

/*************************************************
{filename:CKCharacter}
Name: CKCharacter

Summary: Representation of character

Remarks:
{Image:Character}
+ A CKCharacter is defined as an articulated hierarchical set of body-parts.A character must be a hierarchy of object (CKBodyPart) with an unique root object giving position and orientation
(optional for orientation).

+ A character may have an object called Floor referencer which is used (if present)
by behaviors to make the characters detect floors. This object should be a child of
the root and contain the keywords ""Pas"","Foot" or "FloorRef" in its name to be automatically recognized
otherwise use SetFloorReferenceObject to precise a custom object.

+ It can have animations attached and provides default functionalities to
handle these animations and their transitions. This behavior can be override by calling
SetAutomaticProcess(FALSE). The processing of animations on characters occurs each frame before the
processing of behaviors. The automatic processing of animation plays the active animation until its
a new animation is set to be played using SetNextActiveAnimation, when a new animation becomes active
any kind of transitions can be made (smooth,to the start of the next animation,break,etc...)

+ In addition to the active animation, secondary animations may be played concurrently.
If there are body-parts that are used by both animations the secondary animations have
priority on the currently active animation.

+ The objects making a character are called Body-parts (CKBodyPart which derives from CK3dObject). While names
may be pure convenience with other objects, Body-parts name are used by loaders to detect to which particular object
an animation should be attributed. If there is a body-part that should not be modify by the character animation (for
procedural animation of the head for example), it can be disabled by using CK3dEntity::IgnoreAnimations()

+ As a child class of CKBeObject and CK3dEntity, CKCharacter inherits all their methods for
placement and behaviors gestation.

+ The class id of CKCharacter is CKCID_CHARACTER.


See also: Using Characters,Using Animations,CKAnimation,CKKeyedAnimation,CKKinematicChain
*************************************************/
class CKCharacter : public CK3dEntity
{
public:
#endif
    //-----------------------------------------------------
    // BodyParts

    /************************************************
    Summary: Adds a body-part to this character.
    Arguments:
        part : A pointer to the CKBodyPart to be added.
    Return Value: CK_OK if successful or CKERR_ALREADYPRESENT if part is already a member of this character.

    See also: RemoveBodyPart,GetRootBodyPart,GetBodyPartCount,GetBodyPart
    ************************************************/
    virtual CKERROR AddBodyPart(CKBodyPart *part) = 0;

    /************************************************
    Summary: Removes a body-part from this character.
    Arguments:
        part : A pointer to the CKBodyPart to be removed
    Return Value: CK_OK if successful,CKERR_INVALIDPARAMETER otherwise.

    See also: AddBodyPart,GetRootBodyPart,GetBodyPartCount,GetBodyPart
    ************************************************/
    virtual CKERROR RemoveBodyPart(CKBodyPart *part) = 0;

    /************************************************
    Summary: Gets the root body part.
    Return value :A CKBodyPart pointer.
    Remarks:
        + A character must always have only one root object which will
        be used for the movement of the character.

    See also: AddBodyPart,RemoveBodyPart,GetBodyPartCount,GetBodyPart
    ************************************************/
    virtual CKBodyPart *GetRootBodyPart() = 0;

    /************************************************
    Summary: Forces the character to use a specific body-part as its RootBodyPart
    Arguments:
        part:A pointer to a CKBodyPart that should be set as the root body part.
    Return Value: CK_OK if successful, error code otherwise.
    Remarks:
    + When adding animations and body-parts to a character the root body-part is
    automatically detected
    + A character must always have only one root object which will
    be used for the movement of the character.

    See also: GetRootBodyPart,AddBodyPart,RemoveBodyPart,GetBodyPartCount,GetBodyPart
    ************************************************/
    virtual CKERROR SetRootBodyPart(CKBodyPart *part) = 0;

    /************************************************
    Summary: Gets a body-part at a given index.
    Arguments:
        index : Index of the body-part to be obtained.
    Return Value: A pointer to the index body-part of the character.

    See also: GetRootBodyPart,AddBodyPart,RemoveBodyPart,GetBodyPartCount
    ************************************************/
    virtual CKBodyPart *GetBodyPart(int index) = 0;

    /************************************************
    Summary: Gets the number of body-parts of this character.
    Return Value: The number of body-parts.

    See also: GetRootBodyPart,AddBodyPart,RemoveBodyPart,GetBodyPart
    ************************************************/
    virtual int GetBodyPartCount() = 0;

    //-----------------------------------------------------
    // Animations

    /*****************************************************
    Summary: Adds an animation to the character
    Arguments:
        anim: A CKAnimation pointer to be added for the character.
    Return Value: CK_OK if successful, CKERR_INVALIDPARAMETER otherwise.

    See also: RemoveAnimation,GetAnimation,GetAnimationCount
    *******************************************************/
    virtual CKERROR AddAnimation(CKAnimation *anim) = 0;

    /*******************************************************
    Summary: Removes an animation from the character
    Arguments:
        anim: A CKAnimation pointer to be removed from the character.
    Return Value: CK_OK if successful, CKERR_INVALIDPARAMETER otherwise

    See also: AddAnimation,GetAnimation,GetAnimationCount
    *******************************************************/
    virtual CKERROR RemoveAnimation(CKAnimation *anim) = 0;

    /************************************************
    Summary: Gets a specific animation given its index.
    Arguments:
        index : Index of the animation to be returned.
    Return Value: A CKAnimation pointer.

    See also: AddAnimation,GetAnimation,GetAnimationCount
    ************************************************/
    virtual CKAnimation *GetAnimation(int index) = 0;

    /************************************************
    Summary: Gets the number of animations in this character.
    Return Value: The number of animations.

    See also: RemoveAnimation,GetAnimation,GetAnimationCount
    ************************************************/
    virtual int GetAnimationCount() = 0;

    /************************************************
    Summary: Gets the animation used to make transitions.
    Return Value: A pointer to the animation used for transitions.
    Remarks:
    + A transition animation is always created along with a character.
    + Its parameters will always be updated each time a new transition is needed and
    can be retrieved with GetWarperParameters method.

    See also: AddAnimation,RemoveAnimation,GetAnimation,GetAnimationCount,GetWarperParameters
    ************************************************/
    virtual CKAnimation *GetWarper() = 0;

    //-----------------------------------------------------
    // Playing Animations

    /************************************************
    Summary: Gets the animation being played.
    Return Value: A pointer to the CKAnimation being played.

    See also: SetActiveAnimation,SetNextActiveAnimation
    ************************************************/
    virtual CKAnimation *GetActiveAnimation() = 0;

    /************************************************
    Summary: Gets the animation that will be played next.
    Return Value: A pointer to the next active CKAnimation.

    See also: GetActiveAnimation,SetActiveAnimation,SetNextActiveAnimation
    ************************************************/
    virtual CKAnimation *GetNextActiveAnimation() = 0;

    /************************************************
    Summary: Forces the current active animation

    Return Value:
        CK_OK
    Arguments:
        anim : A pointer to a CKAnimation to be set as current active animation.
    Remarks:
        + This method changes the current active animation without
        any transition.
    See also: GetActiveAnimation,GetNextActiveAnimation,SetNextActiveAnimation
    ************************************************/
    virtual CKERROR SetActiveAnimation(CKAnimation *anim) = 0;

    /*************************************************
    Summary: Sets the next animation to be played by the character

    Arguments:
      anim: A pointer to the next CKAnimation to be played.
        transitionmode : A CKDWORD containing a combination of CK_ANIMATION_TRANSITION_MODE
        warplength : length in frame of the warping animation if there is one.

    Return Value:
            CK_OK if successful, CKERR_INVALIDPARAMETER otherwise
    Remarks:
    Depending on transitionmode value, this function may :

    + Create a transition animation between current position and next animation which will be played before playing anim.
    + Set anim as Active Animation immediately (break).
    + Continue playing current animation in loop.

    if transitionmode is CK_TRANSITION_FROMANIMATION then the transition mode will be taken from the
    animation parameters (set with CKAnimation::SetTransitionMode
    The UnlimittedController building block in the Character category is good sample to see the different usage of transition mode.
    Examples:
    // If the current active animation is anim then the function returns immediately otherwise
    // if the current active animation can be stopped then a transition animation will be created
    // which will be 5 frames long then anim will be played from the frame to which the positions of
    // the character and its body-parts is the closest match to the current ones.
        SetNextActiveAnimation(anim,CK_TRANSITION_LOOPIFEQUAL|CK_TRANSITION_USEVELOCITY|CK_TRANSITION_WARPBEST,5.0f);

    // If the current active animation is anim then the function returns immediately otherwise
    // if the current active animation can be stopped then a transition animation will be created
    // which will be 5 frames long then anim will be played from the start.
        SetNextActiveAnimation(anim,CK_TRANSITION_LOOPIFEQUAL|CK_TRANSITION_USEVELOCITY|CK_TRANSITION_WARPSTART,5.0f);

    See also: GetActiveAnimation,SetActiveAnimation,CK_ANIMATION_TRANSITION_MODE
    *************************************************/
    virtual CKERROR SetNextActiveAnimation(CKAnimation *anim, CKDWORD transitionmode, float warplength = 0.0f) = 0;

    //-----------------------------------------------------
    // animation processing

    /************************************************
    Summary: Updates the character according to current active animation and secondary animations.
    Arguments:
        deltat : a float value giving the elapsed time since last process loop in milliseconds.
    Remarks:
        + This method is automatically called by the framework on each active character before
        the process loop.

    See also: SetAutomaticProcess, SetActiveAnimation, SetNextActiveAnimation
    ************************************************/
    virtual void ProcessAnimation(float deltat = 1.0f) = 0;

    /************************************************
    Summary: Sets whether character animations are processed by the engine.
    Arguments:
        process: FALSE to prevent animations from being processed by the engine each frame.
    Remarks:

    See also: IsAutomaticProcess, ProcessAnimation
    ************************************************/
    virtual void SetAutomaticProcess(CKBOOL process = TRUE) = 0;

    /************************************************
    Summary: Checks whether animations are processed by the engine.
    Return Value: TRUE if animations are processed by the engine.

    See also: SetAutomaticProcess, ProcessAnimation
    ************************************************/
    virtual CKBOOL IsAutomaticProcess() = 0;

    /************************************************
    Summary: Gets the velocity vector of the character.
    Arguments:
        deltat: Time delay in milliseconds.
        velocity: A pointer to a VxVector that will be filled with the velocity vector of the character.
    Remarks:
        + The velocity will be filled with the estimated movement made by the character
        in deltat milliseconds.

    See Also:CKTimeManager
    ************************************************/
    virtual void GetEstimatedVelocity(float deltat, VxVector *velocity) = 0;

    //-----------------------------------------------------
    // Secondary Animations

    /************************************************
    Summary: Plays a secondary animation.
    Arguments:
        anim: A pointer to the CKAnimation to start playing.
        StartingFrame: Frame to starts playing anim from.
        PlayFlags:	Play options (CK_SECONDARYANIMATION_FLAGS)
        warplength:	If play options contain CKSECONDARYANIMATION_DOWARP, length in frames of the transition animation.
        LoopCount:	If play options specifies CKSECONDARYANIMATION_LOOPNTIMES, number of loops.
    Return Value: CK_OK if successful or an error code.
    Remarks:
        + The secondary animations have priority over primary animations, they force the movement of
        bodyparts that they referenced. Most of the time these animations will be specific to parts of
        the characters (arms for example) and should not referenced the root body-part as its the object
        that gives the character movement.

    See also: StopSecondaryAnimation
    ************************************************/
    virtual CKERROR PlaySecondaryAnimation(CKAnimation *anim, float StartingFrame = 0.0f, CK_SECONDARYANIMATION_FLAGS PlayFlags = CKSECONDARYANIMATION_ONESHOT, float warplength = 5.0f, int LoopCount = 0) = 0;

    /************************************************
    Summary: Stops playing a secondary animation.
    Arguments:
        anim : A pointer to the CKAnimation to stop.
        warp: A boolean indication whether a transition should be made to stop current animation.
        warplength: If warp is TRUE :  duration in frames of the transition, irrelevant otherwise.
    Return Value: CK_OK if successful,CKERR_INVALIDPARAMETER if the animation doesn't belong to the character.

    See also: PlaySecondaryAnimation
    ************************************************/
    virtual CKERROR StopSecondaryAnimation(CKAnimation *anim, CKBOOL warp = FALSE, float warplength = 5.0f) = 0;
    virtual CKERROR StopSecondaryAnimation(CKAnimation *anim, float warplength = 5.0f) = 0;

    /************************************************
    Summary: Gets the number of secondary animations currently playing on this character.
    Return Value: Number of secondary animations.

    See also: PlaySecondaryAnimation,GetSecondaryAnimationCount, StopSecondaryAnimation
    ************************************************/
    virtual int GetSecondaryAnimationsCount() = 0;

    /************************************************
    Summary: Gets a secondary animation according to its index.
    Arguments:
        index : Index of the animation to be obtained.
    Return Value: A pointer to a CKAnimation.

    See also: PlaySecondaryAnimation, GetSecondaryAnimationCount, StopSecondaryAnimation
    ************************************************/
    virtual CKAnimation *GetSecondaryAnimation(int index) = 0;

    /************************************************
    Summary: Removes all secondary animations.
    Remarks:
        + Stops and removes all secondary animations of a character.

    See also: PlaySecondaryAnimation, StopSecondaryAnimation
    ************************************************/
    virtual void FlushSecondaryAnimations() = 0;

    /************************************************
    Summary: Aligns character position and orientation with its root
    Remarks:
        + This method is automatically called when automatic process
        of the animations is on to align the referential of the character
        with its root body-part

    See also: GetRootBodyPart, ProcessAnimation, SetAutomaticProcess
    ************************************************/
    virtual void AlignCharacterWithRootPosition() = 0;

    /************************************************
    Summary: Gets the entity used to represent floor reference.
    Return Value: A pointer to a CK3dEntity used as floor reference.
    Remarks:
        + The floor reference object is used to identify the position of the character
        relatively to a floor.

    See also: GetRootBodyPart, SetFloorReferenceObject.
    ************************************************/
    virtual CK3dEntity *GetFloorReferenceObject() = 0;

    /************************************************
    Summary: Sets the entity used to represent floor reference.
    Arguments:
        FloorRef : A pointer to a CK3dEntity used as floor reference.
    Remarks:
        + The floor reference object is used to identify the position of the character
        relatively to a floor.

    See also: GetRootBodyPart, GetFloorReferenceObject
    ************************************************/
    virtual void SetFloorReferenceObject(CK3dEntity *FloorRef) = 0;

    /************************************************
    Summary: Sets the level of detail for animation processing.
    Arguments:
        LOD : A float value between 0..1 giving the animations level of detail.
    Remarks:
    + A character that is far from the viewpoint does not need all body-parts
    animations to be processed.
    + The animations of the body-parts are sorted according to their importance in (contribution to)
    the global animation of the character. You can use this level of detail to remove
    the processing of the less important body-parts animations.
    + At the minimum level of detail (0) only the root and floor reference object animations
    are processed (character movement) , at the full level of detail (1.0f)
    all the body-parts animations are processed.

    See also: GetRootBodyPart, GetFloorReferenceObject
    ************************************************/
    virtual void SetAnimationLevelOfDetail(float LOD) = 0;

    virtual float GetAnimationLevelOfDetail() = 0;

    /************************************************
    Summary: Gets the parameters that were used to create the warping animation.
    Arguments:
        TransitionMode : A pointer to a DWORD that will be filled with the transition mode (CK_ANIMATION_TRANSITION_MODE)
        used to create the warping animation.
        AnimSrc : Source animation
        AnimDest : Destination animation
        FrameSrc: Frame index in the source animation that was used as starting point for the warper.
        FrameDest: Frame index in the destination animation that was used as ending point for the warper.
    Remarks:
        o This method retrieves the parameters that were used to create the last generated
        transition animation (warper).

    See also: GetWarper
    ************************************************/
    virtual void GetWarperParameters(CKDWORD *TransitionMode, CKAnimation **AnimSrc, float *FrameSrc, CKAnimation **AnimDest, float *FrameDest) = 0;

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
    static CKCharacter *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_CHARACTER) ? (CKCharacter *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKCHARACTER_H
