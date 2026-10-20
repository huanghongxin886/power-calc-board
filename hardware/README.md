# hardware/ —— 工程文件与实物

## 文件

| 文件 | 大小 | 说明 |
|---|---|---|
| `功率计算器总.eprj2` | 3.5 MB | **最新工程**（嘉立创EDA专业版）。SQLite 格式，文档正文 `dataStr` 加密，只能读表结构和文档树 |
| `backups/*.epro2` | 640 KB ~ 776 KB × 7 | 工程内自动备份。**zip 结构**，里面的 `.epru` 是明文，可解析网表/铜箔 |
| `photos/board-01.jpg` | | 实物：Type-C 公头侧，可见 4P 调试排针（U2） |
| `photos/board-02.jpg` | | 实物：LQFP-64 主控侧，丝印 `14002740 Y12-2C1010`（嘉立创生产批次号） |
| `render/schematic.png` | | 原理图预览（1920×1360，从备份里的 IMAGE/ 导出） |
| `render/pcb-preview.png` | | PCB 预览（1920×1360，同上） |

> `.eprj2` 是二进制/加密的，**不要**直接 `git diff`；比较版本请看 `data/` 里由 `.epro2` 生成的结果，
> 或者直接用下面“怎么比两版”的命令。

## 备份时间线

| 备份 | 备注 |
|---|---|
| `2026-10-13-13-46` | 当天最早期快照 |
| `2026-10-13-17-17` | |
| `2026-10-13-19-25` | |
| `2026-10-13-20-25` | |
| `2026-10-13-21-25` | |
| `2026-10-13-22-25` | 我第一轮分析的版本 |
| `2026-10-19-20-33` | **最新**，配套 `.eprj2` 修改时间 10-19 20:34 |

## 怎么比两版

```bash
# 解包两份备份并生成网表，然后直接 diff 文本
python ../tools/eprj_tool.py unpack 功率计算器总_2026-10-13-22-25.epro2 -o /tmp/a
python ../tools/eprj_tool.py unpack 功率计算器总_2026-10-19-20-33.epro2 -o /tmp/b
python ../tools/eprj_tool.py netlist /tmp/a/功率计算器总.epru -o /tmp/a.txt
python ../tools/eprj_tool.py netlist /tmp/b/功率计算器总.epru -o /tmp/b.txt
diff /tmp/a.txt /tmp/b.txt
```

## 注意

- 工程里含嘉立创账号相关的 ID（`project_uuid`、`owner_uuid` 等），
- 备份里带 `IMAGE/*.webp` 预览图，`.gitignore` 已经把解包目录排除，只保留 `render/` 里导出的 PNG。
