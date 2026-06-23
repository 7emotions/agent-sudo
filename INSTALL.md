# 安装指南

当人类用户将本文档链接发给你时，按以下步骤执行安装。

## 1. 克隆仓库

```bash
git clone https://github.com/7emotions/agent-sudo.git ~/agent-sudo
cd ~/agent-sudo
```

## 2. 获取二进制

选择以下任一方式：

### 方式一：下载预编译二进制（推荐，x86-64 Linux）

```bash
mkdir -p bin
wget -O bin/agent-sudo-flush https://github.com/7emotions/agent-sudo/releases/latest/download/agent-sudo-flush
chmod +x bin/agent-sudo-flush
```

### 方式二：从源码编译

需要 C++23 编译器（GCC 14+）、CMake 3.22+、Qt6、Eigen3。

```bash
sudo apt install -y g++ cmake qt6-base-dev qt6-multimedia-dev libqt6svg6-dev libeigen3-dev
cmake -S . -B build && cmake --build build
mkdir -p bin
cp build/agent-sudo-flush bin/
```

## 3. 安装到 PATH

```bash
sudo ln -sf ~/agent-sudo/bin/agent-sudo-flush /usr/local/bin/agent-sudo-flush
sudo ln -sf /usr/local/bin/agent-sudo-flush /usr/local/bin/agent-sudo
```

## 4. 验证

```bash
which agent-sudo agent-sudo-flush
echo $DISPLAY
```

`$DISPLAY` 为空表示无图形会话，agent-sudo-flush 会报错退出。此时可回退到 `pkexec`。

## 5. 测试

```bash
agent-sudo --reason "测试 agent-sudo 安装" -- echo "agent-sudo installed OK"
agent-sudo-flush
```

GUI 窗口出现、点击执行后 `agent-sudo installed OK` 正常输出 → 安装成功。

## 卸载

```bash
sudo rm /usr/local/bin/agent-sudo /usr/local/bin/agent-sudo-flush
rm -rf ~/agent-sudo
```
