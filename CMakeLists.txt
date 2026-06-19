cmake_minimum_required(VERSION 3.24)

project(CUDA_Project LANGUAGES CUDA CXX)

# ===== standarts =====
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# ===== CUDA arch  =====
set(CMAKE_CUDA_ARCHITECTURES native)

# all CUDA files in root dir
    file(GLOB CUDA_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cu
    )
# ===== executable =====
    add_executable(run ${CUDA_SOURCES} "pinned_buffer.h")
# ===== includes =====
    target_include_directories(run PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
# ===== compile definitions =====
    target_compile_definitions(run PRIVATE
        NT=8
        NGSIZE=128
    )
# ===== CUDA flags =====
target_compile_options(run PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:--use_fast_math>
    >
)


if(FALSE)
    cmake_minimum_required(VERSION 3.24)

    project(CUDA_Project LANGUAGES CXX CUDA)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_ARCHITECTURES 86)

    # includes
    include_directories(include)

    # core sources
    set(CORE_SOURCES
        src/main.cu
        src/core/run.cu
        src/core/init.cu
        src/core/update.cu
        src/core/drop.cu
    )

    # optional variants
    set(VARIANTS
        src/variants/stepwise-simple.cu
        src/variants/stepwise-compact.cu
    )

    add_executable(run ${CORE_SOURCES})

    target_compile_definitions(run PRIVATE
        NT=8
        NGSIZE=128
    )

    target_compile_options(run PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--use_fast_math>
    )
endif()