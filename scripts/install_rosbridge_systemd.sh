#!/usr/bin/env bash
# 将 rosbridge WebSocket 注册为开机自启（需 sudo）
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
chmod +x "${SCRIPT_DIR}/run_rosbridge_websocket.sh"
sudo install -m 644 "${SCRIPT_DIR}/rosbridge-websocket.service" /etc/systemd/system/rosbridge-websocket.service
sudo systemctl daemon-reload
sudo systemctl enable rosbridge-websocket.service
sudo systemctl restart rosbridge-websocket.service
echo "已启用并尝试启动。查看状态: sudo systemctl status rosbridge-websocket.service"
echo "日志: journalctl -u rosbridge-websocket.service -f"
echo "关闭自启: sudo systemctl disable --now rosbridge-websocket.service"
