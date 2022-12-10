#ifndef XUTIL_H
#define XUTIL_H

#include "VxMathDefines.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <assert.h>
#define XASSERT(a) assert(a)

typedef int(__cdecl *VxSortFunc)(const void *elem1, const void *elem2);

/*************************************************
Summary: Enables the initialisation of a reference from two values.

Arguments:
    iCond:  TRUE to return iT1 , FALSE to return iT2;
*************************************************/
template <class T>
const T &ChooseRef(XBOOL iCond, const T &iT1, const T &iT2)
{
    if (iCond)
        return iT1;
    else
        return iT2;
}

/*************************************************
Summary: Forces a value in an interval.

Arguments:
    t:  value to threshold
    min: minimum valid value
    max: maximum valid value
See Also: XMin,XMax
*************************************************/
template <class T>
void XThreshold(T &t, const T &min, const T &max)
{
    if (t < min)
        t = min;
    else if (t > max)
        t = max;
}

/*************************************************
Summary: Returns the minimum of two values.

Remarks:
    Returns minimum between a and b.



See Also: XMax
*************************************************/
template <class T>
const T &XMin(const T &a, const T &b)
{
    if (a < b)
        return a;
    else
        return b;
}

/*************************************************
Summary: Returns the maximum of two values.

Remarks:
    Returns maximum between a and b.



See Also: XMax
*************************************************/
template <class T>
const T &XMax(const T &a, const T &b)
{
    if (a > b)
        return a;
    else
        return b;
}

/*************************************************
Summary: Finds the minimum and maximum of 2 values.

Remarks:
    Returns minimum and maximum between a,b.



See Also: XMin,XMax
*************************************************/
template <class T>
void XMinMax(const T &a, const T &b, T &min, T &max)
{
    if (a < b)
    {
        min = a;
        max = b;
    }
    else
    {
        min = b;
        max = a;
    }
}

/*************************************************
Summary: Returns the minimum of two values.

Remarks:
    Returns minimum between a,b and c.



See Also: XMax
*************************************************/
template <class T>
const T &XMin(const T &a, const T &b, const T &c)
{
    return (a < b) ? ((c < a) ? c : a) : ((c < b) ? c : b);
}

/*************************************************
Summary: Returns the maximum of two values.

Remarks:
    Returns maximum between a,b and c.



See Also: XMin
*************************************************/
template <class T>
const T &XMax(const T &a, const T &b, const T &c)
{
    return (a > b) ? ((c > a) ? c : a) : ((c > b) ? c : b);
}

/*************************************************
Summary: Finds the minimum and maximum of 3 values.

Remarks:
    Returns minimum and maximum between a,b and c.



See Also: XMin,XMax
*************************************************/
template <class T>
void XMinMax(const T &a, const T &b, const T &c, T &min, T &max)
{
    if (a < b)
    {
        if (c < a)
        {
            min = c;
            max = b;
        }
        else
        {
            min = a;
            if (b < c)
                max = c;
            else
                max = b;
        }
    }
    else
    {
        if (c < b)
        {
            min = c;
            max = a;
        }
        else
        {
            min = b;
            if (a < c)
                max = c;
            else
                max = a;
        }
    }
}

template <class T>
void XMinMax(const T &a, const T &b, const T &c, T &min, T &med, T &max)
{
    if (a < b)
    {
        if (c < a)
        {
            min = c;
            med = a;
            max = b;
        }
        else
        {
            min = a;
            if (b < c)
            {
                med = b;
                max = c;
            }
            else
            {
                med = c;
                max = b;
            }
        }
    }
    else
    {
        if (c < b)
        {
            min = c;
            med = b;
            max = a;
        }
        else
        {
            min = b;
            if (a < c)
            {
                max = c;
                med = a;
            }
            else
            {
                max = a;
                med = c;
            }
        }
    }
}

/*************************************************
Summary: Swaps two elements.

Remarks:
    Swaps a and b.


*************************************************/
template <class T>
void XSwap(T &a, T &b)
{
    T c = a;
    a = b;
    b = c;
}
/*************************************************
Summary: Returns the absolute value of a.

Remarks:

*************************************************/
template <class T>
T XAbs(T a)
{
    return (a > 0) ? a : -a;
}

// float specialization
template <>
inline float XAbs<float>(float a) { return (float)::fabs(a); }

/*************************************************
Summary: Returns the absolute value of a float.

Remarks:

*************************************************/
inline float XFabs(float a)
{
    return (float)::fabs((double)a);
    // return *(float*)&( (*(int*)(&a)) & 0xF800);
}

//------ Memory Management

#define VxNew(a) (new unsigned char[a])

#define VxDelete(a) (delete[](unsigned char *) a)

template <class T>
T *VxAllocate(int n);

template <class T>
void VxFree(T *t);

inline int LowestBitMask(int v)
{
    return ((v & -v));
}

/*************************************************
Summary: return true if num is a power of 2

Remarks:

*************************************************/
inline XBOOL Is2Power(int x)
{
    return x != 0 && x == LowestBitMask(x);
}

/*************************************************
Summary: return the nearest superior power of 2 of v

Remarks:

*************************************************/
inline int Near2Power(int v)
{
    int i = LowestBitMask(v);
    while (i < v)
        i <<= 1;
    return i;
}

/*******************************************************************************
Summary: Global Unique Identifier Structure.

Remarks: Comparison operators are defined so XGUIDS can be compared with
==,!= ,<,> operators.
*******************************************************************************/
class XGUID
{
public:
    explicit XGUID(XDWORD gd1 = 0, XDWORD gd2 = 0)
        : d1(gd1), d2(gd2)
    {
    }

    friend XBOOL operator==(const XGUID &v1, const XGUID &v2)
    {
        return ((v1.d1 == v2.d1) && (v1.d2 == v2.d2));
    }

    friend XBOOL operator!=(const XGUID &v1, const XGUID &v2)
    {
        return ((v1.d1 != v2.d1) || (v1.d2 != v2.d2));
    }

    friend XBOOL operator<(const XGUID &v1, const XGUID &v2)
    {
        if (v1.d1 < v2.d1)
            return true;
        if (v1.d1 == v2.d1)
            return (v1.d2 < v2.d2);
        return false;
    }

    friend XBOOL operator<=(const XGUID &v1, const XGUID &v2)
    {
        return (v1.d1 <= v2.d1);
    }

    friend XBOOL operator>(const XGUID &v1, const XGUID &v2)
    {
        if (v1.d1 > v2.d1)
            return true;
        if (v1.d1 == v2.d1)
            return (v1.d2 > v2.d2);
        return false;
    }

    friend XBOOL operator>=(const XGUID &v1, const XGUID &v2)
    {
        return (v1.d1 >= v2.d1);
    }

    XBOOL inline IsValid()
    {
        return d1 && d2;
    }

    XDWORD d1;
    XDWORD d2;
};

#endif // XUTIL_H
