# 发布前检查清单

在发布 `flutter_f2f_sound` 到 pub.dev 之前，请确认以下所有项目：

## ✅ 代码质量

- [ ] 运行 `flutter analyze` 无错误
- [ ] 运行 `flutter test` 所有测试通过
- [ ] 运行 `dart format` 检查代码格式
- [ ] 确认没有编译警告

## ✅ 文档完整性

- [ ] README.md 完整且清晰
  - [ ] 包含功能描述
  - [ ] 包含平台支持矩阵
  - [ ] 包含安装指南
  - [ ] 包含使用示例
  - [ ] 包含API参考
  - [ ] 包含故障排除指南

- [ ] CHANGELOG.md 更新到最新版本
- [ ] LICENSE 文件存在
- [ ] API 文档完整（所有公共API都有注释）

## ✅ 版本管理

- [ ] pubspec.yaml 版本号正确 (当前: 1.0.0)
- [ ] 遵循语义化版本规范
- [ ] CHANGELOG.md 更新了版本内容
- [ ] 所有平台实现都已更新

## ✅ 平台支持

当前版本支持的平台：
- [x] Android (录音 + 播放 + 录音流)
- [x] iOS (录音 + 播放 + 录音流)
- [x] Windows (录音 + 播放 + 系统音频 + 录音流)
- [x] macOS (录音 + 播放 + 播放流 + 录音流)
- [x] Linux (录音 + 播放 + 系统音频 + 录音流)

## ✅ 测试验证

- [ ] 在 Android 设备/模拟器上测试
- [ ] 在 iOS 设备/模拟器上测试
- [ ] 在 Windows 上测试
- [ ] 在 macOS 上测试
- [ ] 在 Linux 上测试

测试功能：
- [ ] 本地音频播放
- [ ] 网络音频播放
- [ ] 音量控制
- [ ] 播放/暂停/停止
- [ ] 循环播放
- [ ] 录音功能（支持的平台）
- [ ] 系统音频捕获（Windows/Linux）
- [ ] 实时音频流

## ✅ 示例应用

- [ ] example/ 应用运行正常
- [ ] example/ 展示了所有主要功能
- [ ] example/ 代码质量良好

## ✅ 依赖管理

- [ ] 所有依赖项在 pubspec.yaml 中正确声明
- [ ] 依赖版本兼容性良好
- [ ] 没有未使用的依赖

## ✅ pub.dev 特定要求

- [ ] 包名唯一且描述性强
- [ ] 描述简洁明了（< 180字符）
- [ ] 包含相关 topics (audio, sound, media, recording, playback)
- [ ] homepage URL 有效
- [ ] repository URL 有效
- [ ] issue_tracker URL 有效
- [ ] documentation URL 有效

## ✅ 法律合规

- [ ] LICENSE 文件存在 (MIT License)
- [ ] 所有第三方库许可证正确
- [ ] 没有侵犯版权的代码

## ✅ 发布准备

- [ ] 运行 `dart pub publish --dry-run` 成功
- [ ] 准备好 Google 账户
- [ ] 了解发布流程

## 📝 发布后任务

发布完成后：
- [ ] 在 GitHub 创建 Release
- [ ] 标记发布的版本
- [ ] 通知用户更新
- [ ] 监控 pub.dev 统计
- [ ] 收集用户反馈

## 🚀 快速发布命令

### Windows:
```bash
publish.bat
```

### Linux/macOS:
```bash
chmod +x publish.sh
./publish.sh
```

### 手动发布:
```bash
# 1. 检查
flutter analyze
flutter test
dart format --output=none --set-exit-if-changed .

# 2. 干跑
dart pub publish --dry-run

# 3. 发布
dart pub publish
```

## 📞 获取帮助

- 查看 [PUBLISHING.md](PUBLISHING.md) 获取详细指南
- 访问 [pub.dev 文档](https://dart.dev/tools/pub/publishing)
- 查看 [Flutter 发布指南](https://flutter.dev/to/publish-plugin)

---

**重要提示**：
- 一旦发布到 pub.dev，版本号无法删除或重用
- 确保所有功能都经过测试
- 首次发布后，包名将永久保留
- 建议先在测试环境充分验证

祝你发布顺利！🎉
