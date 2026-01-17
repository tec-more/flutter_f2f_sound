#!/bin/bash

# flutter_f2f_sound 发布到 pub.dev 的辅助脚本
# 使用方法: ./publish.sh

set -e

echo "🚀 准备发布 flutter_f2f_sound 到 pub.dev..."
echo ""

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 检查当前目录
if [ ! -f "pubspec.yaml" ]; then
    echo -e "${RED}❌ 错误: 请在 flutter_f2f_sound 根目录运行此脚本${NC}"
    exit 1
fi

echo -e "${YELLOW}📋 步骤 1/6: 运行代码分析...${NC}"
flutter analyze
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ 代码分析失败，请修复错误后再试${NC}"
    exit 1
fi
echo -e "${GREEN}✅ 代码分析通过${NC}"
echo ""

echo -e "${YELLOW}📋 步骤 2/6: 运行测试...${NC}"
flutter test
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ 测试失败，请修复错误后再试${NC}"
    exit 1
fi
echo -e "${GREEN}✅ 测试通过${NC}"
echo ""

echo -e "${YELLOW}📋 步骤 3/6: 检查代码格式...${NC}"
dart format --output=none --set-exit-if-changed .
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}⚠️  代码格式不标准，正在自动格式化...${NC}"
    dart format .
    echo -e "${YELLOW}请检查格式化后的代码并重新运行${NC}"
    exit 1
fi
echo -e "${GREEN}✅ 代码格式检查通过${NC}"
echo ""

echo -e "${YELLOW}📋 步骤 4/6: 干跑发布（检查是否有问题）...${NC}"
dart pub publish --dry-run
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ 干跑失败，请修复错误后再试${NC}"
    exit 1
fi
echo -e "${GREEN}✅ 干跑成功${NC}"
echo ""

echo -e "${YELLOW}📋 步骤 5/6: 检查必要文件...${NC}"
required_files=("README.md" "CHANGELOG.md" "LICENSE")
for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}❌ 缺少必要文件: $file${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✅ 所有必要文件都存在${NC}"
echo ""

echo -e "${YELLOW}📋 步骤 6/6: 实际发布到 pub.dev...${NC}"
echo -e "${YELLOW}⚠️  即将发布到 pub.dev，确保你已经：${NC}"
echo "   1. 更新了版本号 (当前: $(grep 'version: ' pubspec.yaml | cut -d' ' -f2))"
echo "   2. 更新了 CHANGELOG.md"
echo "   3. 检查了所有更改"
echo ""
read -p "确认发布? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo -e "${YELLOW}❌ 发布已取消${NC}"
    exit 0
fi

echo ""
echo -e "${GREEN}🚀 开始发布...${NC}"
dart pub publish

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 发布成功！${NC}"
    echo "请访问 https://pub.dev/packages/flutter_f2f_sound 查看你的包"
else
    echo ""
    echo -e "${RED}❌ 发布失败${NC}"
    exit 1
fi
