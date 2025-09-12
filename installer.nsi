; MedYOLO11Qt 安装脚本
; NSIS (Nullsoft Scriptable Install System) 安装程序

; 基本设置
Name "MedYOLO11Qt"
OutFile "MedYOLO11Qt_Setup.exe"
InstallDir "$PROGRAMFILES\MedYOLO11Qt"

; 请求管理员权限
RequestExecutionLevel admin

; 现代UI界面
!include "MUI2.nsh"

; 界面设置
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; 欢迎页面
!insertmacro MUI_PAGE_WELCOME
; 许可协议页面
!insertmacro MUI_PAGE_LICENSE "${NSISDIR}\Docs\Modern UI\License.txt"
; 目录选择页面
!insertmacro MUI_PAGE_DIRECTORY
; 安装页面
!insertmacro MUI_PAGE_INSTFILES
; 完成页面
!insertmacro MUI_PAGE_FINISH

; 卸载页面
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; 语言设置
!insertmacro MUI_LANGUAGE "SimpChinese"

; 安装部分
Section "主程序" SecMain
  SetOutPath "$INSTDIR"
  
  ; 复制主程序
  File "build\Release\medapp.exe"
  
  ; 复制Qt DLLs
  File "build\Release\Qt6Core.dll"
  File "build\Release\Qt6Gui.dll"
  File "build\Release\Qt6Widgets.dll"
  
  ; 复制ONNXRuntime DLL
  File "build\Release\onnxruntime.dll"
  
  ; 复制GDCM DLLs
  File "build\Release\gdcm*.dll"
  
  ; 创建platforms目录并复制插件
  SetOutPath "$INSTDIR\platforms"
  File "build\Release\platforms\qwindows.dll"
  
  ; 创建models目录并复制模型文件
  SetOutPath "$INSTDIR\models"
  File "models\fai_xray.onnx"
  
  ; 创建开始菜单快捷方式
  CreateDirectory "$SMPROGRAMS\MedYOLO11Qt"
  CreateShortcut "$SMPROGRAMS\MedYOLO11Qt\MedYOLO11Qt.lnk" "$INSTDIR\medapp.exe"
  CreateShortcut "$SMPROGRAMS\MedYOLO11Qt\卸载.lnk" "$INSTDIR\uninstall.exe"
  
  ; 创建桌面快捷方式
  CreateShortcut "$DESKTOP\MedYOLO11Qt.lnk" "$INSTDIR\medapp.exe"
  
  ; 写入卸载程序
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; 写入注册表信息
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                 "DisplayName" "MedYOLO11Qt"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                 "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                 "DisplayIcon" "$INSTDIR\medapp.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                 "Publisher" "Your Company Name"
  WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                   "NoModify" 1
  WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" \
                   "NoRepair" 1
SectionEnd

; 可选组件 - 示例文件
Section "示例文件" SecExamples
  SetOutPath "$INSTDIR\examples"
  File /r "test\*"
SectionEnd

; 卸载部分
Section "Uninstall"
  ; 删除程序文件
  Delete "$INSTDIR\medapp.exe"
  Delete "$INSTDIR\*.dll"
  
  ; 删除platforms目录
  RMDir /r "$INSTDIR\platforms"
  
  ; 删除models目录
  RMDir /r "$INSTDIR\models"
  
  ; 删除示例文件
  RMDir /r "$INSTDIR\examples"
  
  ; 删除卸载程序
  Delete "$INSTDIR\uninstall.exe"
  
  ; 删除安装目录
  RMDir "$INSTDIR"
  
  ; 删除开始菜单快捷方式
  Delete "$SMPROGRAMS\MedYOLO11Qt\MedYOLO11Qt.lnk"
  Delete "$SMPROGRAMS\MedYOLO11Qt\卸载.lnk"
  RMDir "$SMPROGRAMS\MedYOLO11Qt"
  
  ; 删除桌面快捷方式
  Delete "$DESKTOP\MedYOLO11Qt.lnk"
  
  ; 删除注册表信息
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt"
SectionEnd

; 函数 - 安装完成后显示消息
Function .onInstSuccess
  MessageBox MB_OK "MedYOLO11Qt 安装完成！点击确定启动程序。"
  Exec "$INSTDIR\medapp.exe"
FunctionEnd

; 函数 - 初始化安装程序
Function .onInit
  ; 检查是否已安装，如果已安装则提示卸载
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MedYOLO11Qt" "UninstallString"
  StrCmp $R0 "" done
  
  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "MedYOLO11Qt 已经安装。$" \
  "按确定卸载旧版本，或取消退出安装程序。" \
  IDOK uninst
  Abort
  
  uninst:
    ClearErrors
    ExecWait '$R0 _?=$INSTDIR' ; 不复制卸载程序
    
    IfErrors no_remove_uninstaller
    ; 你可以在这里使用 Delete 命令删除 $INSTDIR，但它可能已经被卸载程序删除了
    no_remove_uninstaller:
  
  done:
FunctionEnd