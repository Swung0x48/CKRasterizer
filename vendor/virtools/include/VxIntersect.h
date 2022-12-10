#ifndef VXINTERSECT_H
#define VXINTERSECT_H

#include "VxMathDefines.h"

struct VxVector;
struct VxBbox;
class VxMatrix;
class VxRay;
class VxOBB;
class VxPlane;
class VxSphere;
class VxFrustum;

/**********************************************************
Name: VxIntersect

Summary: Static methods for intersection tests.

Remarks: VxIntersect class contains a list of static methods useful for miscellaneous intersection tests.
    Using these methods one can test intersection of ray,segment or line with a Box,infinite planes, triangle faces
    or Sphere. Methods for box-box (Boxes) and plane-plane (Planes) intersection are also provided




See Also : VxPlane,VxRay,VxBbox,VxVector,Intersections
*********************************************************/
class VxIntersect
{
public:
    //----------- Boxes

    // Intersection Ray - Box (return -1 if origin inside)
    VX_EXPORT static XBOOL RayBox(const VxRay &ray, const VxBbox &box);
    VX_EXPORT static XBOOL RayBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);
    // Intersection Segment - Box (return -1 if origin inside)
    VX_EXPORT static XBOOL SegmentBox(const VxRay &ray, const VxBbox &box);
    VX_EXPORT static XBOOL SegmentBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);
    // Intersection Line - Box
    VX_EXPORT static XBOOL LineBox(const VxRay &ray, const VxBbox &box);
    VX_EXPORT static XBOOL LineBox(const VxRay &ray, const VxBbox &box, VxVector &inpoint, VxVector *outpoint = NULL, VxVector *innormal = NULL, VxVector *outnormal = NULL);

    // Intersection Box - Box
    VX_EXPORT static XBOOL AABBAABB(const VxBbox &box1, const VxBbox &box2);
    VX_EXPORT static XBOOL AABBOBB(const VxBbox &box1, const VxOBB &box2);
    VX_EXPORT static XBOOL OBBOBB(const VxOBB &box1, const VxOBB &box2);
    VX_EXPORT static XBOOL AABBFace(const VxBbox &box1, const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N);

    //---------- Planes

    // Intersection Ray - Plane
    VX_EXPORT static XBOOL RayPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    VX_EXPORT static XBOOL RayPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    // Intersection Segment - Plane
    VX_EXPORT static XBOOL SegmentPlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    VX_EXPORT static XBOOL SegmentPlaneCulled(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    // Intersection Line - Plane
    VX_EXPORT static XBOOL LinePlane(const VxRay &ray, const VxPlane &plane, VxVector &point, float &dist);
    // Intersection Box - Plane
    VX_EXPORT static XBOOL BoxPlane(const VxBbox &box, const VxPlane &plane);
    // Intersection Box - Plane
    VX_EXPORT static XBOOL BoxPlane(const VxBbox &box, const VxMatrix &mat, const VxPlane &plane);
    // Intersection Box - Plane
    VX_EXPORT static XBOOL FacePlane(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxPlane &plane);

    // Intersection of 3 Planes
    VX_EXPORT static XBOOL Planes(const VxPlane &plane1, const VxPlane &plane2, const VxPlane &plane3, VxVector &p);

    //---------- Faces

    // Is a point is inside the boundary of a face (the point is not checked againt the face/plane insides)
    VX_EXPORT static XBOOL PointInFace(const VxVector &point, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, int &i1, int &i2);

    // Intersection Ray - Face
    VX_EXPORT static XBOOL RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    VX_EXPORT static XBOOL RayFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    VX_EXPORT static XBOOL RayFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    // Intersection Segment - Face
    VX_EXPORT static XBOOL SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    VX_EXPORT static XBOOL SegmentFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    VX_EXPORT static XBOOL SegmentFaceCulled(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);
    // Intersection Line - Face
    VX_EXPORT static XBOOL LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist);
    VX_EXPORT static XBOOL LineFace(const VxRay &ray, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const VxVector &norm, VxVector &res, float &dist, int &i1, int &i2);

    // Foe a given point inside a face return the coefficient for each face vertex to interpolate normal or uv data ( pt.uv = pt0.uv * V0Coef + pt1.uv * V1Coef + pt2.uv * V2Coef
    VX_EXPORT static void GetPointCoefficients(const VxVector &pt, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2, const int &i1, const int &i2, float &V0Coef, float &V1Coef, float &V2Coef);

    // Intersection Face - Face
    VX_EXPORT static XBOOL FaceFace(const VxVector &A0, const VxVector &A1, const VxVector &A2, const VxVector &N0, const VxVector &B0, const VxVector &B1, const VxVector &B2, const VxVector &N1);

    //--------- Frustum

    // Intersection Ray - Face
    VX_EXPORT static XBOOL FrustumFace(const VxFrustum &frustum, const VxVector &pt0, const VxVector &pt1, const VxVector &pt2);
    // Intersection Ray - Face
    VX_EXPORT static XBOOL FrustumAABB(const VxFrustum &frustum, const VxBbox &box);
    // Intersection Frustum - Box
    VX_EXPORT static XBOOL FrustumOBB(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);
    // Intersection Frustum - Box
    VX_EXPORT static XBOOL FrustumBox(const VxFrustum &frustum, const VxBbox &box, const VxMatrix &mat);

    //--------- Spheres
    VX_EXPORT static XBOOL SphereSphere(const VxSphere &iS1, const VxVector &iP1, const VxSphere &iS2, const VxVector &iP2, float *oCollisionTime1, float *oCollisionTime2);
    // Intersection Ray - Sphere
    VX_EXPORT static int RaySphere(const VxRay &iRay, const VxSphere &iSphere, VxVector *oInter1, VxVector *oInter2);
    // Intersection Ray - Sphere returns the number of intersection(s) of a ray on the sphere (0,1 or 2)
    VX_EXPORT static int SphereAABB(const VxSphere &iSphere, const VxBbox &iBox);
};

#endif // VXINTERSECT_H
