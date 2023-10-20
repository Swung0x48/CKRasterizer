#ifndef CKVKTEXTURE_H
#define CKVKTEXTURE_H

#include <CKRasterizerTypes.h>

#include "VulkanUtils.h"

class CKVkRasterizerContext;

class CKVkTexture : public CKTextureDesc
{
private:
    CKVkRasterizerContext *rctx;
    VkImage img;
    VkDeviceMemory dmem;
    VkImageView imgv;
public:
    CKVkTexture(CKVkRasterizerContext *ctx, CKTextureDesc *texdesc);
    ~CKVkTexture();

    void create();
    void bind();
    void load(void *data);
};

#endif