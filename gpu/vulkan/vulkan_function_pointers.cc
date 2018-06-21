// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindDeviceFunctionPointers(VkDevice vk_device) {
  // Device functions
  vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
      vkGetDeviceProcAddr(vk_device, "vkAcquireNextImageKHR"));
  if (!vkAcquireNextImageKHR)
    return false;

  vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateCommandBuffers"));
  if (!vkAllocateCommandBuffers)
    return false;

  vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateDescriptorSets"));
  if (!vkAllocateDescriptorSets)
    return false;

  vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateCommandPool"));
  if (!vkCreateCommandPool)
    return false;

  vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorPool"));
  if (!vkCreateDescriptorPool)
    return false;

  vkCreateDescriptorSetLayout =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorSetLayout"));
  if (!vkCreateDescriptorSetLayout)
    return false;

  vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFence"));
  if (!vkCreateFence)
    return false;

  vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFramebuffer"));
  if (!vkCreateFramebuffer)
    return false;

  vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImageView"));
  if (!vkCreateImageView)
    return false;

  vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCreateRenderPass"));
  if (!vkCreateRenderPass)
    return false;

  vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSampler"));
  if (!vkCreateSampler)
    return false;

  vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSemaphore"));
  if (!vkCreateSemaphore)
    return false;

  vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkCreateShaderModule"));
  if (!vkCreateShaderModule)
    return false;

  vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSwapchainKHR"));
  if (!vkCreateSwapchainKHR)
    return false;

  vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyCommandPool"));
  if (!vkDestroyCommandPool)
    return false;

  vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorPool"));
  if (!vkDestroyDescriptorPool)
    return false;

  vkDestroyDescriptorSetLayout =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorSetLayout"));
  if (!vkDestroyDescriptorSetLayout)
    return false;

  vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDevice"));
  if (!vkDestroyDevice)
    return false;

  vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFramebuffer"));
  if (!vkDestroyFramebuffer)
    return false;

  vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFence"));
  if (!vkDestroyFence)
    return false;

  vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImageView"));
  if (!vkDestroyImageView)
    return false;

  vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyRenderPass"));
  if (!vkDestroyRenderPass)
    return false;

  vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySampler"));
  if (!vkDestroySampler)
    return false;

  vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySemaphore"));
  if (!vkDestroySemaphore)
    return false;

  vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyShaderModule"));
  if (!vkDestroyShaderModule)
    return false;

  vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySwapchainKHR"));
  if (!vkDestroySwapchainKHR)
    return false;

  vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkFreeCommandBuffers"));
  if (!vkFreeCommandBuffers)
    return false;

  vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkFreeDescriptorSets"));
  if (!vkFreeDescriptorSets)
    return false;

  vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue"));
  if (!vkGetDeviceQueue)
    return false;

  vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddr(vk_device, "vkGetFenceStatus"));
  if (!vkGetFenceStatus)
    return false;

  vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
      vkGetDeviceProcAddr(vk_device, "vkGetSwapchainImagesKHR"));
  if (!vkGetSwapchainImagesKHR)
    return false;

  vkResetFences = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddr(vk_device, "vkResetFences"));
  if (!vkResetFences)
    return false;

  vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkUpdateDescriptorSets"));
  if (!vkUpdateDescriptorSets)
    return false;

  vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddr(vk_device, "vkWaitForFences"));
  if (!vkWaitForFences)
    return false;

  // Queue functions
  vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
      vkGetDeviceProcAddr(vk_device, "vkQueuePresentKHR"));
  if (!vkQueuePresentKHR)
    return false;

  vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(vk_device, "vkQueueSubmit"));
  if (!vkQueueSubmit)
    return false;

  vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkQueueWaitIdle"));
  if (!vkQueueWaitIdle)
    return false;

  // Command Buffer functions
  vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkBeginCommandBuffer"));
  if (!vkBeginCommandBuffer)
    return false;

  vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdBeginRenderPass"));
  if (!vkCmdBeginRenderPass)
    return false;

  vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdEndRenderPass"));
  if (!vkCmdEndRenderPass)
    return false;

  vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddr(vk_device, "vkCmdExecuteCommands"));
  if (!vkCmdExecuteCommands)
    return false;

  vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdNextSubpass"));
  if (!vkCmdNextSubpass)
    return false;

  vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddr(vk_device, "vkCmdPipelineBarrier"));
  if (!vkCmdPipelineBarrier)
    return false;

  vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkEndCommandBuffer"));
  if (!vkEndCommandBuffer)
    return false;

  vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkResetCommandBuffer"));
  if (!vkResetCommandBuffer)
    return false;

  return true;
}

}  // namespace gpu