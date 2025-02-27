# find_package(Vulkan REQUIRED)

add_library(glm INTERFACE)
add_library(vma INTERFACE)
add_library(json INTERFACE)

add_library(stb_image INTERFACE)
add_library(cgltf INTERFACE)
add_library(reflection INTERFACE)
add_library(volk INTERFACE)
add_library(tinygltf INTERFACE)

add_library(lz4 STATIC)
add_library(fmt_lib STATIC)
add_library(spirv_reflect STATIC)
add_library(inicpp STATIC)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)
target_include_directories(glm INTERFACE glm)
target_include_directories(json INTERFACE json)
target_include_directories(cgltf INTERFACE cgltf)
target_include_directories(reflection INTERFACE reflection)
target_include_directories(volk INTERFACE volk vulkan_base)
target_include_directories(tinygltf INTERFACE tinygltf)

target_sources(lz4 PRIVATE
    lz4/lz4.h
    lz4/lz4.c
    )

target_include_directories(lz4 PUBLIC lz4)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)
file(GLOB IMGUI_FILES "imgui/*")
target_sources(imgui PRIVATE ${IMGUI_FILES})
target_include_directories(imgui PUBLIC vulkan_base volk)

target_include_directories(stb_image INTERFACE stb_image)

target_include_directories(fmt_lib PUBLIC fmt_lib)
file(GLOB FMT_FILES "fmt_lib/fmt/*")
target_sources(fmt_lib PRIVATE ${FMT_FILES})

target_include_directories(spirv_reflect PUBLIC spirv_reflect)
target_sources(spirv_reflect PRIVATE
	spirv_reflect/spirv_reflect.h
	spirv_reflect/spirv_reflect.c
	)

target_include_directories(inicpp PUBLIC inicpp)
file(GLOB INICPP_FILES "inicpp/*")
target_sources(inicpp PRIVATE ${INICPP_FILES})

add_library(glad STATIC)
target_include_directories(glad PUBLIC glad)
target_sources(glad PRIVATE 
    glad/glad/glad.h
    glad/src/glad.c
    )
    
add_library(zstd STATIC)
target_include_directories(zstd PUBLIC basis_universal)
target_sources(zstd PRIVATE 
    basis_universal/zstd/zstd.h 
    basis_universal/zstd/zstd.c
    )
    
add_library(basisu STATIC)
target_include_directories(basisu PUBLIC basis_universal)
file(GLOB BASISU_FILES
    basis_universal/zstd/*.h
    basis_universal/zstd/zstd.с
    basis_universal/encoder/*.h
    basis_universal/encoder/*.cpp
    basis_universal/transcoder/*.h
    basis_universal/transcoder/*.cpp
    )
target_sources(basisu PRIVATE ${BASISU_FILES})
target_link_libraries(basisu zstd)
    