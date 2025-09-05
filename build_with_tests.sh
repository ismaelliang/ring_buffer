#!/bin/bash

# 构建脚本，包含Google Test 的测试用例
# 依赖项通过 vcpkg manifest 模式 管理

set -e  # 如果任何命令失败，则退出

echo "=== Building RingBuffer Project with vcpkg and Google Test ==="

# 检查 VCPKG_ROOT 是否设置
if [ -z "$VCPKG_ROOT" ]; then
    echo "Warning: VCPKG_ROOT environment variable is not set."
    echo "Please set VCPKG_ROOT to your vcpkg installation directory."
    echo "Example: export VCPKG_ROOT=/path/to/vcpkg"
    exit 1
fi

echo "Using vcpkg from: $VCPKG_ROOT"

# 通过 vcpkg 安装依赖
echo "Installing dependencies via vcpkg..."
$VCPKG_ROOT/vcpkg install

# 创建构建目录
echo "Creating build directory..."
mkdir -p build
cd build

# 配置项目 CMake
echo "Configuring project with cmake..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
         -DCMAKE_BUILD_TYPE=Release

# 构建项目
echo "Building project..."
cmake --build . --config Release

# 跑测试
echo "Running tests..."
ctest --output-on-failure --build-config Release

echo ""
echo "=== Build and Test Complete ==="
echo "Executables are available in:"
echo "  - bin/producer"
echo "  - bin/consumer" 
echo "  - bin/check_header"
echo "  - test/ringbuffer_tests"
echo "  - test/no_create_tests"
