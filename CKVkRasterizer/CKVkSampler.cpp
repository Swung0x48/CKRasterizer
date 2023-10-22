#include "CKVkSampler.h"

CKVkSampler::CKVkSampler(VkSampler _s, VkDevice _dev) : s(_s), dev(_dev) {}

CKVkSampler::~CKVkSampler()
{
    vkDestroySampler(dev, s, nullptr); }

VkSampler CKVkSampler::sampler() { return s; }

CKVkSamplerBuilder::CKVkSamplerBuilder() : sci(make_vulkan_structure<VkSamplerCreateInfo>({
    .magFilter=VK_FILTER_LINEAR,
    .minFilter=VK_FILTER_LINEAR,
    .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias=0.,
    .anisotropyEnable=VK_FALSE,
    .maxAnisotropy=1.,
    .compareEnable=VK_FALSE,
    .compareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
    .minLod=-1000.,
    .maxLod=1000.,
    .borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    .unnormalizedCoordinates=VK_FALSE}))
{
}

CKVkSamplerBuilder &CKVkSamplerBuilder::mag_filter(VkFilter f)
{
    sci.magFilter = f;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::min_filter(VkFilter f)
{
    sci.minFilter = f;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::mipmap_mode(VkSamplerMipmapMode m)
{
    sci.mipmapMode = m;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::address_mode_u(VkSamplerAddressMode m)
{
    sci.addressModeU = m;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::address_mode_v(VkSamplerAddressMode m)
{
    sci.addressModeV = m;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::address_mode_w(VkSamplerAddressMode m)
{
    sci.addressModeW = m;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::mip_lod_bias(float b)
{
    sci.mipLodBias = b;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::anisotropy_enable(bool e)
{
    sci.anisotropyEnable = e;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::max_anisotropy(float a)
{
    sci.maxAnisotropy = a;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::compare_enable(bool e)
{
    sci.compareEnable = e;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::compare_op(VkCompareOp o)
{
    sci.compareOp = o;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::min_lod(float l)
{
    sci.minLod = l;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::max_lod(float l)
{
    sci.maxLod = l;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::border_color(VkBorderColor c)
{
    sci.borderColor = c;
    return *this;
}

CKVkSamplerBuilder &CKVkSamplerBuilder::unnormalized_coords(bool u)
{
    sci.unnormalizedCoordinates = u;
    return *this;
}

CKVkSampler *CKVkSamplerBuilder::build(VkDevice dev) const
{
    VkSampler s;
    vkCreateSampler(dev, &sci, nullptr, &s);
    return new CKVkSampler(s, dev);
}
