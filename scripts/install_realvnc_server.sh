#!/bin/bash
# RealVNC Server（官方 Linux ARM64 deb）安装与启用「当前图形桌面」共享
# 适用于 Ubuntu 20.04 aarch64 + LightDM/XFCE（香橙派官方桌面镜像类环境）
#
# 官方包说明: https://www.realvnc.com/en/connect/download/vnc/linux/
# 与 tightvncserver 冲突，安装前会卸载 tightvncserver
#
# 用法（在板子上）:
#   sudo bash ~/newbot_ws/scripts/install_realvnc_server.sh
#
set -euo pipefail

DEB_URL="https://downloads.realvnc.com/download/file/vnc.files/VNC-Server-7.13.1-Linux-ARM64.deb"
DEB_PATH="/tmp/VNC-Server-Linux-ARM64.deb"

if [[ "$(id -u)" -ne 0 ]]; then
  echo "请使用 root 运行: sudo bash $0" >&2
  exit 1
fi

echo "[1/5] 卸载与 RealVNC 冲突的 tightvncserver（若已安装）..."
apt-get remove -y tightvncserver 2>/dev/null || true

echo "[2/5] 下载 RealVNC Server deb（若尚未下载）..."
if [[ ! -f "$DEB_PATH" ]] || [[ ! -s "$DEB_PATH" ]]; then
  wget -O "$DEB_PATH" "$DEB_URL"
fi

echo "[3/5] 安装 deb 并修复依赖..."
dpkg -i "$DEB_PATH" || true
apt-get install -f -y

echo "[4/5] 启用并启动 X11 服务模式（共享物理显示器 :0）..."
# 官方文档: systemd 用 vncserver-x11-serviced.service（postinst 会跑 vncinitconfig --install-defaults 生成单元）
systemctl daemon-reload 2>/dev/null || true
if systemctl cat vncserver-x11-serviced.service &>/dev/null; then
  systemctl enable --now vncserver-x11-serviced.service
elif [[ -x /usr/lib/vnc/vncservice ]]; then
  # vncservice 仅有 start/stop，无 install 子命令
  /usr/lib/vnc/vncservice start vncserver-x11-serviced || true
else
  echo "警告: 未找到 systemd 单元或 /usr/lib/vnc/vncservice，请检查 realvnc-vnc-server 是否安装成功。" >&2
fi

echo "[5/5] 完成。"
echo ""
echo "后续请在本机完成（任选其一）："
echo "  1) 图形界面运行「VNC Server」/ vnclicensewiz，登录 RealVNC 账号或配置直连密码；"
echo "  2) 终端: sudo vnclicensewiz"
echo ""
echo "电脑端安装 VNC Viewer: https://www.realvnc.com/en/connect/download/viewer/"
echo "直连时默认端口常为 5900；若与 xrdp 并存，注意防火墙与端口。"
echo "查看监听: ss -tlnp | grep -E '5900|5901'"
