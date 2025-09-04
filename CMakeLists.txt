cmake_minimum_required(VERSION 3.18)
project(MedYOLO11Qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动 moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 选项
option(USE_GDCM "Enable DICOM reading via GDCM (libgdcm)" ON)
option(USE_ORT  "Enable ONNXRuntime for FAI classification" ON)

# Qt
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 源码（按你工程结构）
set(SRC
  src/main.cpp
  src/MainWindow.h src/MainWindow.cpp
  src/ImageView.h src/ImageView.cpp
  src/InferenceEngine.h src/InferenceEngine.cpp
  src/DicomUtils.h src/DicomUtils.cpp
  src/MetaTable.h src/MetaTable.cpp
)

# ---------------- GDCM（可选，兼容多种安装方式） ----------------
set(GDCM_LIBS "")
set(GDCM_INCLUDE_DIRS_FALLBACK "")

if(USE_GDCM)
  # 先找 CONFIG（现代包），找不到再用模块模式
  find_package(GDCM QUIET CONFIG)
  if(NOT GDCM_FOUND)
    find_package(GDCM QUIET) # module mode
  endif()

  if(GDCM_FOUND)
    message(STATUS "GDCM found: ${GDCM_VERSION}")
    if(TARGET GDCM::gdcmMSFF)
      list(APPEND GDCM_LIBS GDCM::gdcmMSFF)
    elseif(TARGET gdcmMSFF)
      list(APPEND GDCM_LIBS gdcmMSFF)
    elseif(DEFINED GDCM_LIBRARIES)
      list(APPEND GDCM_LIBS ${GDCM_LIBRARIES})
      if(DEFINED GDCM_INCLUDE_DIRS)
        set(GDCM_INCLUDE_DIRS_FALLBACK ${GDCM_INCLUDE_DIRS})
      endif()
    else()
      find_library(GDCM_MSFF_LIB NAMES gdcmMSFF)
      if(GDCM_MSFF_LIB)
        list(APPEND GDCM_LIBS ${GDCM_MSFF_LIB})
      else()
        message(WARNING "GDCM located but 'gdcmMSFF' library not found; disabling DICOM.")
        set(USE_GDCM OFF)
      endif()
    endif()

    if(USE_GDCM)
      add_compile_definitions(HAVE_GDCM)
    endif()
  else()
    message(WARNING "GDCM not found. DICOM disabled. Install libgdcm-dev or set -DUSE_GDCM=OFF.")
    set(USE_GDCM OFF)
  endif()
endif()

# ---------------- ONNXRuntime（可选，FAI 分型） ----------------
# 默认兜底安装前缀；可在配置时用 -DONNXRUNTIME_DIR=... 覆盖
set(ONNXRUNTIME_DIR "/opt/onnxruntime-1.22.0" CACHE PATH "ONNX Runtime prefix")
message(STATUS "CMAKE ONNXRUNTIME_DIR = ${ONNXRUNTIME_DIR}")

# 允许用户显式传入 ONNXRUNTIME_INCLUDE_DIR / ONNXRUNTIME_LIB；否则自动查找
if(USE_ORT)
  if(NOT DEFINED ONNXRUNTIME_INCLUDE_DIR)
    find_path(ONNXRUNTIME_INCLUDE_DIR
      NAMES onnxruntime_cxx_api.h
      HINTS
        ${ONNXRUNTIME_DIR}
        $ENV{ONNXRUNTIME_DIR}
        $ENV{ONNXRUNTIME_ROOT}
        /opt/onnxruntime-1.22.0
        /opt/onnxruntime
      PATH_SUFFIXES
        include
        include/onnxruntime
        include/onnxruntime/core/session
    )
  endif()

  if(NOT DEFINED ONNXRUNTIME_LIB)
    find_library(ONNXRUNTIME_LIB
      NAMES onnxruntime onnxruntime_shared
      HINTS
        ${ONNXRUNTIME_DIR}
        $ENV{ONNXRUNTIME_DIR}
        $ENV{ONNXRUNTIME_ROOT}
        /opt/onnxruntime-1.22.0
        /opt/onnxruntime
      PATH_SUFFIXES
        lib
        lib64
    )
  endif()

  message(STATUS "ONNXRUNTIME_INCLUDE_DIR = ${ONNXRUNTIME_INCLUDE_DIR}")
  message(STATUS "ONNXRUNTIME_LIB = ${ONNXRUNTIME_LIB}")

  if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    add_compile_definitions(HAVE_ORT)
  else()
    message(WARNING "ONNXRuntime not found. Set ONNXRUNTIME_DIR or pass ONNXRUNTIME_INCLUDE_DIR/ONNXRUNTIME_LIB explicitly.")
    set(USE_ORT OFF)
  endif()
endif()

# ---------------- 目标 ----------------
add_executable(medapp ${SRC})
target_include_directories(medapp PRIVATE src)

# 补充 include（老式 GDCM 包/手动路径）
if(GDCM_INCLUDE_DIRS_FALLBACK)
  target_include_directories(medapp PRIVATE ${GDCM_INCLUDE_DIRS_FALLBACK})
endif()

# ONNXRuntime 头文件
if(ONNXRUNTIME_INCLUDE_DIR)
  target_include_directories(medapp PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
endif()

# 链接库
target_link_libraries(medapp PRIVATE Qt6::Widgets ${GDCM_LIBS} ${ONNXRUNTIME_LIB})

# 运行时 RPATH：把 ORT 的库目录写进可执行文件，免 LD_LIBRARY_PATH
if(ONNXRUNTIME_LIB AND NOT WIN32)
  get_filename_component(ORT_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
  set_target_properties(medapp PROPERTIES
    BUILD_RPATH "${ORT_LIB_DIR}"
    INSTALL_RPATH "${ORT_LIB_DIR}")
endif()

# Windows/macOS 的部署提示（可选）
if (WIN32)
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Tip: use windeployqt to copy Qt runtime DLLs.")
elseif(APPLE)
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Tip: use macdeployqt to bundle the app.")
endif()
