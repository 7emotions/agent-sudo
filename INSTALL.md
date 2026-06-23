# 安装指南

当人类用户将本文档链接发给你时，按以下步骤执行安装。

## 步骤

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
# 检查二进制是否可用
which agent-sudo agent-sudo-flush

# 检查 DISPLAY（GUI 模式需要图形会话）
echo $DISPLAY
```

`$DISPLAY` 为空表示无图形会话，agent-sudo-flush 会报错退出。此时可回退到 `pkexec`。

### 6. 测试

```bash
# 队列一条无害命令
agent-sudo --reason "测试 agent-sudo 安装" -- echo "agent-sudo installed OK"

# 弹出 GUI 审批窗口
agent-sudo-flush
```

GUI 窗口出现、点击执行后 `agent-sudo installed OK` 正常输出 → 安装成功。

## 卸载

```bash
sudo rm /usr/local/bin/agent-sudo /usr/local/bin/agent-sudo-flush
rm -rf ~/agent-sudo
```
