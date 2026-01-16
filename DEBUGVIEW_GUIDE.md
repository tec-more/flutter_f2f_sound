# DebugView 安装和使用指南

## 什么是 DebugView？

DebugView 是一个 Windows 系统调试工具，可以实时查看应用程序输出的调试信息。对于调试 Windows 平台插件非常有用。

## 下载和安装

### 方法1：从微软官网下载

1. **访问下载页面**：
   - 英文：https://docs.microsoft.com/en-us/sysinternals/downloads/debugview
   - 或者直接下载：https://download.sysinternals.com/files/DebugView.zip

2. **解压文件**：
   - 下载完成后，将 `DebugView.zip` 解压到任意目录
   - 例如：`C:\Tools\DebugView\`

3. **运行程序**：
   - 双击 `Dbgview.exe` 即可运行
   - 无需安装，直接使用

### 方法2：使用命令行下载（PowerShell）

```powershell
# 创建目录
New-Item -ItemType Directory -Path "C:\Tools\DebugView" -Force

# 下载
Invoke-WebRequest -Uri "https://download.sysinternals.com/files/DebugView.zip" -OutFile "C:\Tools\DebugView\DebugView.zip"

# 解压
Expand-Archive -Path "C:\Tools\DebugView\DebugView.zip" -DestinationPath "C:\Tools\DebugView" -Force

# 运行
Start-Process "C:\Tools\DebugView\Dbgview.exe"
```

## 配置 DebugView

### 1. 启用捕获

运行 DebugView 后，需要启用捕获功能：

1. **启用内核捕获**（可选）：
   - 菜单：`Capture` > `Capture Kernel`
   - 或按快捷键：`Ctrl+K`

2. **启用 Win32 捕获**（默认已启用）：
   - 菜单：`Capture` > `Capture Win32`
   - 或按快捷键：`Ctrl+W`

### 2. 设置过滤

为了只查看我们插件的调试信息，可以设置过滤：

1. **打开过滤器**：
   - 菜单：`Edit` > `Filter/Highlight`
   - 或按快捷键：`Ctrl+F`

2. **设置包含过滤器**：
   - 在 `Include` 框中输入：`*FlutterF2fSound*`
   - 点击 `Apply` 或 `OK`

3. **或者设置排除过滤器**：
   - 排除一些不需要的系统信息
   - 在 `Exclude` 框中输入不需要的内容

### 3. 设置选项

推荐的设置：

1. **菜单：`Options`**：
   - ✅ `Capture Kernel` - 捕获内核调试信息
   - ✅ `Capture Win32` - 捕获 Win32 调试信息
   - ❌ `Force Carriage Returns` - 不强制换行
   - ✅ `Clock Time` - 显示时间戳
   - ✅ `Milliseconds` - 显示毫秒

## 查看调试输出

### 运行应用程序

1. **启动 DebugView**（先启动）
2. **启用捕获**（确保 `Capture Win32` 已勾选）
3. **运行 Flutter 应用**：
   ```bash
   cd d:\Programs\flutter\ai\flutter_f2f_sound\flutter_f2f_sound\example
   flutter run -d windows
   ```

### 预期的调试输出

#### 系统音频初始化

```
[12345.678] Initializing system sound WASAPI...
[12345.689] Got default render endpoint successfully
[12345.701] Activated audio client successfully
[12345.723] Got mix format: 48000 Hz, 2 channels, 32 bits
[12345.745] Initialized audio client for loopback successfully
[12345.767] Buffer frame count: 48000
[12345.789] System sound WASAPI initialization complete
[12345.812] Starting system sound capture thread...
[12345.834] System sound audio client started successfully
```

#### 数据捕获日志

```
[12346.123] System sound: packet 100, size: 4096 bytes, flags: 0x00000002, silence: 1
[12346.145] System sound: skipping silent packet 100
[12347.234] System sound: packet 200, size: 4096 bytes, flags: 0x00000000, silence: 0
[12347.256] System sound: sent non-silent packet 200, size: 4096 bytes
[12348.345] ProcessSystemSoundData: Sending packet 50 to Flutter, size: 4096 bytes
```

#### 错误日志

```
[12345.678] Failed to get default render endpoint: 0x800700AA
[12345.689] Failed to activate audio client: 0x80070005
```

## 调试技巧

### 1. 清除日志

- 菜单：`Edit` > `Clear Display`
- 或按快捷键：`Ctrl+X`

### 2. 保存日志

- 菜单：`File` > `Save As...`
- 选择保存位置和格式（.txt 或 .log）

### 3. 搜索日志

- 菜单：`Edit` > `Find`
- 或按快捷键：`Ctrl+F`

### 4. 设置书签

- 右键点击日志行 > `Bookmark`
- 或按 `F9` 切换书签
- 跳转：`Edit` > `Go to Bookmark`

### 5. 自动滚动

- 菜单：`Options` > `Auto Scroll`
- 或按 `Ctrl+A` 切换

## 常见错误码

系统音频捕获可能出现的错误码：

| 错误码 | 名称 | 含义 | 解决方案 |
|-------|------|------|----------|
| 0x800700AA | ERROR_BAD_DEVICE | 音频设备不可用 | 检查音频设备是否启用 |
| 0x80070005 | E_ACCESSDENIED | 访问被拒绝 | 以管理员权限运行 |
| 0x88890003 | AUDCLNT_E_DEVICE_INVALID | 设备无效 | 重新连接音频设备 |
| 0x8889000F | AUDCLNT_E_UNSUPPORTED_FORMAT | 格式不支持 | 使用设备支持的格式 |
| 0x88890019 | AUDCLNT_E_BUFFER_ERROR | 缓冲区错误 | 重新初始化音频 |

## 高级用法

### 1. 远程调试

DebugView 支持远程查看调试信息：

1. **在远程机器上运行**：
   ```
   Dbgview.exe /a
   ```

2. **在本地连接**：
   - 菜单：`Computer` > `Connect`
   - 输入远程机器名称或IP

### 2. 命令行参数

```
Dbgview.exe [选项]

选项：
  /a        - 接受远程连接
  /c        - 清除日志
  /f <file> - 加载过滤器文件
  /l        - 启用日志记录
  /t        - 显示时钟时间
```

### 3. 高亮显示

设置特定文本的高亮显示：

1. 菜单：`Edit` > `Filter/Highlight`
2. 在 `Highlight` 标签页添加规则：
   - 文本：`ERROR`
   - 颜色：红色
   - 点击 `Add`

### 4. 日志统计

查看日志统计信息：

- 菜单：`View` > `Statistics`
- 显示：总行数、过滤后的行数等

## 故障排除

### 问题1：看不到任何调试输出

**原因**：捕获未启用或权限不足

**解决方案**：
1. 确保 `Capture Win32` 已勾选
2. 以管理员权限运行 DebugView
3. 重启 DebugView

### 问题2：输出太慢

**原因**：日志太多

**解决方案**：
1. 设置过滤器：`*FlutterF2fSound*`
2. 禁用内核捕获：取消 `Capture Kernel`
3. 增加缓冲区大小：`Options` > `Circular Buffer`

### 问题3：DebugView 崩溃

**原因**：日志量过大

**解决方案**：
1. 清除显示：`Ctrl+X`
2. 增加缓冲区大小
3. 设置过滤器减少输出

## 其他调试工具

除了 DebugView，还可以使用：

### 1. Visual Studio 输出窗口

如果使用 Visual Studio 编译：
- 菜单：`View` > `Output`
- 选择 `Debug` 输出

### 2. Windows Performance Recorder (WPR)

记录系统性能和事件：
```cmd
wpr -start GeneralProfile
# 运行应用程序
wpr -stop trace.etl
```

### 3. Process Monitor

监控文件、注册表、进程活动：
- 下载：https://docs.microsoft.com/en-us/sysinternals/downloads/procmon

### 4. TraceView

查看 ETW (Event Tracing for Windows) 事件：
- Windows SDK 工具

## 示例工作流程

完整的工作流程示例：

1. **启动 DebugView**（管理员权限）
2. **设置过滤器**：`*FlutterF2fSound*`
3. **启用捕获**：`Capture Win32`
4. **清除旧日志**：`Ctrl+X`
5. **运行应用**：
   ```bash
   flutter run -d windows
   ```
6. **执行操作**：点击"Start System Sound Capture"
7. **查看输出**：检查初始化日志
8. **保存日志**：`File` > `Save As...`
9. **分析问题**：根据日志定位问题

## 快捷键参考

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+X` | 清除显示 |
| `Ctrl+F` | 打开过滤器 |
| `Ctrl+A` | 自动滚动 |
| `Ctrl+K` | 捕获内核 |
| `Ctrl+W` | 捕获 Win32 |
| `Ctrl+E` | 启用/禁用所有捕获 |
| `F5` | 刷新 |
| `Ctrl+S` | 保存日志 |
| `Ctrl+P` | 打印 |

## 相关资源

- **Sysinternals 官网**：https://docs.microsoft.com/en-us/sysinternals/
- **DebugView 文档**：https://docs.microsoft.com/en-us/sysinternals/downloads/debugview
- **Windows 调试工具**：https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/
- **OutputDebugString API**：https://docs.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-outputdebugstringw

## 总结

DebugView 是一个强大的调试工具，特别适合调试 Windows 平台插件。通过查看详细的调试输出，可以快速定位问题：

✅ **实时查看调试信息**
✅ **过滤和搜索日志**
✅ **保存和分析日志**
✅ **远程调试支持**
✅ **轻量级，无需安装**

对于本项目的系统音频捕获调试，DebugView 可以显示：
- WASAPI 初始化步骤
- 音频格式信息
- 数据包统计
- 错误信息和错误码

这将大大提高调试效率！
