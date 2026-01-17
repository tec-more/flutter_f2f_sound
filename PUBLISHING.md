# 发布到 pub.dev 指南

本文档说明如何将 `flutter_f2f_sound` 插件发布到 pub.dev。

## 前置准备

### 1. 安装发布工具

```bash
# 激活 pub 发布工具
dart pub global activate pub_dev_client

# 或使用 flutter pub
flutter pub global activate pub_dev_client
```

### 2. 准备 Google 账户

- 确保你有 Google 账户
- 访问 https://pub.dev 并登录
- 完成账户验证

## 发布前检查清单

### 1. 代码质量检查

```bash
# 在项目根目录运行
cd flutter_f2f_sound

# 运行代码分析
flutter analyze

# 运行测试
flutter test

# 检查格式化
dart format --output=none --set-exit-if-changed .
```

### 2. 更新版本号

编辑 `pubspec.yaml`:

```yaml
name: flutter_f2f_sound
description: A powerful cross-platform audio plugin for Flutter...
version: 1.0.0  # 确保版本号正确
```

### 3. 更新 CHANGELOG.md

创建或更新 `CHANGELOG.md`:

```markdown
## [1.0.0] - 2025-01-17

### Added
- Initial release
- Audio playback support (Android, iOS, macOS, Windows, Linux)
- Audio recording with real-time streaming (Android, iOS, macOS, Windows, Linux)
- System audio capture (Windows, Linux)
- Network audio streaming with non-blocking async
- Volume and playback controls
- Loop playback support
- Automatic sample rate conversion
```

### 4. 确保 README.md 完整

当前 README.md 已包含：
- ✅ 功能说明
- ✅ 平台支持矩阵
- ✅ 安装指南
- ✅ 使用示例
- ✅ API 文档
- ✅ 技术细节
- ✅ 故障排除

### 5. 检查文件结构

确保项目结构正确：

```
flutter_f2f_sound/
├── lib/
│   ├── flutter_f2f_sound.dart
│   ├── flutter_f2f_sound_method_channel.dart
│   ├── flutter_f2f_sound_platform_interface.dart
│   └── flutter_f2f_sound_web.dart
├── android/
│   └── src/main/kotlin/com/tecmore/flutter_f2f_sound/
├── ios/
│   └── Classes/
├── windows/
├── linux/
├── macos/
├── web/
├── example/
├── test/
├── README.md
├── CHANGELOG.md
├── LICENSE
└── pubspec.yaml
```

## 发布步骤

### 1. 干跑（Dry Run）

首先进行一次模拟发布，检查是否有问题：

```bash
cd flutter_f2f_sound

# 干跑发布
dart pub publish --dry-run
```

修复所有报告的错误和警告。

### 2. 实际发布

```bash
# 发布到 pub.dev
dart pub publish
```

系统会提示你访问一个 URL 进行身份验证。

### 3. 完成身份验证

- 复制终端中显示的 URL
- 在浏览器中打开该 URL
- 选择你的 Google 账户并授权
- 返回终端继续

### 4. 等待审核

- 发布后，包会进入审核队列
- 通常需要几分钟到几小时
- 审核通过后，包将出现在 pub.dev 上

## 发布后验证

### 1. 检查包页面

访问 https://pub.dev/packages/flutter_f2f_sound

确认：
- ✅ 包信息正确显示
- ✅ README.md 正常渲染
- ✅ 版本号正确
- ✅ 平台支持正确

### 2. 测试安装

```bash
# 创建测试项目
flutter create test_flutter_f2f_sound
cd test_flutter_f2f_sound

# 添加依赖
flutter pub add flutter_f2f_sound

# 运行示例
flutter run
```

## 版本管理

### 语义化版本

遵循语义化版本规范：`MAJOR.MINOR.PATCH`

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的问题修复

示例：
- `1.0.0` - 初始发布
- `1.0.1` - Bug 修复
- `1.1.0` - 新增功能
- `2.0.0` - 重大变更

### 发布新版本

1. 更新 `pubspec.yaml` 中的版本号
2. 更新 `CHANGELOG.md`
3. 提交更改：
   ```bash
   git add pubspec.yaml CHANGELOG.md
   git commit -m "Bump version to 1.0.1"
   git push
   ```
4. 运行 `dart pub publish`

## 常见问题

### Q: 发布失败提示 "403 Forbidden"

**A**: 确保你已经正确设置了 Google 账户和 pub.dev 访问权限。

### Q: 包名已被占用

**A**: `flutter_f2f_sound` 可能已被使用。你需要：
1. 更改包名（如 `flutter_f2f_sound_pro`）
2. 或联系原包的所有者

### Q: 如何撤销已发布的版本？

**A**: 已发布的版本无法删除，但可以：
- 发布新版本修复问题
- 联系 pub.dev 支持（严重问题）
- 标记包为 discontinued（废弃）

### Q: 如何添加其他开发者？

**A**: 在 `pubspec.yaml` 中添加：

```yaml
publish_to: none
```

然后在 GitHub 仓库设置中管理协作者。

## 维护建议

### 定期更新

- 定期检查依赖版本
- 修复用户报告的问题
- 添加新功能

### 监控反馈

- 查看 GitHub Issues
- 监控 pub.dev 评分
- 收集用户反馈

### 文档维护

- 保持 README.md 更新
- 更新 CHANGELOG.md
- 添加使用示例

## 相关资源

- [Flutter 包发布指南](https://flutter.dev/to/publish-plugin)
- [pub.dev 官方文档](https://dart.dev/tools/pub/publishing)
- [语义化版本](https://semver.org/)
- [包发布最佳实践](https://github.com/flutter/flutter/wiki/Publishing-to-pub.dev)

## 支持与反馈

如果遇到问题：
1. 查看 [GitHub Issues](https://github.com/tecmore/flutter_f2f_sound/issues)
2. 阅读 [pub.dev 帮助文档](https://pub.dev/help)
3. 在 Flutter 社区寻求帮助

---

祝你发布顺利！🎉
