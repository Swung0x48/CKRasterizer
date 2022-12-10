#if !defined(CKCURVE_H) || defined(CK_3DIMPLEMENTATION)
#define CKCURVE_H
#ifndef CK_3DIMPLEMENTATION

#include "CK3dEntity.h"
#include "VxColor.h"

/*************************************************
{filename:CKCurve}
Summary: Representation of a 3D curve

Remarks:
{image:curve}
+ The CKCurve class is derivated from CK3dEntity. It adds functions
create spline curves made up of CKCurvePoint (control points).

+ Each control points defines where the curve should pass and how it
acts at this point (incoming and outgoing tangents  or TCB parameters)

+ Its class id is CKCID_CURVE

See also: CKCurvePoint
*************************************************/
class CKCurve : public CK3dEntity
{
public:
#endif
    //------------------------------------
    // Length

    /************************************************
    Summary: Returns the length of the curve
    Return value: Length of the curve
    Remarks:
        + The curve length is updated every time a curve point moves.

    See also: SetLength
    ************************************************/
    virtual float GetLength() = 0;

    //------------------------------
    // Open/Closed Curves

    /************************************************
    Summary: Sets the curve as opened .

    See also: IsOpen,Close
    ************************************************/
    virtual void Open() = 0;

    /************************************************
    Summary: Sets the curve as closed .

    See also: IsOpen,Open
    ************************************************/
    virtual void Close() = 0;

    /************************************************
    Summary: Checks the curve as opened or closed.
    Return Value: TRUE if the curve is opened.

    See also: Close, Open
    ************************************************/
    virtual CKBOOL IsOpen() = 0;

    //----------------------------------------
    // Get Point position in the world referential

    /************************************************
    Summary: Gets a position along the curve.
    Arguments:
        step: A float coefficient between 0 and 1 indicating position along the curve.
        pos: A pointer to a VxVector which will be filled with position on the curve at specified step.
        dir: A optional pointer to a VxVector which will contain the direction of the curve at the specified step.
    Return Value: CK_OK if successful or an error code otherwise.
    Remarks:
        + The returned position is given in the world referential.

    See also: GetLocalPos
    ************************************************/
    virtual CKERROR GetPos(float step, VxVector *pos, VxVector *dir = NULL) = 0;

    /************************************************
    Summary: Gets a local position along the curve.

    Arguments:
        step: A float coefficient between 0 and 1 indicating position along the curve.
        pos: A pointer to a VxVector which will be filled with position on the curve at specified step.
        dir: A optional pointer to a VxVector which will contain the direction of the curve at the specified step.

    Return Value: CK_OK if successful or an error code otherwise.
    Remarks:
        + The returned position is given in the curve referential

    See also: GetPos
    ************************************************/
    virtual CKERROR GetLocalPos(float step, VxVector *pos, VxVector *dir = NULL) = 0;

    //---------------------------------------
    // Control points

    /************************************************
    Summary: Gets tangents to a control point
    Arguments:
        index: Index of the control point to get the tangents of.
        pt: A pointer to the CKCurvePoint to get the tangents of.
        in:  A pointer to a VxVector containing incoming tangent.
        out: A pointer to a VxVector containing outgoing tangent.
    Return Value: CK_OK if successful,an error code otherwise.

    See also: SetTangents
    ************************************************/
    virtual CKERROR GetTangents(int index, VxVector *in, VxVector *out) = 0;
    virtual CKERROR GetTangents(CKCurvePoint *pt, VxVector *in, VxVector *out) = 0;

    /************************************************
    Summary: Sets tangents to a control point
    Arguments:
        index: Index of the control point to get the tangents of.
        pt: A pointer to the CKCurvePoint to get the tangents of.
        in:  A pointer to a VxVector containing incoming tangent.
        out: A pointer to a VxVector containing outgoing tangent.

    Return Value: CK_OK if successful,an error code otherwise.

    See also: GetTangents
    ************************************************/
    virtual CKERROR SetTangents(int index, VxVector *in, VxVector *out) = 0;
    virtual CKERROR SetTangents(CKCurvePoint *pt, VxVector *in, VxVector *out) = 0;

    /************************************************
    Summary: Sets the fitting coefficient for the curve.
    Arguments:
        fit: Fitting coefficient.
    Remarks:
        {Image:FittingCoef}
        + A fitting coefficient of 0 make the curve pass by every control point.

    See also: GetFittingCoeff
    ************************************************/
    virtual void SetFittingCoeff(float fit) = 0;

    virtual float GetFittingCoeff() = 0;

    //------------------------------
    // Control points

    /************************************************
    Summary: Removes a control point.
    Arguments:
        pt: A pointer to the control point to remove.
        removeall: TRUE if all references to the control point should be removed.
    Return Value: CK_OK if successful or CKERR_INVALIDPARAMETER if pt is an invalid control point

    See also: RemoveAllControlPoints,InsertControlPoint,AddControlPoint,GetControlPointCount,GetControlPoint
    ************************************************/
    virtual CKERROR RemoveControlPoint(CKCurvePoint *pt, CKBOOL removeall = FALSE) = 0;

    /************************************************
    Summary: Inserts a control point in the curve.
    Arguments:
        prev: A pointer to the control point after which the point should be inserted.
        pt: A pointer to the control point to insert.
    Return Value: CK_OK if successful or an error code otherwise.

    See also: RemoveControlPoint,AddControlPoint,GetControlPointCount,GetControlPoint
    ************************************************/
    virtual CKERROR InsertControlPoint(CKCurvePoint *prev, CKCurvePoint *pt) = 0;

    /************************************************
    Summary: Adds a control point to the curve.
    Arguments:
        pt: A pointer to the CKCurvePoint to add.
    Return Value: CK_OK if successful, an error code otherwise.

    See also: RemoveControlPoint,InsertControlPoint,GetControlPointCount,GetControlPoint
    ************************************************/
    virtual CKERROR AddControlPoint(CKCurvePoint *pt) = 0;

    /************************************************
    Summary: Returns the number of control points
    Return Value: Number of control points

    See also: RemoveControlPoint,InsertControlPoint,AddControlPoint,GetControlPoint
    ************************************************/
    virtual int GetControlPointCount() = 0;

    /************************************************
    Summary: Gets a control point according to its index.
    Arguments:
        pos: Index of the control point to retrieve.
    Return Value: A pointer to the CKCurvePoint.

    See also: RemoveControlPoint,InsertControlPoint,GetControlPointCount,AddControlPoint
    ************************************************/
    virtual CKCurvePoint *GetControlPoint(int pos) = 0;

    /************************************************
    Summary: Removes all the control points
    Return Value: CK_OK if successful, an error code otherwise.

    See also: RemoveControlPoint
    ************************************************/
    virtual CKERROR RemoveAllControlPoints() = 0;

    //------------------------------
    // Mesh Representation

    /************************************************
    Summary: Sets the number of segments used to represent the curve.
    Arguments:
        steps: Number of segments.
    Return Value: CK_OK, if successful
    Remarks:
        + A line mesh can be created to represent the curve with CreateLineMesh, this method
        sets the number of segments used for this mesh.

    See also: GetStepCount
    ************************************************/
    virtual CKERROR SetStepCount(int steps) = 0;

    virtual int GetStepCount() = 0;

    /************************************************
    Summary: Creates a line mesh to represent the curve
    Return value: CK_OK if successful an error code otherwise.

    See also: SetStepCount,SetColor
    ************************************************/
    virtual CKERROR CreateLineMesh() = 0;

    virtual CKERROR UpdateMesh() = 0;

    /************************************************
    Summary: Gets the color used for the curve mesh.
    Return Value: Current color.

    See also: SetColor
    ************************************************/
    virtual VxColor GetColor() = 0;

    /************************************************
    Summary: Sets the color used for the curve mesh.
    Arguments:
        Color: new color for the curve.
    Remarks:

    See also: GetColor,CreateLineMesh
    ************************************************/
    virtual void SetColor(const VxColor &Color) = 0;

    virtual void Update() = 0;

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
    static CKCurve *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_CURVE) ? (CKCurve *)iO : NULL;
    }

#ifndef CK_3DIMPLEMENTATION
};

#endif
#endif // CKCURVE_H
