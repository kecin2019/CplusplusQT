# CMakeLists.txt

# 设置最低CMake版本

cmake_minimum_required(VERSION 3.18)
project(MedYOLO11Qt LANGUAGES CXX)

# 设置Windows平台UTF-8编码支持
# if(WIN32)
#     add_compile_options(/utf-8)
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
#     add_compile_definitions(UNICODE _UNICODE)
# endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动 moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 选项（保持启用GDCM和ORT）
option(USE_GDCM "Enable DICOM reading via GDCM (libgdcm)" ON)
option(USE_ORT  "Enable ONNXRuntime for FAI classification" ON)

# ---------------- 手动指定 Qt 路径 ----------------
# 注意：Qt路径需精确到编译器目录（根据你的VS版本选择，如msvc2022_64）
set(QT_INSTALL_PATH "E:/tools/Qt/6.9.2/msvc2022_64")  # 替换为实际编译器目录
set(CMAKE_PREFIX_PATH ${QT_INSTALL_PATH})  # 告诉CMake从这里找Qt
find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
  # 复制模型文件
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:medapp>/models/
  )
  add_custom_command(TARGET medapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/models/fai_xray.onnx
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
  install(FILES "${CMAKE_SOURCE_DIR}/models/fai_xray.onnx"
          DESTINATION bin/models)
endif()

# CPack 配置（用于生成安装包）
set(CPACK_PACKAGE_NAME "MedYOLO11Qt")
set(CPACK_PACKAGE_VENDOR "Your Company")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Medical Image Analysis Software with AI Classification")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DICOM viewer with FAI classification using YOLO")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://yourcompany.com")
set(CPACK_PACKAGE_CONTACT "support@yourcompany.com")

# Windows 特定配置
if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "MedYOLO11Qt")
  set(CPACK_NSIS_DISPLAY_NAME "MedYOLO11Qt")
  set(CPACK_NSIS_PACKAGE_NAME "MedYOLO11Qt")
  set(CPACK_NSIS_UNINSTALL_NAME "uninstall")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://yourcompany.com")
  set(CPACK_NSIS_CONTACT "support@yourcompany.com")
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