#ifndef CKVKPIPELINESTATE_H
#define CKVKPIPELINESTATE_H

#include <cstdint>
#include <functional>

#include <VxDefines.h>
#include <CKRasterizerEnums.h>

#include "VulkanUtils.h"

class ManagedVulkanPipeline;
class VkPipelineBuilder;

class CKVkPipelineState
{
public:
    CKVkPipelineState();

    bool operator ==(const CKVkPipelineState &o) const;

    void set_vertex_format(uint32_t vfmt);
    void set_primitive_type(VXPRIMITIVETYPE primty);
    void set_fill_mode(VXFILL_MODE fillmode);
    void set_cull_mode(VXCULL cullmode);
    void set_inverse_winding(bool inversewinding);
    void set_depth_test(bool depthtest);
    void set_depth_write(bool depthwrite);
    void set_depth_func(VXCMPFUNC depthfunc);
    void set_blending_enable(bool e);
    void set_src_blend(VXBLEND_MODE srccolor, VXBLEND_MODE srcalpha);
    void set_dst_blend(VXBLEND_MODE dstcolor, VXBLEND_MODE dstalpha);

    static ManagedVulkanPipeline *build_pipeline(const VkPipelineBuilder *ptemplate, VkDevice vkdev, const CKVkPipelineState &st);
private:
    uint32_t vf;
    VXPRIMITIVETYPE pty;
    VXFILL_MODE fillm;
    VXCULL cullm;
    bool inversew;
    bool deptht;
    bool depthw;
    VXCMPFUNC deptho;
    bool blende;
    VXBLEND_MODE srcc;
    VXBLEND_MODE dstc;
    VXBLEND_MODE srca;
    VXBLEND_MODE dsta;
};

template<>
struct std::hash<CKVkPipelineState>
{
    size_t operator()(const CKVkPipelineState& s) const noexcept;
};

#endif