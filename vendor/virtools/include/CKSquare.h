#ifndef CKSQUARE_H
#define CKSQUARE_H

#include "CKDefines.h"

class CKSquare
{
public:
    ////////////////////////////////////////////////////////
    ////            Members of a Square                 ////
    ////////////////////////////////////////////////////////

    union
    { // Value
        int ival;
        float fval;
        CKDWORD dval;
        void *ptr;
    };
};

#endif // CKSQUARE_H
