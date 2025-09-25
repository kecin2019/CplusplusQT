# CMakeLists.txt

# 设置最低CMake版本

cmake_minimum_required(VERSION 3.18)
project(MedYOLO11Qt LANGUAGES CXX)

# 设置Windows平台UTF-8编码支持
if(WIN32)
    add_compile_options(/utf-8)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 设置跨平台编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC编译选项
    add_compile_options(-Wall -Wextra -pedantic -Werror=return-type)
    # 对于较新版本的GCC，可以启用-Wsuggest-override
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0")
        add_compile_options(-Wsuggest-override)
    endif()
elseif(MSVC)
    # MSVC编译选项
    add_compile_options(/W4 /wd4251 /wd4275)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动 moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 选项（保持启用GDCM和ORT）
option(USE_GDCM "Enable DICOM reading via GDCM (libgdcm)" ON)
option(USE_ORT  "Enable ONNXRuntime for FAI classification" ON)

# ---------------- 跨平台查找Qt ----------------
if(WIN32)
    # Windows下可以手动指定Qt路径（可选）
    # set(QT_INSTALL_PATH "E:/tools/Qt/6.9.2/msvc2022_64")  # 替换为实际编译器目录
    # set(CMAKE_PREFIX_PATH ${QT_INSTALL_PATH})  # 告诉CMake从这里找Qt
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---------------- GDCM（DICOM库） ----------------
if(USE_GDCM)
  if(WIN32)
    # Windows下手动指定GDCM路径
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
  else()
    # Linux下使用系统包管理器安装的GDCM
    find_package(GDCM REQUIRED)
    set(GDCM_INCLUDE_DIRS_FALLBACK ${GDCM_INCLUDE_DIRS})
    list(APPEND GDCM_LIBS ${GDCM_LIBRARIES})
  endif()

  if((WIN32 AND GDCM_MSFF_LIB AND GDCM_COMMON_LIB AND GDCM_DSED_LIB AND EXISTS ${GDCM_INCLUDE_DIRS}) OR 
     (UNIX AND GDCM_FOUND))
    if(WIN32)
      message(STATUS "GDCM found at: ${GDCM_INSTALL_PATH}")
      list(APPEND GDCM_LIBS ${GDCM_MSFF_LIB} ${GDCM_COMMON_LIB} ${GDCM_DSED_LIB})
      set(GDCM_INCLUDE_DIRS_FALLBACK ${GDCM_INCLUDE_DIRS})
    endif()
    add_compile_definitions(HAVE_GDCM)  # 启用DICOM功能
  else()
    message(FATAL_ERROR "GDCM not found. Please install GDCM or disable with -DUSE_GDCM=OFF.")
    set(USE_GDCM OFF)
  endif()
endif()

# ---------------- ONNXRuntime ----------------
if(USE_ORT)
  if(WIN32)
    # Windows下手动指定ONNXRuntime路径
    set(ONNXRUNTIME_INSTALL_PATH "E:/tools/onnxruntime-win-x64-1.22.1")
    set(ONNXRUNTIME_INCLUDE_DIR ${ONNXRUNTIME_INSTALL_PATH}/include)
    set(ONNXRUNTIME_LIBRARY_DIR ${ONNXRUNTIME_INSTALL_PATH}/lib)
    
    # 查找 ONNXRuntime 库
    find_library(ONNXRUNTIME_LIB
      NAMES onnxruntime
      PATHS ${ONNXRUNTIME_LIBRARY_DIR}
      NO_DEFAULT_PATH  # 只在指定路径查找
    )
  else()
    # Linux下使用系统包管理器安装的ONNXRuntime或从源码构建
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
              PATHS /usr/include/onnxruntime /usr/local/include/onnxruntime)
    find_library(ONNXRUNTIME_LIB onnxruntime
                PATHS /usr/lib /usr/local/lib)
  endif()
  
  if(ONNXRUNTIME_LIB AND EXISTS ${ONNXRUNTIME_INCLUDE_DIR})
    if(WIN32)
      message(STATUS "ONNXRuntime found at: ${ONNXRUNTIME_INSTALL_PATH}")
    else()
      message(STATUS "ONNXRuntime found")
    endif()
    add_compile_definitions(HAVE_ORT)
  else()
    message(WARNING "ONNXRuntime not found. Disable with -DUSE_ORT=OFF.")
    set(USE_ORT OFF)
  endif()
endif()

# ---------------- 源文件 ----------------
file(GLOB SRC "src/*.cpp" "src/*.h")
# 移除了 src/resources.qrc 引用

# ---------------- 目标配置 ----------------
add_executable(medapp ${SRC})
target_include_directories(medapp PRIVATE 
  src 
  ${GDCM_INCLUDE_DIRS_FALLBACK}  # 添加GDCM头文件路径
  ${ONNXRUNTIME_INCLUDE_DIR}     # 添加ONNX头文件路径
)

# 链接库
target_link_libraries(medapp PRIVATE 
  Qt6::Widgets 
  ${GDCM_LIBS}       # 链接GDCM库
  ${ONNXRUNTIME_LIB} # 链接ONNX库
)

# 跨平台设置
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
  
  # GDCM DLLs
  file(GLOB GDCM_DLLS "${GDCM_INSTALL_PATH}/bin/gdcm*.dll")
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${GDCM_DLLS}
    $<TARGET_FILE_DIR:medapp>
  )
else()
  # Linux平台设置
  # 确保可执行文件有执行权限
  set_target_properties(medapp PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/lib"
  )
  
  # 在Linux下，通常不需要复制Qt和其他库，因为它们通过包管理器安装到系统中
endif()

# 跨平台复制模型文件
add_custom_command(TARGET medapp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
  $<TARGET_FILE_DIR:medapp>/models/
)
add_custom_command(TARGET medapp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${CMAKE_SOURCE_DIR}/models/fai_xray.onnx
  $<TARGET_FILE_DIR:medapp>/models/
)
add_custom_command(TARGET medapp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${CMAKE_SOURCE_DIR}/models/mri_segmentation.onnx
  $<TARGET_FILE_DIR:medapp>/models/
)

# 安装规则 (跨平台)
install(TARGETS medapp
  RUNTIME DESTINATION bin
)

# 跨平台安装模型文件
execute_process(COMMAND mkdir -p "${CMAKE_BINARY_DIR}/temp_install/models")
install(DIRECTORY models/ DESTINATION bin/models)

# 平台特定的安装规则
if(WIN32)
  # 安装依赖 DLL (Windows)
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
  # 在文件末尾添加加密工具
  add_executable(encrypt_model tools/encrypt_model.cpp)
  target_link_libraries(encrypt_model PRIVATE Qt6::Core)
  
  # 安装时复制加密后的模型文件
  install(FILES "${CMAKE_SOURCE_DIR}/models/fai_xray.encrypted"
          DESTINATION bin/models RENAME "fai_xray.onnx")
  install(FILES "${CMAKE_SOURCE_DIR}/models/mri_segmentation.onnx"
          DESTINATION bin/models)
  
  # install(FILES "${CMAKE_SOURCE_DIR}/models/fai_xray.onnx"
  #         DESTINATION bin/models)
else()
  # Linux平台安装规则
  # 在Linux上，通常通过包管理器安装依赖，不需要复制库文件
  # 创建运行脚本帮助用户启动应用
  install(CODE "\n    file(WRITE \"$ENV{DESTDIR}\\${CMAKE_INSTALL_PREFIX}/bin/run_medapp.sh\" \"
      #!/bin/sh\\n\
      cd \"$(dirname \\"$0\\")\"\\n\
      ./medapp\\n\"
    )\n    execute_process(COMMAND chmod +x \"$ENV{DESTDIR}\\${CMAKE_INSTALL_PREFIX}/bin/run_medapp.sh\")\n  ")
  
  # 添加桌面图标支持
  install(CODE "\n    file(WRITE \"$ENV{DESTDIR}\\${CMAKE_INSTALL_PREFIX}/share/applications/medyolo11qt.desktop\" \"
      [Desktop Entry]\\n\
      Type=Application\\n\
      Name=MedYOLO11Qt\\n\
      Comment=Medical Image Analysis Software with AI Classification\\n\
      Exec=\"$CMAKE_INSTALL_PREFIX/bin/run_medapp.sh\"\\n\
      Icon=utilities-terminal\\n\
      Terminal=false\\n\
      Categories=Science;MedicalSoftware;Graphics;\\n\"
    )\n  ")
endif()

# CPack 配置（用于生成安装包）
set(CPACK_PACKAGE_NAME "MedYOLO11Qt")
set(CPACK_PACKAGE_VENDOR "Your Company")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Medical Image Analysis Software with AI Classification")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DICOM viewer with FAI classification using YOLO")

# 平台特定的CPack配置
if(WIN32)
  # Windows 特定配置
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
  set(CPACK_DESKTOP_SHORTCUTS
      "MedYOLO11Qt" "bin/medapp.exe"
  )
  
  # 手动指定NSIS编译器路径
  set(CMAKE_NSIS_COMPILER "E:/tools/NSIS/makensis.exe")
else()
  # Linux 特定配置
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_PACKAGE_NAME "medyolo11qt")
  set(CPACK_PACKAGE_CONTACT "Your Name <your.email@example.com>")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.email@example.com>")
  
  # 指定Debian包依赖
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-base (>= 6.0), libgdcm3.0 (>= 3.0), onnxruntime (>= 1.10)")
  
  # 设置安装路径前缀
  if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX /usr/local)
  endif()
endif()

include(CPack)