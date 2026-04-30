# ESP8266 LED 远程控制

通过 MQTT + WebSocket 实现的 ESP8266 LED 远程控制项目，支持局域网外访问。

**网页控制端：** https://xijianwei.github.io/esp8266-led-control/

---

## 硬件

| 参数      | 说明                                  |
| ------- | ----------------------------------- |
| 板子      | Ai-Thinker NodeMCU-8266 v1.2（安信可科技） |
| WiFi 模块 | ESP-12S                             |
| 控制引脚    | GPIO2（板载 LED，HIGH=亮，LOW=灭）          |
| 按键      | 仅 RST 键                             |

---

## 架构

```
[index.html 网页]
    ↕ mqtt.js over WebSocket
[HiveMQ 公共 Broker]  ← 免费，无需账号
    ↕ TCP 1883
[ESP8266 固件]
```

### MQTT 主题

| 主题                                   | 方向      | 内容           |
| ------------------------------------ | ------- | ------------ |
| `esp8266/led/nodemcu_a1b2c3/control` | 网页 → 设备 | `ON` / `OFF` |
| `esp8266/led/nodemcu_a1b2c3/state`   | 设备 → 网页 | `ON` / `OFF` |
| `esp8266/led/nodemcu_a1b2c3/cmd`     | 网页 → 设备 | `RESET_WIFI` |

---

## 快速上手

### 1. 安装 Arduino IDE

下载地址：https://www.arduino.cc/en/software（建议 2.x 版本）

### 2. 添加 ESP8266 开发板支持

Arduino IDE → 首选项 → 附加开发板管理器网址，填入：

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

工具 → 开发板 → 开发板管理器 → 搜索 `esp8266` → 安装 `ESP8266 by ESP8266 Community`

### 3. 安装依赖库

工具 → 管理库，分别搜索安装：

| 库名             | 作者           |
| -------------- | ------------ |
| `PubSubClient` | Nick O'Leary |
| `WiFiManager`  | tzapu        |

### 4. 开发板配置

工具菜单选择：

| 选项            | 值                                              |
| ------------- | ---------------------------------------------- |
| 开发板           | NodeMCU 1.0 (ESP-12E Module)                   |
| Upload Speed  | 115200                                         |
| CPU Frequency | 80 MHz                                         |
| Flash Size    | 4MB (FS:2MB OTA:~1019KB)                       |
| Port          | 对应的 USB 串口（macOS 一般是 `/dev/cu.usbserial-xxxx`） |

### 5. 烧录固件

1. USB 连接板子与电脑
2. 打开 `esp8266_LED_control.ino`
3. 点击上传按钮
4. 打开串口监视器（波特率 115200）查看启动日志

---

## 文件说明

- `esp8266_LED_control.ino` — ESP8266 固件
- `index.html` — 网页控制端

---

## 固件说明

**依赖库（Arduino IDE 库管理器安装）：**

- `PubSubClient` by Nick O'Leary
- `WiFiManager` by tzapu

**首次配网：**

1. 烧录固件后，设备开启热点 `ESP8266-Setup`，密码 `12345678`
2. 手机连接该热点，浏览器访问 `192.168.4.1`
3. 点击 `Configure WiFi`，选择 WiFi 并输入密码保存
4. 设备重启后自动连接 WiFi 并接入 MQTT

**重置 WiFi：**

- 打开网页控制端 → 点击底部 `// 如何配置 Wi-Fi //` → 点击 `重置设备 Wi-Fi`（设备需在线）

---

## 网页控制端

- **UI 风格**：Cyberpunk / Neon 暗色主题
- **功能**：LED 实时状态、一键开关、MQTT 连接状态、系统日志、WiFi 配网说明与重置
- **访问方式**：
  - 公网：https://xijianwei.github.io/esp8266-led-control/
  - 局域网：`http://[Mac的IP]:8080/index.html`（需运行 `python3 -m http.server 8080`）
