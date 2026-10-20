# 功率计算器总 · 硬件工程与调试记录

嘉立创EDA专业版工程 **「功率计算器总」** 的版本留档 + 一次「J-Link 下载失败」的完整排查记录。

> 板子：USB 直插式功率计（Type-C 公头 + USB-A 公头一体），约 25 × 30 mm，2 层板。
> 主控：**TI MSPM0G3507SPMR**（LQFP-64，Cortex-M0+） ｜ 计量：**INA228AIDGSR** ｜ 显示：HS96L01W4S03（7P 排针） ｜ 电源：**LGS5145** 降压到 3.3V

---

## 1. 目录结构

```
.
├─ README.md                     ← 你在这里
├─ hardware/
│  ├─ 功率计算器总.eprj2          ← 最新工程（SQLite，正文加密）
│  ├─ backups/                   ← 工程内自动备份（.epro2 = zip，内含明文 .epru）
│  ├─ photos/                    ← 实物照片
│  └─ render/                    ← 原理图 / PCB 预览图
├─ docs/
│  ├─ 诊断-无法下载.md            ← 主报告（现象/结论/量测清单/改板建议）
│  └─ 证据-原理图与PCB核对.md     ← 从工程里抠出来的网表与铜箔证据
├─ tools/
│  ├─ eprj_tool.py               ← 解包 + 提网表 + 提 PCB 铜箔（仅标准库）
│  └─ README.md
└─ data/                         ← 工具产出的证据（自动生成，已入库）
   ├─ sch_netlist.txt            ← 原理图网表（105 个网络）
   ├─ pcb_copper_report.txt      ← PCB 走线/焊盘网络/距离排查
   └─ eprj2_tables.txt           ← .eprj2 的 SQLite 表结构概况
```

---

## 2. 关键结论（截至 2026-10-19 备份）

**现象**：`JLink - Cortex-M Error / No Cortex-M SW Device Found`

**已排除**（用工程文件 + 万用表确认）：

- 供电和共地正常（只接 VCC+GND 时板上 LED 亮）
- `U2.1 → U8 pad12 (PA19/SWDIO)`、`U2.2 → U8 pad13 (PA20/SWCLK)` 走线端到端通
- `U8 pad40 = VCC`、`pad41 = GND`、`pad32 = VCORE + 470nF` 都对
- 符号库脚号和 TI 手册一致（pin12 = PA19/SWDIO，pin13 = PA20/SWCLK）

**待查**（详见 [docs/诊断-无法下载.md](docs/诊断-无法下载.md)）：

| 优先级 | 检查点 | 期望 | 不对说明 |
|---|---|---|---|
| 1 | `VCORE`（U8 pad32） | 1.2~1.4V | 芯片没工作/虚焊/已损坏 |
| 2 | `NRST`（U8 pad38） | ≈3.3V | 复位被拉住 → 飞 10k 上拉 |
| 3 | `U2-1 ↔ U8-12` / `U2-2 ↔ U8-13` | 各 ≈0Ω | 走线断 / 接线交叉 |
| 4 | USB-C 上电时 `VCC` | 3.3V | 4.1~4.3V = 分压错，芯片可能已过压 |
| 5 | J-Link 器件/复位方式 | MSPM0G3507 + Connect under reset | 工具链问题 |

> ⚠️ LED 接在 VCC 轨上，**它亮只说明 3.3V 有电，不代表 MCU 活着**。

---

## 3. 已知设计问题（下一版要改）

1. **调试排针 U2 第 4 脚（GND）在 PCB 上没有走线**，只靠铺铜连通 → 改版时容易丢，且 DRC 查不出来。
2. **NRST 只有一颗 10nF 电容**：原理图里的 `4.7kΩ 上拉 + SW7 复位键 + H2 排针` 都没有落到 PCB。
3. **3.3V 反馈网络原理图与 PCB 不一致**：
   - 原理图：`LGS5145-FB` 只连到 **H3 的三个脚，而这三角被一根线短接**；三个分压中点 `FB1/FB2/FB3` 只连到 **H4** —— 反馈环是开的；
   - PCB：FB 直接接 `13K1/3K1`（5.23×），而原理图的 3.3V 档是 `100k/32k`（4.13×）。
4. **没有引出任何串口**（PA10/PA11 未接），也没有 USB-串口芯片 → 想"读端口"只能从 H5（ALERT/SDA/SCL 的 I2C）或 OLED 口想办法；MSPM0 是 M0+，没有 SWO。

---

## 4. 引脚对应速查

### MCU（原理图 U4 / PCB U8，MSPM0G3507SPMR）

| 功能 | pad | 说明 |
|---|---|---|
| SWDIO | 12 | PA19/SWDIO → U2.1 |
| SWCLK | 13 | PA20/SWCLK → U2.2 |
| NRST | 38 | 10nF 到地（PCB 无上拉/按键） |
| VDD | 40 | VCC |
| VSS | 41 | GND |
| VCORE | 32 | 只接 470nF 到地 |
| SDA / SCL | 9 / 10 | PA16/PA17 → INA228 + H5 + 10k 上拉 |
| C0-SDA / C0-SCL | 43 / 44 | OLED 的 I2C |

### 调试排针 U2（HDR-TH_4P-P2.54，封装 tag 写着 SWD）

| U2 | 网络 | 通用 4P SWD 线序 |
|---|---|---|
| 1 | **SWDIO** | 1 = 3V3 |
| 2 | **SWCLK** | 2 = SWDIO |
| 3 | **VCC** | 3 = SWCLK |
| 4 | **GND** | 4 = GND |

> ⚠️ **不能 1:1 直插**：1:1 时地是对的、电也照进（LED 会亮），但 SWDIO/SWCLK 全错。

---

## 5. 怎么复现分析

工程 `.eprj2` 的正文是加密的，**连接性分析一律走 `_backup/*.epro2`**（zip 里的 `.epru` 是明文）：

```bash
# 1) 解包最新备份
python tools/eprj_tool.py unpack hardware/backups/功率计算器总_2026-10-19-20-33.epro2 -o build/

# 2) 原理图网表
python tools/eprj_tool.py netlist build/功率计算器总.epru -o data/sch_netlist.txt

# 3) PCB 铜箔 / 焊盘网络 / 距离排查
python tools/eprj_tool.py pcb build/功率计算器总.epru -o data/pcb_copper_report.txt
```

只依赖 Python 标准库（3.8+）。

---

## 6. 推到远端（可选）

```bash
git remote add origin <你的仓库地址>
git branch -M main
git push -u origin main
```

> 仓库现在只有本地历史，没有配置 remote。工程文件约 3.5MB + 备份约 4.9MB，普通 Git/GitHub 完全放得下；如果以后备份变多，再考虑 Git LFS 或只保留里程碑版本。

---

## 7. 备份时间线

| 备份 | 说明 |
|---|---|
| 10-13 13:46 → 22:25（6 份） | 当天下午到晚上的连续改动（PCB 逐步布线） |
| **10-19 20:33** | 最新。相比 10-13：元件 29→28（删掉一颗 7K），走线 348→352，过孔 56→54 |

工程文件 `功率计算器总.eprj2` 的修改时间：2026-10-19 20:34。
