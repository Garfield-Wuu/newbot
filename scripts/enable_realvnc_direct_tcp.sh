#!/bin/bash
# 为 RealVNC Service Mode 打开「直连 RFB」（本机监听 TCP，默认 5900），便于用 192.168.x.x:5900 连接。
# Service Mode 以 root 运行，除系统目录外还会读取 /root/.vnc/config.d/，两处都写入以免漏读。
# 若仅用 RealVNC 云账号在 Viewer 里连设备，通常不需要执行本脚本。
#
# 用法（板子上）:
#   sudo bash ~/newbot_ws/scripts/enable_realvnc_direct_tcp.sh
#
set -euo pipefail

CUSTOM_ETC="/etc/vnc/config.d/vncserver-x11.custom"
CUSTOM_ROOT="/root/.vnc/config.d/vncserver-x11.custom"

CONTENT='# RealVNC：允许局域网直连（RFB over TCP）
AllowIpListenRfb=TRUE
localhost=FALSE
RfbPort=5900
IpListenProtocols=TCP
'

if [[ "$(id -u)" -ne 0 ]]; then
  echo "请使用 root 运行: sudo bash $0" >&2
  exit 1
fi

write_custom() {
  local dest="$1"
  local dir
  dir=$(dirname "$dest")
  mkdir -p "$dir"
  if [[ -f "$dest" ]]; then
    cp -a "$dest" "${dest}.bak.$(date +%Y%m%d%H%M%S)"
    echo "已备份: $dest -> ${dest}.bak.*"
  fi
  printf '%s' "$CONTENT" > "$dest"
  echo "已写入 $dest"
}

write_custom "$CUSTOM_ETC"
mkdir -p /root/.vnc/config.d
write_custom "$CUSTOM_ROOT"

systemctl restart vncserver-x11-serviced.service
sleep 2

systemctl is-active --quiet vncserver-x11-serviced.service || {
  echo "警告: vncserver-x11-serviced 未处于 active，请: journalctl -u vncserver-x11-serviced -n 40" >&2
}

echo ""
if ss -tlnp | grep -qE ':5900\b'; then
  echo "检测到 TCP 5900 已在监听，可在 Viewer 使用 例如 192.168.x.x:5900"
else
  echo "仍未检测到 5900 监听。常见原因:"
  echo "  1) 当前 RealVNC 订阅（如 Connect Lite / 家用）策略禁止 IP 直连，只能走云；请用 Viewer 登录同一账号从「设备列表」连接，或升级支持直连的套餐。"
  echo "  2) 查看服务端日志: sudo tail -80 /root/.vnc/vncserver-x11.log"
fi

echo ""
echo "若尚未设置服务模式 VNC 密码: sudo vncpasswd -service"
echo "改密码或本脚本后若仍无 5900，请再执行一次: sudo systemctl restart vncserver-x11-serviced"
echo ""
echo "Windows VNC Viewer：连接项「属性 → Security」里若勾选 SSO/智能卡，直连密码常会失败；"
echo "  对局域网直连请取消勾选，仅用 VNC 密码。"
echo ""
