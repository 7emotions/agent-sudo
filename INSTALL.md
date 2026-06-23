# 安装指南

当人类用户将本文档链接发给你时，按以下步骤执行安装。

## 方式一：下载预编译二进制（推荐，x86-64 Linux）

无需编译环境和依赖，直接下载 GitHub Release 附件。

### 1. 下载

```bash
wget https://github.com/7emotions/agent-sudo/releases/latest/download/agent-sudo-flush
```

### 2. 安装

```bash
sudo cp agent-sudo-flush /usr/local/bin/
sudo chmod +x /usr/local/bin/agent-sudo-flush
sudo ln -sf /usr/local/bin/agent-sudo-flush /usr/local/bin/agent-sudo
```

### 3. 验证

```bash
which agent-sudo agent-sudo-flush
echo $DISPLAY
```

## 方式二：从源码编译

需要 C++23 编译器和 Qt6 构建环境。

### 1. 克隆仓库

```bash
git clone https://github.com/7emotions/agent-sudo.git ~/agent-sudo
cd ~/agent-sudo
```

### 2. 安装编译依赖

```bash
sudo apt install -y g++ cmake qt6-base-dev qt6-multimedia-dev libqt6svg6-dev libeigen3-dev
```

### 3. 编译

```bash
cmake -S . -B build && cmake --build build
```

### 4. 安装

```bash
sudo cp build/agent-sudo-flush /usr/local/bin/
sudo ln -sf /usr/local/bin/agent-sudo-flush /usr/local/bin/agent-sudo
```

### 5. 验证

```bash
which agent-sudo agent-sudo-flush
echo $DISPLAY
```

`$DISPLAY` 为空表示无图形会话，agent-sudo-flush 会报错退出。此时可回退到 `pkexec`。

## 验证安装

```bash
agent-sudo --reason "测试 agent-sudo 安装" -- echo "agent-sudo installed OK"
agent-sudo-flush
```

GUI 窗口出现、点击执行后 `agent-sudo installed OK` 正常输出 → 安装成功。

## 卸载

```bash
sudo rm /usr/local/bin/agent-sudo /usr/local/bin/agent-sudo-flush
```
