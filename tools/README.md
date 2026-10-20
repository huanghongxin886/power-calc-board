# tools/ —— 工程文件离线分析

`eprj_tool.py`：把嘉立创EDA专业版工程拆成可读的网表和 PCB 铜箔报告。**只依赖 Python 标准库（3.8+）**。

## 三个子命令

### 1) unpack —— 解包

```bash
python eprj_tool.py unpack ../hardware/backups/功率计算器总_2026-10-19-20-33.epro2 -o build/
```

- `.epro2` → 当 zip 解开，得到 `IMAGE/`、`project2.json`、`<工程名>.epru`（明文，逐行 JSON）
- `.eprj2` → 只导 SQLite 表结构与行数（正文加密），存成 `eprj2_tables.txt`

### 2) netlist —— 原理图网表

```bash
python eprj_tool.py netlist build/功率计算器总.epru -o ../data/sch_netlist.txt
```

输出形如 `[SWDIO] (2): U2.1(1); U4.PA19/SWDIO(12)`。
网络名来自每根导线自带的 `NET` 属性（等价于网络标签），电源符号用 `Global Net Name`。

### 3) pcb —— 铜箔与焊盘

```bash
python eprj_tool.py pcb build/功率计算器总.epru -o ../data/pcb_copper_report.txt
```

给出：各网络走线段数/过孔数、**每个焊盘落在哪个网络**（依据"走线端点或过孔中心与焊盘中心重合"）、
以及**没有走线的焊盘离最近铜箔有多远**（用来抓"忘了连"的脚）、铺铜定义概况。

## 实现要点（踩过的坑）

1. `.eprj2` 的正文是加密的，**只能**从 `_backup/*.epro2` 里的明文 `.epru` 分析。
2. `.epru` 每行 `头||体|`，头里 `type` 决定对象种类；`DOCHEAD` 行开始一个新文档。
3. 符号引脚是符号坐标系，实例只有 `(x, y, rotation)`：
   - 平移：`abs = comp + pin`（用 33 个电源符号校准，33/33 命中）
   - 旋转：顺时针 `{0:(px,py), 90:(py,-px), 180:(-px,-py), 270:(-py,px)}`（用 rot=90/180/270 的阻容件校准，100% 命中）
4. 封装里焊盘坐标在 `centerX/centerY`，不是 `x/y`（用错会全落到原点）。
5. `sqlite3` 里**不要用同一个 cursor 嵌套查询**：`for name in cur.execute(...)` 里再 `cur.execute(...)` 会把外层迭代重置，只跑一行。
6. `.epru` 里少数中文字符是 GBK 残留，按 `errors='replace'` 读，别当异常处理。
