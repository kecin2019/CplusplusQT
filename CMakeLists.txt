# MedYOLO11Qt - 医学图像 AI 分析软件
# CMake 构建配置文件
# 基于 Qt 6 + ONNXRuntime + GDCM 的现代化医学影像处理平台

cmake_minimum_required(VERSION 3.18)
project(MedYOLO11Qt 
  VERSION 1.0.0
  DESCRIPTION "Medical Image Analysis Software with YOLO11 AI Classification"
  LANGUAGES CXX
)

# =============================================================================
# 基础配置
# =============================================================================

# C++ 标准配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 自动化工具配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 构建类型配置
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# =============================================================================
# 功能选项配置
# =============================================================================

# 核心功能开关
option(USE_GDCM "启用 DICOM 医学影像支持 (通过 GDCM 库)" ON)
option(USE_ORT  "启用 ONNXRuntime AI 推理引擎" ON)

# Windows 平台特定配置
if(WIN32)
  # GUI 应用程序配置
  set(CMAKE_WIN32_EXECUTABLE ON)
  
  # 运行时库配置
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# =============================================================================
# Qt 6 框架配置
# =============================================================================

# Qt 安装路径配置（根据实际安装位置调整）
set(QT_INSTALL_PATH "E:/tools/Qt/6.9.2/msvc2022_64" CACHE PATH "Qt 6 安装路径")
set(CMAKE_PREFIX_PATH ${QT_INSTALL_PATH})

# 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

# Qt 版本信息输出
message(STATUS "Qt 版本: ${Qt6_VERSION}")
message(STATUS "Qt 安装路径: ${QT_INSTALL_PATH}")

# =============================================================================
# GDCM 医学影像库配置
# =============================================================================

if(USE_GDCM)
  message(STATUS "正在配置 GDCM 医学影像库...")
  
  # GDCM 安装路径
  set(GDCM_INSTALL_PATH "E:/tools/GDCM-3.2.0-Windows-x86_64" CACHE PATH "GDCM 安装路径")
  set(GDCM_INCLUDE_DIRS ${GDCM_INSTALL_PATH}/include/gdcm-3.2)
  set(GDCM_LIBRARY_DIR ${GDCM_INSTALL_PATH}/lib)
  
  # 查找 GDCM 核心库
  find_library(GDCM_MSFF_LIB
    NAMES gdcmMSFF
    PATHS ${GDCM_LIBRARY_DIR} 
    NO_DEFAULT_PATH
  )
  
  find_library(GDCM_COMMON_LIB
    NAMES gdcmCommon
    PATHS ${GDCM_LIBRARY_DIR}
    NO_DEFAULT_PATH
  )
  
  find_library(GDCM_DSED_LIB
    NAMES gdcmDSED
    PATHS ${GDCM_LIBRARY_DIR}
    NO_DEFAULT_PATH
  )

  # 验证 GDCM 库
  if(GDCM_MSFF_LIB AND GDCM_COMMON_LIB AND GDCM_DSED_LIB AND EXISTS ${GDCM_INCLUDE_DIRS})
    message(STATUS "✅ GDCM 配置成功")
    message(STATUS "   安装路径: ${GDCM_INSTALL_PATH}")
    list(APPEND GDCM_LIBS ${GDCM_MSFF_LIB} ${GDCM_COMMON_LIB} ${GDCM_DSED_LIB})
    set(GDCM_INCLUDE_DIRS_FALLBACK ${GDCM_INCLUDE_DIRS})
    add_compile_definitions(HAVE_GDCM)
  else()
    message(FATAL_ERROR "❌ GDCM 库未找到，请检查安装路径: ${GDCM_INSTALL_PATH}")
    set(USE_GDCM OFF)
  endif()
endif()

# ---------------- GDCM（DICOM库） ----------------
if(USE_GDCM)
  # 手动指定 GDCM 路径
set(GDCM_INSTALL_PATH "E:/tools/GDCM-3.2.0-Windows-x86_64")
set(GDCM_INCLUDE_DIRS ${GDCM_INSTALL_PATH}/include/gdcm-3.2)
set(GDCM_LIBRARY_DIR ${GDCM_INSTALL_PATH}/lib)
  
  # 查找 GDCM 库
  find_library(GDCM_MSFF_LIB
    NAMES gdcmMSFF
    PATHS ${GDCM_LIBRARY_DIR} 
    NO_DEFAULT_PATH  # 只在指定路径查找
  )
  find_library(GDCM_COMMON_LIB
    NAMES gdcmCommon
    PATHS ${GDCM_LIBRARY_DIR}
    NO_DEFAULT_PATH
  )
  find_library(GDCM_DSED_LIB
    NAMES gdcmDSED
    PATHS ${GDCM_LIBRARY_DIR}
    NO_DEFAULT_PATH
  )

  if(GDCM_MSFF_LIB AND GDCM_COMMON_LIB AND GDCM_DSED_LIB AND EXISTS ${GDCM_INCLUDE_DIRS})
    message(STATUS "GDCM found at: ${GDCM_INSTALL_PATH}")
    list(APPEND GDCM_LIBS ${GDCM_MSFF_LIB} ${GDCM_COMMON_LIB} ${GDCM_DSED_LIB})
    set(GDCM_INCLUDE_DIRS_FALLBACK ${GDCM_INCLUDE_DIRS})
    add_compile_definitions(HAVE_GDCM)  # 启用DICOM功能
  else()
    message(FATAL_ERROR "GDCM not found in ${GDCM_INSTALL_PATH}. Check path.")
    set(USE_GDCM OFF)
  endif()
endif()

# ---------------- ONNXRuntime ----------------
# 手动指定 ONNXRuntime 路径
set(ONNXRUNTIME_INSTALL_PATH "E:/tools/onnxruntime-win-x64-1.22.1")
set(ONNXRUNTIME_INCLUDE_DIR ${ONNXRUNTIME_INSTALL_PATH}/include)
set(ONNXRUNTIME_LIBRARY_DIR ${ONNXRUNTIME_INSTALL_PATH}/lib)

if(USE_ORT)
  # 查找 ONNXRuntime 库
  find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS ${ONNXRUNTIME_LIBRARY_DIR}
    NO_DEFAULT_PATH  # 只在指定路径查找
  )
  
  if(ONNXRUNTIME_LIB AND EXISTS ${ONNXRUNTIME_INCLUDE_DIR})
    message(STATUS "ONNXRuntime found at: ${ONNXRUNTIME_INSTALL_PATH}")
    add_compile_definitions(HAVE_ORT)
  else()
    message(WARNING "ONNXRuntime not found. Disable with -DUSE_ORT=OFF.")
    set(USE_ORT OFF)
  endif()
endif()

# ---------------- 源文件 ----------------
# 新的模块化源文件结构
file(GLOB CORE_SRC "src/core/*.cpp" "src/core/*.h")
file(GLOB AI_SRC "src/ai/*.cpp" "src/ai/*.h") 
file(GLOB UI_SRC "src/ui/*.cpp" "src/ui/*.h")
file(GLOB MEDICAL_SRC "src/medical/*.cpp" "src/medical/*.h")

# 合并所有源文件
set(SRC ${CORE_SRC} ${AI_SRC} ${UI_SRC} ${MEDICAL_SRC})

# ---------------- 目标配置 ----------------
add_executable(medapp ${SRC})
target_include_directories(medapp PRIVATE 
  src 
  src/core
  src/ai
  src/ui
  src/medical
  include
  ${GDCM_INCLUDE_DIRS_FALLBACK}  # 添加GDCM头文件路径
  ${ONNXRUNTIME_INCLUDE_DIR}     # 添加ONNX头文件路径
)

# 链接库
target_link_libraries(medapp PRIVATE 
  Qt6::Widgets 
  Qt6::Concurrent
  ${GDCM_LIBS}       # 链接GDCM库
  ${ONNXRUNTIME_LIB} # 链接ONNX库
)

# Windows 部分
if(WIN32)
  # 设置 Windows 子系统（GUI）
  set_target_properties(medapp PROPERTIES
    WIN32_EXECUTABLE ON
  )
  
  # 复制运行时依赖（Qt + ONNX）
  # Qt DLLs
  # 注意：路径需要根据你的实际Qt安装目录调整
  set(QT_DLL_DIR "${QT_INSTALL_PATH}/bin")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB QT_DLLS "${QT_DLL_DIR}/Qt6Widgetsd.dll" "${QT_DLL_DIR}/Qt6Cored.dll" "${QT_DLL_DIR}/Qt6Guid.dll")
  else()
    file(GLOB QT_DLLS "${QT_DLL_DIR}/Qt6Widgets.dll" "${QT_DLL_DIR}/Qt6Core.dll" "${QT_DLL_DIR}/Qt6Gui.dll")
  endif()
  
  # ONNXRuntime DLL
  set(ONNXRUNTIME_DLL "${ONNXRUNTIME_LIBRARY_DIR}/onnxruntime.dll")
  
  # 复制到输出目录
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${QT_DLLS} ${ONNXRUNTIME_DLL}
    $<TARGET_FILE_DIR:medapp>
  )
  
  # 复制Qt平台插件
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET medapp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:medapp>/platforms/
    )
    add_custom_command(TARGET medapp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${QT_INSTALL_PATH}/plugins/platforms/qwindowsd.dll"
      $<TARGET_FILE_DIR:medapp>/platforms/
    )
  else()
    add_custom_command(TARGET medapp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:medapp>/platforms/
    )
    add_custom_command(TARGET medapp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${QT_INSTALL_PATH}/plugins/platforms/qwindows.dll"
      $<TARGET_FILE_DIR:medapp>/platforms/
    )
  endif()
  # 复制模型文件 - 更新路径
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:medapp>/models/
  )
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/models/original/fai_xray.onnx
    $<TARGET_FILE_DIR:medapp>/models/
  )
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/models/original/mri_segmentation.onnx
    $<TARGET_FILE_DIR:medapp>/models/
  )
  
  # GDCM DLLs
  file(GLOB GDCM_DLLS "${GDCM_INSTALL_PATH}/bin/gdcm*.dll")
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${GDCM_DLLS}
    $<TARGET_FILE_DIR:medapp>
  )
endif()

# 安装规则 (可选)
install(TARGETS medapp
  RUNTIME DESTINATION bin
)

# 安装依赖 DLL (Windows)
if(WIN32)
  # Qt
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB QT_DEBUG_DLLS "${QT_DLL_DIR}/Qt6Widgetsd.dll" "${QT_DLL_DIR}/Qt6Cored.dll" "${QT_DLL_DIR}/Qt6Guid.dll")
    install(FILES ${QT_DEBUG_DLLS} DESTINATION bin)
  else()
    file(GLOB QT_RELEASE_DLLS "${QT_DLL_DIR}/Qt6Widgets.dll" "${QT_DLL_DIR}/Qt6Core.dll" "${QT_DLL_DIR}/Qt6Gui.dll")
    install(FILES ${QT_RELEASE_DLLS} DESTINATION bin)
  endif()
  # ONNXRuntime
  install(FILES ${ONNXRUNTIME_DLL} DESTINATION bin)
  # GDCM
  file(GLOB GDCM_DLLS "${GDCM_INSTALL_PATH}/bin/gdcm*.dll")
  install(FILES ${GDCM_DLLS} DESTINATION bin)
  
  # 安装平台插件
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    install(DIRECTORY "${QT_INSTALL_PATH}/plugins/platforms/" 
            DESTINATION bin/platforms
            FILES_MATCHING PATTERN "*d.dll")
  else()
    install(DIRECTORY "${QT_INSTALL_PATH}/plugins/platforms/" 
            DESTINATION bin/platforms
            FILES_MATCHING PATTERN "*.dll")
  endif()
  
  # 安装模型文件
  # 加密工具 - 更新路径
  add_executable(encrypt_model scripts/maintenance/tools/encrypt_model.cpp)
  target_link_libraries(encrypt_model PRIVATE Qt6::Core)
  
  # 安装时复制模型文件 - 更新路径
  install(FILES "${CMAKE_SOURCE_DIR}/models/original/fai_xray.onnx"
          DESTINATION bin/models)
  install(FILES "${CMAKE_SOURCE_DIR}/models/original/mri_segmentation.onnx"
          DESTINATION bin/models)
  # 安装加密模型
  if(EXISTS "${CMAKE_SOURCE_DIR}/models/encrypted/fai_xray.encrypted")
    install(FILES "${CMAKE_SOURCE_DIR}/models/encrypted/fai_xray.encrypted"
            DESTINATION bin/models RENAME "fai_xray.onnx")
  endif()
  
  # install(FILES "${CMAKE_SOURCE_DIR}/models/fai_xray.onnx"
  #         DESTINATION bin/models)
endif()

# CPack 配置（用于生成安装包）
set(CPACK_PACKAGE_NAME "MedYOLO11Qt")
set(CPACK_PACKAGE_VENDOR "Your Company")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Medical Image Analysis Software with AI Classification")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DICOM viewer with FAI classification using YOLO")

# Windows 特定配置
if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "MedYOLO11Qt")
  set(CPACK_NSIS_DISPLAY_NAME "MedYOLO11Qt")
  set(CPACK_NSIS_PACKAGE_NAME "MedYOLO11Qt")
  set(CPACK_NSIS_UNINSTALL_NAME "uninstall")
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MENU_LINKS
      "bin/medapp.exe" "MedYOLO11Qt"
  )
  set(CPACK_NSIS_CREATE_ICONS_EXTRA 
      "CreateShortCut \"\$SMPROGRAMS\\MedYOLO11Qt.lnk\" \"\$INSTDIR\\bin\\medapp.exe\""
      "CreateShortCut \"\$DESKTOP\\MedYOLO11Qt.lnk\" \"\$INSTDIR\\bin\\medapp.exe\""
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA 
      "Delete \"\$SMPROGRAMS\\MedYOLO11Qt.lnk\""
      "Delete \"\$DESKTOP\\MedYOLO11Qt.lnk\""
  )
endif()

# 手动指定NSIS编译器路径
set(CMAKE_NSIS_COMPILER "E:/tools/NSIS/makensis.exe")
include(CPack)

