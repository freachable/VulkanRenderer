cmake_minimum_required(VERSION 3.29)

project(VulkanRenderer LANGUAGES CXX)

option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

# Scan for C++ module dependencies only if C++ 20 module is enabled
# (CMake would otherwise scan every C++20 source by default)
set(CMAKE_CXX_SCAN_FOR_MODULES ${ENABLE_CPP20_MODULE})

# ---------------------------------------------------------------------------
# Dependencies (all provided by vcpkg, see vcpkg.json)
# ---------------------------------------------------------------------------
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Vulkan 1.4 REQUIRED)

# slangc comes from the vcpkg "shader-slang" host dependency. The hint makes
# sure it wins over a Vulkan SDK or distro copy that might also be on PATH.
find_program(SLANGC_EXECUTABLE slangc
  HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/shader-slang"
  REQUIRED
)

# Validation layers come from the vcpkg "vulkan-validationlayers" port. The
# loader only finds them through VK_ADD_LAYER_PATH; the app sets that at
# startup (debug builds only) unless it is already set in the environment.
if(WIN32)
  set(VALIDATION_LAYER_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
else()
  set(VALIDATION_LAYER_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/vulkan/explicit_layer.d")
endif()

# ---------------------------------------------------------------------------
# Vulkan-Hpp, optionally as a C++20 module
# ---------------------------------------------------------------------------
if(ENABLE_CPP20_MODULE)
  add_library(VulkanCppModule)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)

  target_compile_definitions(VulkanCppModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
  target_include_directories(VulkanCppModule PUBLIC "${Vulkan_INCLUDE_DIR}")
  target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)

  set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

  # Add MSVC-specific compiler options for proper C++ module support
  if(MSVC)
    target_compile_options(VulkanCppModule PRIVATE
      /std:c++latest      # Use latest C++ standard for better module support
      /permissive-        # Standards conformance mode
      /Zc:__cplusplus     # Enable correct __cplusplus macro
      /EHsc               # Enable C++ exception handling
      /Zc:preprocessor    # Use conforming preprocessor
      /translateInclude   # Automatically translate #include to import for standard library
    )
  endif()

  target_sources(VulkanCppModule
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS "${Vulkan_INCLUDE_DIR}"
    FILES "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )
else()
  # Interface library carrying the same settings when modules are disabled
  add_library(VulkanCppModule INTERFACE)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)
  target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
  target_compile_definitions(VulkanCppModule
    INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
  )
endif()

# ---------------------------------------------------------------------------
# Output layout: the executable and the compiled shaders live in <build>/bin
# (or <build>/bin/<config> for multi-config generators). The absolute shader
# path is compiled into the app, so it works from any working directory.
# ---------------------------------------------------------------------------
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(IS_MULTI_CONFIG)
  set(RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
else()
  set(RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set(SHADER_SPV "${RUNTIME_DIR}/shaders/slang.spv")

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
  src/main.cpp
  src/Application.cpp
  src/graphics/VulkanContext.cpp
  src/graphics/Swapchain.cpp
  src/graphics/Shader.cpp
  src/graphics/GraphicsPipeline.cpp
  src/graphics/Buffer.cpp
  src/graphics/Image.cpp
  src/graphics/Texture.cpp
  src/graphics/DepthBuffer.cpp
  src/graphics/Command.cpp
  src/graphics/FrameData.cpp
  src/graphics/RenderContext.cpp
  src/graphics/Renderer.cpp
  src/graphics/Mesh.cpp
  src/assets/ObjLoader.cpp
  src/platform/Window.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Vulkan::cppm
  glfw
  glm::glm
  tinyobjloader::tinyobjloader
)
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE
  # Treat VK_ERROR_OUT_OF_DATE_KHR as a success code
  VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
  "VR_VALIDATION_LAYER_DIR=\"${VALIDATION_LAYER_DIR}\""
  "VR_SHADER_PATH=\"${SHADER_SPV}\""
  "VR_TEXTURE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/textures/viking_room.png\""
  "VR_MODEL_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/models/viking_room.obj\""

  # Vulkan's clip-space depth is [0, 1], not OpenGL's [-1, 1]. Must be set before any glm include.
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  $<$<BOOL:${ENABLE_CPP20_MODULE}>:USE_CPP20_MODULES=1>

  # MSVC flags std::getenv as deprecated
  $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

# ---------------------------------------------------------------------------
# Shaders
# ---------------------------------------------------------------------------
function(add_slang_shader_target TARGET)
  cmake_parse_arguments(SHADER "" "OUTPUT" "SOURCES" ${ARGN})

  if(NOT SHADER_SOURCES)
    message(FATAL_ERROR "No Slang shader sources provided")
  endif()

  get_filename_component(SHADER_OUTPUT_DIR "${SHADER_OUTPUT}" DIRECTORY)

  add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${SLANGC_EXECUTABLE}"
            ${SHADER_SOURCES}
            -entry vertMain
            -entry fragMain
            -o "${SHADER_OUTPUT}"
    DEPENDS ${SHADER_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Compiling Slang shaders"
    VERBATIM
  )

  add_custom_target(${TARGET} DEPENDS "${SHADER_OUTPUT}")
endfunction()

add_slang_shader_target(VulkanRendererShaders
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang"
  OUTPUT  "${SHADER_SPV}"
)
add_dependencies(${PROJECT_NAME} VulkanRendererShaders)
