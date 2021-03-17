/* Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vulkan/vulkan.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

#include "fileformats/stb_image.h"
#include "environmentalLight.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include <numeric>
#include "imgui_helper.h"
#include "imgui_orient.h"

#include "environmentalLight.hpp"

bool EnvironmentalLight::renderUI() {
    using GuiH = ImGuiH::Control;
    static SunAndSky dss = SunAndSky_default();  // default values

    bool changed = false;
    
    changed |= GuiH::Checkbox("Use Sun & Sky","", &m_useSunAndSky, false, GuiH::Flags::Normal);
    if (m_useSunAndSky) {
        changed |= GuiH::Custom("Direction", "Sun Direction", [&] {
            float indent = ImGui::GetCursorPos().x;
            changed |= ImGui::DirectionGizmo("", &m_sunAndSky.sun_direction.x, true);
            ImGui::NewLine();
            ImGui::SameLine(indent);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            changed |= ImGui::InputFloat3("##IG", &m_sunAndSky.sun_direction.x);
            return changed;
            });
        changed |= GuiH::Slider("Disk Scale", "", &m_sunAndSky.sun_disk_scale, &dss.sun_disk_scale, GuiH::Flags::Normal, 0.f, 100.f);
        changed |= GuiH::Slider("Glow Intensity", "", &m_sunAndSky.sun_glow_intensity, &dss.sun_glow_intensity,
            GuiH::Flags::Normal, 0.f, 5.f);
        changed |= GuiH::Slider("Disk Intensity", "", &m_sunAndSky.sun_disk_intensity, &dss.sun_disk_intensity,
            GuiH::Flags::Normal, 0.f, 5.f);
        changed |= GuiH::Color("Night Color", "", &m_sunAndSky.night_color.x, &dss.night_color.x, GuiH::Flags::Normal);
    }
    return changed;
}

void EnvironmentalLight::updateSunAndSky(const vk::CommandBuffer& cmdBuf) {
    cmdBuf.updateBuffer<SunAndSky>(m_sunAndSkyBuffer.buffer, 0, m_sunAndSky);
}

void EnvironmentalLight::createDescriptorSetLayout() {
    using vkDS = vk::DescriptorSetLayoutBinding;
    using vkDT = vk::DescriptorType;
    using vkSS = vk::ShaderStageFlagBits;

    auto flag = vkSS::eRaygenKHR| vkSS::eClosestHitKHR | vkSS::eAnyHitKHR|vkSS::eMissKHR | vkSS::eCompute | vkSS::eVertex|vkSS::eFragment;
    auto& bind = m_descBind;

    bind.addBinding(vkDS(B_SUNANDSKY, vkDT::eUniformBuffer, 1,  flag));
    bind.addBinding(vkDS(B_HDR, vkDT::eCombinedImageSampler, 1, flag));  // HDR image
    bind.addBinding(vkDS(B_IMPORT_SMPL, vkDT::eCombinedImageSampler, 1, flag));  // importance sampling
    m_descSetLayout = bind.createLayout(m_device);
    m_descPool = bind.createPool(m_device, 1);
    m_descSet = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void EnvironmentalLight::updateDescriptorSet() {
    auto& bind = m_descBind;
    // Using the environment
    std::vector<vk::WriteDescriptorSet> writes;
    vk::DescriptorBufferInfo            sunskyDesc{ m_sunAndSkyBuffer.buffer, 0, VK_WHOLE_SIZE };
    writes.emplace_back(bind.makeWrite(m_descSet, B_SUNANDSKY, &sunskyDesc));
    writes.emplace_back(bind.makeWrite(m_descSet, B_HDR, &m_textures.txtHdr.descriptor));
    writes.emplace_back(bind.makeWrite(m_descSet, B_IMPORT_SMPL, &m_textures.accelImpSmpl.descriptor));

    m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
void EnvironmentalLight::destroy()
{
  m_alloc->destroy(m_textures.txtHdr);
  m_alloc->destroy(m_textures.accelImpSmpl);
  m_alloc->destroy(m_sunAndSkyBuffer);

  Descriptor::destroy();
}


//--------------------------------------------------------------------------------------------------
// Loading the HDR environment texture (HDR) and create the important accel structure
//
void EnvironmentalLight::loadEnvironment(const std::string& hrdImage)
{
  destroy();


  int width, height, component;

  float*         pixels     = stbi_loadf(hrdImage.c_str(), &width, &height, &component, STBI_rgb_alpha);
  vk::DeviceSize bufferSize = width * height * 4 * sizeof(float);
  vk::Extent2D   imgSize(width, height);


  vk::SamplerCreateInfo samplerCreateInfo{{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
  vk::Format            format = vk::Format::eR32G32B32A32Sfloat;
  vk::ImageCreateInfo   icInfo = nvvk::makeImage2DCreateInfo(imgSize, format);

  {
    // We are using a different index (1), to allow loading in a different
    // queue/thread that the display (0)
    auto                     queue = m_device.getQueue(m_graphicsQueueIndex, 1);
    nvvk::ScopeCommandBuffer cmdBuf(m_device, m_graphicsQueueIndex, queue);
    nvvk::Image              image  = m_alloc->createImage(cmdBuf, bufferSize, pixels, icInfo);
    vk::ImageViewCreateInfo  ivInfo = nvvk::makeImageViewCreateInfo(image.image, icInfo);
    m_textures.txtHdr               = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);
  }
  m_alloc->finalizeAndReleaseStaging();

  _createEnvironmentAccelTexture(pixels, imgSize, m_textures.accelImpSmpl);

  stbi_image_free(pixels);

  m_debug.setObjectName(m_textures.txtHdr.image, "SkyHdr");
  m_debug.setObjectName(m_textures.accelImpSmpl.image, "SkyImpSamp");

  m_sunAndSkyBuffer = m_alloc->createBuffer(sizeof(SunAndSky),
      vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_debug.setObjectName(m_sunAndSkyBuffer.buffer, "sunsky");

}

//--------------------------------------------------------------------------------------------------
// Build alias map for the importance sampling
//
float EnvironmentalLight::_build_alias_map(const std::vector<float>& data, std::vector<Env_accel>& accel)
{
  uint32_t size = static_cast<uint32_t>(data.size());

  // create qs (normalized)
  float sum = std::accumulate(data.begin(), data.end(), 0.f);

  // Ratio against average
  float fsize = static_cast<float>(size);
  for(uint32_t i = 0; i < size; ++i)
  {
    accel[i].q     = fsize * data[i] / sum;
    accel[i].alias = i;
  }

  // Create partition table. Table cut in half, first part
  // is below the normal and the other half is above the normal
  std::vector<uint32_t> partition_table(size);
  uint32_t              s     = 0u;
  uint32_t              large = size;
  for(uint32_t i = 0; i < size; ++i)
  {
    partition_table[(accel[i].q < 1.0f) ? (s++) : (--large)] = i;
  }

  // create alias map
  for(s = 0; s < large && large < size; ++s)
  {
    const uint32_t j = partition_table[s];
    const uint32_t k = partition_table[large];
    accel[j].alias   = k;
    accel[k].q += accel[j].q - 1.0f;
    large = (accel[k].q < 1.0f) ? (large + 1u) : large;
  }

  return sum;
}

inline float luminance(const float* color)
{
  return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

//--------------------------------------------------------------------------------------------------
// Create acceleration data for importance sampling
// See:  https://arxiv.org/pdf/1901.05423.pdf
void EnvironmentalLight::_createEnvironmentAccelTexture(const float* pixels, vk::Extent2D& size, nvvk::Texture& accelTex)
{
  const uint32_t rx = size.width;
  const uint32_t ry = size.height;

  // Create importance sampling data
  std::vector<Env_accel> env_accel(rx * ry);
  std::vector<float>     importance_data(rx * ry);
  float                  cos_theta0 = 1.0f;
  const float            step_phi   = float(2.0 * M_PI) / float(rx);
  const float            step_theta = float(M_PI) / float(ry);
  double                 total      = 0;
  for(uint32_t y = 0; y < ry; ++y)
  {
    const float theta1     = float(y + 1) * step_theta;
    const float cos_theta1 = std::cos(theta1);
    const float area       = (cos_theta0 - cos_theta1) * step_phi;  // solid angle
    cos_theta0             = cos_theta1;

    for(uint32_t x = 0; x < rx; ++x)
    {
      const uint32_t idx  = y * rx + x;
      const uint32_t idx4 = idx * 4;
      importance_data[idx] = area * std::max(pixels[idx4], std::max(pixels[idx4 + 1], pixels[idx4 + 2]));  // CIE luminance?
      total += luminance(&pixels[idx4]);
    }
  }

  m_average = static_cast<float>(total) / static_cast<float>(rx * ry);

  // Filling env_accel
  m_integral = _build_alias_map(importance_data, env_accel);

  // probability density function (PDF)
  const float inv_env_integral = 1.0f / m_integral;
  for(uint32_t i = 0; i < rx * ry; ++i)
  {
    const uint32_t idx4 = i * 4;
    env_accel[i].pdf    = std::max(pixels[idx4], std::max(pixels[idx4 + 1], pixels[idx4 + 2])) * inv_env_integral;
  }

  {
    // We are using a different index (1), to allow loading in a different
    // queue/thread that the display (0)
    auto                     queue = m_device.getQueue(m_graphicsQueueIndex, 1);
    nvvk::ScopeCommandBuffer cmdBuf(m_device, m_graphicsQueueIndex, queue);

    vk::SamplerCreateInfo samplerCreateInfo{};
    vk::Format            format     = vk::Format::eR32G32B32A32Sfloat;
    vk::ImageCreateInfo   icInfo     = nvvk::makeImage2DCreateInfo({rx, ry}, format);
    vk::DeviceSize        bufferSize = rx * ry * sizeof(Env_accel);

    nvvk::Image             image  = m_alloc->createImage(cmdBuf, bufferSize, env_accel.data(), icInfo);
    vk::ImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, icInfo);
    accelTex                       = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);
  }
  m_alloc->finalizeAndReleaseStaging();
}
