#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Newbot 状态 TUI：订阅 /robot_state、/battery、/scan、/odom、/cmd_vel 等并刷新终端界面。
依赖：ROS Noetic、Python3、curses（标准库）。无需 pip 安装第三方库。

用法：
  source /opt/ros/noetic/setup.bash && source ~/newbot_ws/devel/setup.bash
  python3 ~/newbot_ws/scripts/newbot_status_tui.py

按键：q 退出；r 强制刷新标题时间。
"""

from __future__ import print_function

import curses
import math
import os
import threading
import time
from collections import deque

import rospy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import BatteryState, LaserScan
from std_msgs.msg import Int16MultiArray
from tf.transformations import euler_from_quaternion

# BatteryState.power_supply_status 常用值（与 base_control 中枚举一致）
_SUPPLY_STATUS = {
    0: "UNKNOWN",
    1: "CHARGING",
    2: "DISCHARGING",
    3: "NOT_CHARGING",
    4: "FULL",
}


class SharedState(object):
    def __init__(self):
        self.lock = threading.Lock()
        self.robot_state = None  # list
        self.robot_state_stamp = 0.0
        self.battery = None  # BatteryState copy fields
        self.battery_stamp = 0.0
        self.scan_hz = 0.0
        self.scan_frame = ""
        self.scan_ranges_valid = 0
        self.scan_ranges_total = 0
        self.scan_stamp = 0.0
        self.scan_times = deque(maxlen=80)
        self.odom_x = 0.0
        self.odom_y = 0.0
        self.odom_yaw_deg = 0.0
        self.odom_vx = 0.0
        self.odom_wz = 0.0
        self.odom_stamp = 0.0
        self.cmd_v = 0.0
        self.cmd_w = 0.0
        self.cmd_stamp = 0.0
        self.errors = []

    def age(self, t):
        if t <= 0:
            return None
        return time.time() - t


def _battery_snapshot(msg):
    return {
        "voltage": msg.voltage,
        "percentage": msg.percentage,
        "status": msg.power_supply_status,
        "present": msg.present,
    }


def setup_subscribers(st):
    def rs_cb(msg):
        with st.lock:
            st.robot_state = list(msg.data)
            st.robot_state_stamp = time.time()

    def bat_cb(msg):
        with st.lock:
            st.battery = _battery_snapshot(msg)
            st.battery_stamp = time.time()

    def scan_cb(msg):
        now = time.time()
        with st.lock:
            st.scan_times.append(now)
            if len(st.scan_times) >= 2:
                dt = st.scan_times[-1] - st.scan_times[0]
                st.scan_hz = (len(st.scan_times) - 1) / dt if dt > 1e-6 else 0.0
            st.scan_frame = msg.header.frame_id
            arr = msg.ranges
            st.scan_ranges_total = len(arr)
            st.scan_ranges_valid = sum(1 for r in arr if not (math.isnan(r) or r <= msg.range_min or r > msg.range_max))
            st.scan_stamp = now

    def odom_cb(msg):
        with st.lock:
            q = msg.pose.pose.orientation
            roll, pitch, yaw = euler_from_quaternion([q.x, q.y, q.z, q.w])
            st.odom_x = msg.pose.pose.position.x
            st.odom_y = msg.pose.pose.position.y
            st.odom_yaw_deg = math.degrees(yaw)
            st.odom_vx = msg.twist.twist.linear.x
            st.odom_wz = msg.twist.twist.angular.z
            st.odom_stamp = time.time()

    def cmd_cb(msg):
        with st.lock:
            st.cmd_v = msg.linear.x
            st.cmd_w = msg.angular.z
            st.cmd_stamp = time.time()

    rospy.Subscriber("/robot_state", Int16MultiArray, rs_cb, queue_size=1)
    rospy.Subscriber("/battery", BatteryState, bat_cb, queue_size=1)
    rospy.Subscriber("/scan", LaserScan, scan_cb, queue_size=2)
    rospy.Subscriber("/odom", Odometry, odom_cb, queue_size=1)
    rospy.Subscriber("/cmd_vel", Twist, cmd_cb, queue_size=1)


def _fmt_age(age):
    if age is None:
        return "--"
    if age < 1.0:
        return "{:.0f}ms".format(age * 1000)
    return "{:.1f}s".format(age)


def draw(stdscr, st):
    stdscr.nodelay(True)
    stdscr.timeout(200)
    curses.curs_set(0)

    try:
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_GREEN, -1)
        curses.init_pair(2, curses.COLOR_YELLOW, -1)
        curses.init_pair(3, curses.COLOR_RED, -1)
        curses.init_pair(4, curses.COLOR_CYAN, -1)
    except Exception:
        pass

    def pair_for_age(age):
        if age is None:
            return 0
        if age < 2.0:
            return 1
        if age < 5.0:
            return 2
        return 3

    row = 0
    while True:
        ch = stdscr.getch()
        if ch == ord("q") or ch == ord("Q"):
            break
        h, w = stdscr.getmaxyx()
        stdscr.erase()
        row = 0

        def line(s, attr=0):
            nonlocal row
            if row >= h - 1:
                return
            stdscr.addnstr(row, 0, s[: max(0, w - 1)], w - 1, attr)
            row += 1

        with st.lock:
            rs = st.robot_state
            rs_age = st.age(st.robot_state_stamp)
            bat = st.battery
            bat_age = st.age(st.battery_stamp)
            shz = st.scan_hz
            sfa = st.age(st.scan_stamp)
            ox, oy, oyaw = st.odom_x, st.odom_y, st.odom_yaw_deg
            ovx, owz = st.odom_vx, st.odom_wz
            o_age = st.age(st.odom_stamp)
            cv, cw = st.cmd_v, st.cmd_w
            c_age = st.age(st.cmd_stamp)
            sframe = st.scan_frame
            sv, stot = st.scan_ranges_valid, st.scan_ranges_total

        line(" Newbot 状态监视 TUI   [q]退出  [r]仅刷新时钟", curses.A_BOLD | curses.color_pair(4))
        line(" ROS_MASTER_URI: " + os.environ.get("ROS_MASTER_URI", "(unset)"), 0)
        line(" 本地时间: " + time.strftime("%Y-%m-%d %H:%M:%S"), 0)
        line("-" * min(70, w - 1), 0)

        line(" [MCU /robot_state]  最近更新: " + _fmt_age(rs_age), curses.color_pair(pair_for_age(rs_age)))
        if rs is None or len(rs) < 8:
            line("   (尚无数据 — 请确认 base_control 已运行)", curses.color_pair(3))
        else:
            line(
                "   编码器 L/R: {:6d} / {:6d}    电压: {:.2f} V ({:d} mV)".format(
                    int(rs[0]), int(rs[1]), rs[2] / 1000.0, int(rs[2])
                )
            )
            line(
                "   充电器/充满: {} / {}    雷达使能 enable_power: {}".format(
                    int(rs[3]), int(rs[4]), int(rs[7])
                )
            )
            line("   PWM 左/右: {:6d} / {:6d}".format(int(rs[5]), int(rs[6])))

        line("-" * min(70, w - 1), 0)
        line(" [电池 /battery]  最近更新: " + _fmt_age(bat_age), curses.color_pair(pair_for_age(bat_age)))
        if bat is None:
            line("   (尚无数据)", curses.color_pair(3))
        else:
            pct = bat["percentage"]
            if isinstance(pct, float) and math.isnan(pct):
                pct_s = "--"
            else:
                pct_s = "{:.0f}%".format(float(pct) * 100.0)
            stname = _SUPPLY_STATUS.get(bat["status"], str(bat["status"]))
            line(
                "   电压: {:.3f} V   估算电量: {}   状态: {}   present: {}".format(
                    bat["voltage"], pct_s, stname, bat["present"]
                )
            )

        line("-" * min(70, w - 1), 0)
        line(" [雷达 /scan]  最近更新: " + _fmt_age(sfa), curses.color_pair(pair_for_age(sfa)))
        line("   估算频率: {:.2f} Hz   frame: {}   有效距离点: {}/{}".format(shz, sframe, sv, stot))
        if sfa is None or sfa > 3.0:
            line("   提示: 无点云时可试: enable_lidar true + rosservice call /start_scan", curses.color_pair(2))

        line("-" * min(70, w - 1), 0)
        line(" [里程计 /odom]  最近更新: " + _fmt_age(o_age), curses.color_pair(pair_for_age(o_age)))
        line(
            "   位姿 x,y,yaw: {:.4f} m, {:.4f} m, {:.2f} deg".format(ox, oy, oyaw)
        )
        line("   速度 vx,wz: {:.4f} m/s, {:.4f} rad/s".format(ovx, owz))

        line("-" * min(70, w - 1), 0)
        line(" [速度指令 /cmd_vel]  最近更新: " + _fmt_age(c_age), curses.color_pair(pair_for_age(c_age)))
        line("   linear.x: {:.4f} m/s   angular.z: {:.4f} rad/s".format(cv, cw))

        line("-" * min(70, w - 1), 0)
        line(" 刷新周期 ~200ms  |  颜色: 绿=新 黄=稍旧 红=过期或无数据", 0)

        stdscr.refresh()


def main():
    rospy.init_node("newbot_status_tui", anonymous=True, disable_signals=True)
    st = SharedState()
    setup_subscribers(st)
    spin_thread = threading.Thread(target=rospy.spin)
    spin_thread.daemon = True
    spin_thread.start()
    time.sleep(0.3)
    try:
        curses.wrapper(lambda scr: draw(scr, st))
    finally:
        rospy.signal_shutdown("tui exit")


if __name__ == "__main__":
    main()
