# 安装指南

当人类用户将本文档链接发给你时，按以下步骤执行安装。

## 步骤

### 1. 克隆仓库

```bash
git clone https://github.com/7emotions/pkagt.git ~/pkagt
```

### 2. 安装依赖

```bash
pip install PySide2
```

如果 pip 源没有 PySide2，用 apt：

```bash
sudo apt install -y python3-pyside2.qtwidgets
```

### 3. 创建符号链接

```bash
sudo ln -sf ~/pkagt/agent-sudo/main.py /usr/local/bin/agent-sudo
sudo ln -sf ~/pkagt/agent-sudo/main.py /usr/local/bin/agent-sudo-flush
```

### 4. 验证

```bash
# 检查二进制是否可用
which agent-sudo agent-sudo-flush

# 检查版本
agent-sudo --version

# 检查 DISPLAY（GUI 模式需要）
echo $DISPLAY
```

`$DISPLAY` 为空表示无图形会话，agent-sudo-flush 会报错退出。此时可回退到 `pkexec`。

### 5. 测试

```bash
# 队列一条无害命令
agent-sudo --reason "测试 agent-sudo 安装" -- echo "pkagt installed OK"

# 弹出 GUI 审批窗口
agent-sudo-flush
```

GUI 窗口出现、点击执行后 `echo "pkagt installed OK"` 正常输出 → 安装成功。

## 卸载

```bash
sudo rm /usr/local/bin/agent-sudo /usr/local/bin/agent-sudo-flush
rm -rf ~/pkagt
```
