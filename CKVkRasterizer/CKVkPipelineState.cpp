#include "CKVkPipelineState.h"
#include "ManagedVulkanPipeline.h"
#include "VkPipelineBuilder.h"

#include <cstring>
#include <deque>

CKVkPipelineState::CKVkPipelineState() :
    vf(0), pty(VX_TRIANGLELIST), fillm(VXFILL_SOLID), cullm(VXCULL_CW), inversew(false),
    deptht(false), depthw(false), deptho(VXCMP_LESS), blende(false),
    srcc(VXBLEND_ONE), dstc(VXBLEND_ZERO), srca(VXBLEND_ONE), dsta(VXBLEND_ZERO)
{
}


bool CKVkPipelineState::operator==(const CKVkPipelineState &o) const
{
    return !memcmp(this, &o, sizeof(CKVkPipelineState));
}

void CKVkPipelineState::set_vertex_format(uint32_t vfmt) { vf = vfmt; }

void CKVkPipelineState::set_primitive_type(VXPRIMITIVETYPE primty) { pty = primty; }

void CKVkPipelineState::set_fill_mode(VXFILL_MODE fillmode) { fillm = fillmode; }

void CKVkPipelineState::set_cull_mode(VXCULL cullmode) { cullm = cullmode; }

void CKVkPipelineState::set_inverse_winding(bool inversewinding) { inversew = inversewinding; }

void CKVkPipelineState::set_depth_test(bool depthtest) { deptht = depthtest; }

void CKVkPipelineState::set_depth_write(bool depthwrite) { depthw = depthwrite; }

void CKVkPipelineState::set_depth_func(VXCMPFUNC depthfunc) { deptho = depthfunc; }

void CKVkPipelineState::set_blending_enable(bool e) { blende = e; }

void CKVkPipelineState::set_src_blend(VXBLEND_MODE srccolor, VXBLEND_MODE srcalpha)
{
    srcc = srccolor;
    srca = srcalpha;
}

void CKVkPipelineState::set_dst_blend(VXBLEND_MODE dstcolor, VXBLEND_MODE dstalpha)
{
    dstc = dstcolor;
    dsta = dstalpha;
}

ManagedVulkanPipeline *CKVkPipelineState::build_pipeline(const VkPipelineBuilder *ptemplate, VkDevice vkdev, const CKVkPipelineState &st)
{
    auto vxblend2vkblendfactor = [](VXBLEND_MODE m) -> VkBlendFactor
    {
        switch (m)
        {
            case VXBLEND_ZERO            : return VK_BLEND_FACTOR_ZERO;
            case VXBLEND_ONE             : return VK_BLEND_FACTOR_ONE;
            case VXBLEND_SRCCOLOR        : return VK_BLEND_FACTOR_SRC_COLOR;
            case VXBLEND_INVSRCCOLOR     : return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case VXBLEND_SRCALPHA        : return VK_BLEND_FACTOR_SRC_ALPHA;
            case VXBLEND_INVSRCALPHA     : return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case VXBLEND_DESTALPHA       : return VK_BLEND_FACTOR_DST_ALPHA;
            case VXBLEND_INVDESTALPHA    : return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case VXBLEND_DESTCOLOR       : return VK_BLEND_FACTOR_DST_COLOR;
            case VXBLEND_INVDESTCOLOR    : return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case VXBLEND_SRCALPHASAT     : return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case VXBLEND_BOTHSRCALPHA    : return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case VXBLEND_BOTHINVSRCALPHA : return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            default                      : return VK_BLEND_FACTOR_MAX_ENUM;
        }
    };
    VkPipelineColorBlendAttachmentState blendatt {
        .blendEnable=st.blende,
        .srcColorBlendFactor=vxblend2vkblendfactor(st.srcc),
        .dstColorBlendFactor=vxblend2vkblendfactor(st.dstc),
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=vxblend2vkblendfactor(st.srca),
        .dstAlphaBlendFactor=vxblend2vkblendfactor(st.dsta),
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
    };

    auto b = VkPipelineBuilder(*ptemplate);
    b.add_input_binding(rst_vertex_format_to_vulkan_input_binding(st.vf));
    auto vav = rst_vertex_format_to_vulkan_vertex_attrib(st.vf);
    std::deque<VkVertexInputAttributeDescription> vad(vav.begin(), vav.end());
    for (;!vad.empty(); vad.pop_front())
        b.add_vertex_attribute(std::move(vad.front()));

    switch (st.pty)
    {
        case VX_POINTLIST     : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST); break;
        case VX_LINELIST      : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST); break;
        case VX_LINESTRIP     : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP); break;
        case VX_TRIANGLELIST  : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST); break;
        case VX_TRIANGLESTRIP : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP); break;
        case VX_TRIANGLEFAN   : b.primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN); break;
    }

    switch (st.fillm)
    {
        case VXFILL_POINT     : b.polygon_mode(VK_POLYGON_MODE_POINT); break;
        case VXFILL_WIREFRAME : b.polygon_mode(VK_POLYGON_MODE_LINE); break;
        case VXFILL_SOLID     : b.polygon_mode(VK_POLYGON_MODE_FILL); break;
    }

    switch (st.cullm)
    {
        case VXCULL_NONE:
            b.cull_mode(VK_CULL_MODE_NONE);
        break;
        case VXCULL_CW:
            b.cull_mode(st.inversew ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT)
             .front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        break;
        case VXCULL_CCW:
            b.cull_mode(st.inversew ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT)
             .front_face(VK_FRONT_FACE_CLOCKWISE);
        break;
    }

    b.depth_test_enable(st.deptht).depth_write_enable(st.depthw);
    switch (st.deptho)
    {
        case VXCMP_NEVER        : b.depth_op(VK_COMPARE_OP_NEVER); break;
        case VXCMP_LESS         : b.depth_op(VK_COMPARE_OP_LESS); break;
        case VXCMP_EQUAL        : b.depth_op(VK_COMPARE_OP_EQUAL); break;
        case VXCMP_LESSEQUAL    : b.depth_op(VK_COMPARE_OP_LESS_OR_EQUAL); break;
        case VXCMP_GREATER      : b.depth_op(VK_COMPARE_OP_GREATER); break;
        case VXCMP_NOTEQUAL     : b.depth_op(VK_COMPARE_OP_NOT_EQUAL); break;
        case VXCMP_GREATEREQUAL : b.depth_op(VK_COMPARE_OP_GREATER_OR_EQUAL); break;
        case VXCMP_ALWAYS       : b.depth_op(VK_COMPARE_OP_ALWAYS); break;
    }

    b.add_blending_attachment(std::move(blendatt));
    return b.build(vkdev);
}

size_t std::hash<CKVkPipelineState>::operator()(const CKVkPipelineState &s) const noexcept
{
    const bool *p = reinterpret_cast<const bool*>(&s);
    std::vector<bool> m(p, p + sizeof(CKVkPipelineState));
    return std::hash<std::vector<bool>>{}(m);
}
