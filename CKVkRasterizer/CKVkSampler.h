#ifndef CKVKSAMPLER_H
#define CKVKSAMPLER_H

#include <CKRasterizerTypes.h>

#include "VulkanUtils.h"

class CKVkRasterizerContext;

class CKVkSampler
{
public:
    ~CKVkSampler();

    VkSampler sampler();
private:
    CKVkSampler(VkSampler _s, VkDevice _dev);
    VkSampler s;
    VkDevice dev;

    friend class CKVkSamplerBuilder;
};

class CKVkSamplerBuilder
{
public:
    CKVkSamplerBuilder();

    CKVkSamplerBuilder& mag_filter(VkFilter f);
    CKVkSamplerBuilder& min_filter(VkFilter f);
    CKVkSamplerBuilder& mipmap_mode(VkSamplerMipmapMode m);
    CKVkSamplerBuilder& address_mode_u(VkSamplerAddressMode m);
    CKVkSamplerBuilder& address_mode_v(VkSamplerAddressMode m);
    CKVkSamplerBuilder& address_mode_w(VkSamplerAddressMode m);
    CKVkSamplerBuilder& mip_lod_bias(float b);
    CKVkSamplerBuilder& anisotropy_enable(bool e);
    CKVkSamplerBuilder& max_anisotropy(float a);
    CKVkSamplerBuilder& compare_enable(bool e);
    CKVkSamplerBuilder& compare_op(VkCompareOp o);
    CKVkSamplerBuilder& min_lod(float l);
    CKVkSamplerBuilder& max_lod(float l);
    CKVkSamplerBuilder& border_color(VkBorderColor c);
    CKVkSamplerBuilder& unnormalized_coords(bool u);
    
    CKVkSampler* build(VkDevice dev) const;
private:
    VkSamplerCreateInfo sci;
};

#endif