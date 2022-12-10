#if !defined(CKLIGHT_H) || defined(CK_3DIMPLEMENTATION)
#define CKLIGHT_H
#ifndef CK_3DIMPLEMENTATION

#include "CK3dEntity.h"
#include "VxColor.h"
#include "VxColor.h"

/*************************************************
{filename:CKLight}
Summary: Class for lights

Remarks:
    + The CKLight class is derived from CK3dEntity.

    + A CKLight can be a spot, directional light
    or point of light.

    + A range can be given outside which objects won't receive any contribution
    from this light.

    + Spot lights are controlled by three additional parameters which are the angle
    of the inner cone (SetHotSpot), outer cone  (SetFallOff) and a factor that controls the
    decrease of intensity between the inner and outer cone limits.

    + The Class Identifier of a light is CKCID_LIGHT

See also: CKTargetLight,Using Materials,Using Lights
*************************************************/
class CKLight : public CK3dEntity
{
public:
#endif

    /************************************************
    Summary: Sets the light color.
    Arguments:
        c: A reference to a VxColor that contains the new light color.
    Remarks:

    See also: GetColor
    ************************************************/
    virtual void SetColor(const VxColor &c) = 0;
    /************************************************
    Summary: Returns the light color.

    Return Value:
        A VxColor reference to the color of the light
    See also: SetColor
    ************************************************/
    virtual const VxColor &GetColor() = 0;

    /************************************************
    Summary: Sets the light constant attenuation.
    Arguments:
        Value: Constant attenuation.
    Remarks:

    See also: SetLinearAttenuation,SetQuadraticAttenuation
    ************************************************/
    virtual void SetConstantAttenuation(float Value) = 0;
    /************************************************
    Summary: Sets the light linear attenuation.
    Arguments:
        Value: Constant attenuation.
    Remarks:

    See also: SetLinearAttenuation,SetQuadraticAttenuation
    ************************************************/
    virtual void SetLinearAttenuation(float Value) = 0;
    /************************************************
    Summary: Sets the light quadratic attenuation.
    Arguments:
        Value: Quadratic attenuation.
    Remarks:

    See also: SetConstantAttenuation,SetLinearAttenuation
    ************************************************/
    virtual void SetQuadraticAttenuation(float Value) = 0;
    /************************************************
    Summary: Gets the light constant attenuation factor.
    Return Value: Constant attenuation factor.
    Remarks:

    See also: SetConstantAttenuation,GetLinearAttenuation,GetQuadraticAttenuation
    ************************************************/
    virtual float GetConstantAttenuation() = 0;
    /************************************************
    Summary: Gets the light linear attenuation factor.
    Return Value: Linear attenuation factor.
    Remarks:

    See also: SetLinearAttenuation,GetQuadraticAttenuation
    ************************************************/
    virtual float GetLinearAttenuation() = 0;
    /************************************************
    Summary: Gets the light Quadratic attenuation factor.
    Return Value: Quadratic attenuation factor.
    Remarks:

    See also: GetLinearAttenuation,SetQuadraticAttenuation
    ************************************************/
    virtual float GetQuadraticAttenuation() = 0;

    //---------------------------------------------------------
    // TYPE

    /************************************************
    Summary: Gets the type of a light (point,spot,directional).
    Return Value: A VXLIGHT_TYPE type.
    Remarks:

    See also: SetType, RCKLight
    ************************************************/
    virtual VXLIGHT_TYPE GetType() = 0;
    /************************************************
    Summary: Sets the type of a light (point,spot,directional)
    Arguments:
        Type: A VXLIGHT_TYPE type.
    Remarks:

    See also: GetType
    ************************************************/
    virtual void SetType(VXLIGHT_TYPE Type) = 0;

    //--------------------------------------------------------
    // RANGE

    /************************************************
    Summary: Gets the light range.
    Return Value: Range of the light.
    Remarks:
        + The range is the distance beyond which the light has no effect and is only valid
    for spot and point lights.

    See also: SetRange
    ************************************************/
    virtual float GetRange() = 0;
    /************************************************
    Summary: Sets the light range.
    Arguments:
        Value: New range of the light.
    Remarks:
        + The range is the distance beyond which the light has no effect and is only valid
        for spot and point lights.

    See also: GetRange
    ************************************************/
    virtual void SetRange(float Value) = 0;

    //--------------------------------------------------------
    // SPOTLIGHT OPTIONS

    /************************************************
    Summary: Gets the angle of the inner cone for a spot.
    Return value: Inner cone angle.
    Remarks:

    See also: SetHotSpot, GetFallOff,GetFallOffShape
    ************************************************/
    virtual float GetHotSpot() = 0;

    /************************************************
    Summary: Gets the angle of the outer cone for a spot.
    Return value: Outer cone angle.
    Remarks:

    See also: SetFallOff, GetHotSpot,GetFallOffShape
    ************************************************/
    virtual float GetFallOff() = 0;

    /************************************************
    Summary: Sets the angle of the inner cone for a spot.
    Arguments:
        value: Inner cone angle in radian.
    Remarks:

    See also: GetHotSpot, SetFallOffShape,SetFallOff
    ************************************************/
    virtual void SetHotSpot(float Value) = 0;

    /************************************************
    Summary: Sets the angle of the outer cone for a spot.
    Arguments:
        value: Outer cone angle in radian.
    Remarks:

    See also: GetFallOff,SetFallOffShape,SetHotSpot
    ************************************************/
    virtual void SetFallOff(float Value) = 0;

    /************************************************
    Summary: Gets the shape between inner and outer cone.
    Return Value:
        A float value giving the interpolation factor between inner and outer cone.

    See also: SetFallOffShape,GetFallOff,GetHotSpot
    ************************************************/
    virtual float GetFallOffShape() = 0;

    /************************************************
    Summary: Sets the FallOff Shape of the light.
    Arguments:
        Value: FallOff Shape of the light.
    Remarks:

    See also: GetFallOffShape,SetFallOff,SetHotSpot
    ************************************************/
    virtual void SetFallOffShape(float Value) = 0;

    //--------------------------------------------------
    // ACTIVITY OPTIONS

    /************************************************
    Summary: Activates the light
    Arguments:
        Active: TRUE enables the light, FALSE disables it.
    Remarks:
        + Hiding a light (CKObject::Show) also disables it.

    See also: GetActivity,SetSpecularFlag
    ************************************************/
    virtual void Active(CKBOOL Active) = 0;

    /************************************************
    Summary: Gets the light activity status.
    Return Value: TRUE if light is active, FALSE otherwise.
    Remarks:

    See also: Active
    ************************************************/
    virtual CKBOOL GetActivity() = 0;

    /************************************************
    Summary: Enables or disables specular highlights.
    Arguments:
        Specular: TRUE to activate specular highlights.
    Remarks:
        + Specular highlights require more time to be computed so you
        should avoid them whenever possible if you are looking for maximum
        performances.

    See also: GetSpecularFlag
    ************************************************/
    virtual void SetSpecularFlag(CKBOOL Specular) = 0;

    /************************************************
    Summary: Returns if specular highlights are enabled.
    Return Value: TRUE if the light produces specular highlights, FALSE otherwise.
    Remarks:
        + Specular highlights require more time to be computed so you
        should avoid them whenever possible if you are looking for maximum
        performances.

    See also: SetSpecularFlag
    ************************************************/
    virtual CKBOOL GetSpecularFlag() = 0;

    //-------------------------------------------
    // Target Access

    /************************************************
    Summary: Gets the target of the light
    Return Value: A pointer to the target CK3dEntity.
    Remarks:
        + This method always returns NULL for CKLights, but is available on CKTargetLight.

    See also: SetTarget
    ************************************************/
    virtual CK3dEntity *GetTarget() = 0;

    /************************************************
    Summary: Sets the target of the light
    Arguments:
        target: A pointer to CK3DEntity to be set as target.
    Remarks:
        + This method does nothing for CKLights as they not have targets but is available
        on CKTargetLight.

    See also: GetTarget
    ************************************************/
    virtual void SetTarget(CK3dEntity *target) = 0;

    //-------------------------------------------
    // Target Access

    /************************************************
    Summary: Gets the multiplication power  of the light
    Return Value: A pointer to the target CK3dEntity.
    Remarks:
        o This method was introduced in version 2.1
        o The Light color is multiplied by this float when used during rendering.
        o The light power is a float which can take any value including negative ones. Its default
        value is to be 1.0f which sets the light original color but it can be use to attenuate or extrapolate
        the global influence of the light without changing its diffuse color.

    See also: SetLightPower
    ************************************************/
    virtual float GetLightPower() = 0;

    /************************************************
    Summary: Sets the multiplication power  of the light
    Arguments:
        target: A pointer to CK3DEntity to be set as target.
    Remarks:
        o This method was introduced in version 2.1
        o The Light color is multiplied by this float when used during rendering.
        o The light power is a float which can take any value including negative ones. Its default
        value is to be 1.0f which sets the light original color but it can be use to attenuate or extrapolate
        the global influence of the light without changing its diffuse color.

    See also: GetLightPower
    ************************************************/
    virtual void SetLightPower(float power = 1.0f) = 0;

    // Dynamic Cast method (returns NULL if the object can't be cast)
    static CKLight *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_LIGHT) ? (CKLight *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKLIGHT_H
