#ifndef VXMATH_H
#define VXMATH_H

#include "VxMathDefines.h"

// Containers
#include "XP.h"
#include "XSmartPtr.h"
#include "XString.h"
#include "XArray.h"
#include "XSArray.h"
#include "XClassArray.h"
#include "XList.h"
#include "XHashTable.h"
#include "XSHashTable.h"

// Port Class Utility
#include "VxSharedLibrary.h"
#include "VxMeMoryMappedFile.h"
#include "CKPathSplitter.h"
#include "CKDirectoryParser.h"
#include "VxWindowFunctions.h"
#include "VxVector.h"
#include "Vx2dVector.h"
#include "VxMatrix.h"
#include "VxConfiguration.h"
#include "VxQuaternion.h"
#include "VxRect.h"
#include "VxOBB.h"
#include "VxRay.h"
#include "VxSphere.h"
#include "VxPlane.h"
#include "VxIntersect.h"
#include "VxDistance.h"
#include "VxFrustum.h"
#include "VxColor.h"
#include "VxMemoryPool.h"
#include "VxTimeProfiler.h"
#include "VxImageDescEx.h"

// Threads and Synchro
#include "VxMutex.h"
#include "VxThread.h"

//------ Automatically called in dynamic library
void InitVxMath();

void VxDetectProcessor();

//------ Interpolation
VX_EXPORT void InterpolateFloatArray(void *Res, void *array1, void *array2, float factor, int count);
VX_EXPORT void InterpolateVectorArray(void *Res, void *Inarray1, void *Inarray2, float factor, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT void MultiplyVectorArray(void *Res, void *Inarray1, const VxVector &factor, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT void MultiplyVector2Array(void *Res, void *Inarray1, const Vx2DVector &factor, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT void MultiplyVector4Array(void *Res, void *Inarray1, const VxVector4 &factor, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT void MultiplyAddVectorArray(void *Res, void *Inarray1, const VxVector &factor, const VxVector &offset, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT void MultiplyAddVector4Array(void *Res, void *Inarray1, const VxVector4 &factor, const VxVector4 &offset, int count, XULONG StrideRes, XULONG StrideIn);
VX_EXPORT XBOOL VxTransformBox2D(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags, VXCLIP_FLAGS &AndClipFlags);
VX_EXPORT void VxProjectBoxZExtents(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax);

//------- Structure copying
VX_EXPORT XBOOL VxFillStructure(int Count, void *Dst, XULONG Stride, XULONG SizeSrc, void *Src);
VX_EXPORT XBOOL VxCopyStructure(int Count, void *Dst, XULONG OutStride, XULONG SizeSrc, void *Src, XULONG InStride);
VX_EXPORT XBOOL VxIndexedCopy(const VxStridedData &Dst, const VxStridedData &Src, XULONG SizeSrc, int *Indices, int IndexCount);

//---- Graphic Utilities
VX_EXPORT void VxDoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc);
VX_EXPORT void VxDoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc);

VX_EXPORT void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue);
VX_EXPORT void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues);

VX_EXPORT void VxGetBitCounts(const VxImageDescEx &desc, XULONG &Rbits, XULONG &Gbits, XULONG &Bbits, XULONG &Abits);
VX_EXPORT void VxGetBitShifts(const VxImageDescEx &desc, XULONG &Rshift, XULONG &Gshift, XULONG &Bshift, XULONG &Ashift);

VX_EXPORT void VxGenerateMipMap(const VxImageDescEx &src_desc, XBYTE *DestBuffer);
VX_EXPORT void VxResizeImage32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc);

VX_EXPORT XBOOL VxConvertToNormalMap(const VxImageDescEx &image, XULONG ColorMask);
VX_EXPORT XBOOL VxConvertToBumpMap(const VxImageDescEx &image);

VX_EXPORT XULONG GetBitCount(XULONG dwMask);
VX_EXPORT XULONG GetBitShift(XULONG dwMask);

VX_EXPORT VX_PIXELFORMAT VxImageDesc2PixelFormat(const VxImageDescEx &desc);
VX_EXPORT void VxPixelFormat2ImageDesc(VX_PIXELFORMAT Pf, VxImageDescEx &desc);
VX_EXPORT const char *VxPixelFormat2String(VX_PIXELFORMAT Pf);

VX_EXPORT void VxBppToMask(VxImageDescEx &desc);

VX_EXPORT int GetQuantizationSamplingFactor();
VX_EXPORT void SetQuantizationSamplingFactor(int sf);

//---- Processor features
VX_EXPORT char *GetProcessorDescription();
VX_EXPORT int GetProcessorFrequency();
VX_EXPORT XULONG GetProcessorFeatures();
VX_EXPORT void ModifyProcessorFeatures(XULONG Add, XULONG Remove);
VX_EXPORT ProcessorsType GetProcessorType();

VX_EXPORT XBOOL VxPtInRect(CKRECT *rect, CKPOINT *pt);

// Summary: Compute best Fit Box for a set of points
VX_EXPORT XBOOL VxComputeBestFitBBox(const XBYTE *Points, const XULONG Stride, const int Count, VxMatrix &BBoxMatrix, const float AdditionnalBorder);

// Path Conversion
VX_EXPORT void VxAddDirectorySeparator(XString &path);
VX_EXPORT void VxConvertPathToSystemPath(XString &path);

#endif // VXMATH_H
