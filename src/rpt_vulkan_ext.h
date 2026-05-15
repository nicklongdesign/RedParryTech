#include "prelude.h"
#include <vulkan/vulkan.h>

#ifndef VK_NO_PROTOTYPES
	#define DeclareExtFn_(fn) extern PFN_##fn RPT_##fn = nullptr
	#define DefineExtFn_(device, fn) RPT_##fn = Recast_<PFN_##fn>(vkGetDeviceProcAddr(device, #fn))
#else
	#define DeclareExtFn_(fn) extern PFN_##fn fn = nullptr
	#define DefineExtFn_(device, fn) fn = Recast_<PFN_##fn>(vkGetDeviceProcAddr(device, #fn))
#endif

DeclareExtFn_(vkCreateShadersEXT);
DeclareExtFn_(vkDestroyShaderEXT);
DeclareExtFn_(vkGetShaderBinaryDataEXT);
DeclareExtFn_(vkCmdBindShadersEXT);
DeclareExtFn_(vkCmdSetDepthClampRangeEXT);

#ifndef VK_NO_PROTOTYPES
VkResult vkCreateShadersEXT(
    VkDevice                                    device,
    uint32_t                                    createInfoCount,
    const VkShaderCreateInfoEXT*                pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkShaderEXT*                                pShaders) 
{
	return RPT_vkCreateShadersEXT(device, createInfoCount, pCreateInfos, pAllocator, pShaders);
}

void vkDestroyShaderEXT(
    VkDevice                                    device,
    VkShaderEXT                                 shader,
    const VkAllocationCallbacks*                pAllocator)
{
	RPT_vkDestroyShaderEXT(device, shader, pAllocator);
}

VkResult vkGetShaderBinaryDataEXT(
    VkDevice                                    device,
    VkShaderEXT                                 shader,
    size_t*                                     pDataSize,
    void*                                       pData)
{
	return RPT_vkGetShaderBinaryDataEXT(device, shader, pDataSize, pData);
}

void vkCmdBindShadersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    stageCount,
    const VkShaderStageFlagBits*                pStages,
    const VkShaderEXT*                          pShaders) 
{
	RPT_vkCmdBindShadersEXT(commandBuffer, stageCount, pStages, pShaders);
}

void vkCmdSetDepthClampRangeEXT(
    VkCommandBuffer                             commandBuffer,
    VkDepthClampModeEXT                         depthClampMode,
    const VkDepthClampRangeEXT*                 pDepthClampRange)
{
	RPT_vkCmdSetDepthClampRangeEXT(commandBuffer, depthClampMode, pDepthClampRange);
}
#endif