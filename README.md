Vulkan-based polarimetric BRDF (pBRDF) renderer by path tracing friendly to NVIDIA Vulkan Wrapper
================

Demo (Link to YouTube)
[![](http://img.youtube.com/vi/Urx1Sg8SRy8/0.jpg)](http://www.youtube.com/watch?v=Urx1Sg8SRy8 "demo")

## 1. Explanation
 This code is pBRDF renderer by path tracing on Vulkan API with [NVIDIA Wrapper](https://github.com/nvpro-samples)

## 2. Requirements on Windows
- Visual Studio 2019
- Vulkan SDK above version 1.2.162.0.
- NVIDIA graphics board that supports `VK_KHR_ray_tracing_pipeline` extension and corresponding drivers (latest recommended)
- CMake above version 3.9.6.

The code has not been tested on other platforms.

## 3. Setup
 1. Clone this repository as follows,
   ```
   git clone https://github.com/dipmizu914/vk_pBRDF.git --recurse-submodules
   ```
 2. Build by using CMake.


## 4. Fix third-party code to read the index of refraction (ior)
  1. Add transmission extension name to `thirdparty/shared_sources/nvh/gltfscene.hpp` as follows,
  ```cpp
   // Line 68-
   #define KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME "KHR_lights_punctual"
   #define KHR_TEXTURE_TRANSFORM_EXTENSION_NAME "KHR_texture_transform"
   #define KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME "KHR_materials_pbrSpecularGlossiness"
   #define KHR_MATERIALS_UNLIT_EXTENSION_NAME "KHR_materials_unlit"
   #define KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME "KHR_materials_anisotropy"
   #define KHR_MATERIALS_TRANSMISSION_NAME "KHR_materials_transmission"
   ```

  2. Add ior property for material to `thirdparty/shared_sources/nvh/gltfscene.hpp` as follows,
  ```cpp
   // Line 112- in struct GltfMaterial
    float anisotropy{0};
    vec3  anisotropyDirection{1, 0, 0};
    int   anisotropyTexture{-1};

    float ior{ 1.45 };
    }
   ```
  3. Add ior reader to `void GltfScene::importMaterials(const tinygltf::Model& tmodel)` function in `thirdparty/shared_sources/nvh/gltfscene.cpp` as follows,
  ```cpp
   // Line 112- in void GltfScene::importMaterials(const tinygltf::Model& tmodel)
        if (khr.Has("anisotropyTexture"))
        {
          gmat.anisotropyTexture = khr.Get("anisotropyTexture").Get("index").Get<int>();
        }
      }

      if (tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_NAME) != tmat.extensions.end()) {
        const auto& khr = tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_NAME)->second;
        if (khr.Has("ior"))
        {
          gmat.ior = static_cast<float>(khr.Get("ior").Get<double>());
        }
      }
      m_materials.emplace_back(gmat);
    }
   ```