#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
eprj_tool.py —— 嘉立创EDA专业版工程(.eprj2 / .epro2)离线分析工具

背景：.eprj2 是 SQLite，但原理图/PCB 内容被加密，无法直接读。
      .epro2（工程内自动备份）本质是个 zip，里面的 .epru 是**明文**的按行 JSON，
      所以所有连接性分析都走 .epro2 → .epru 这条路。

用法：
    python eprj_tool.py unpack  <xxx.epro2|xxx.eprj2> -o <输出目录>
    python eprj_tool.py netlist <xxx.epru>  [-o 网表.txt]
    python eprj_tool.py pcb     <xxx.epru>  [-o 报告.txt]

依赖：仅标准库。Python 3.8+。
"""

import argparse
import base64
import collections
import io
import json
import math
import os
import sqlite3
import sys
import zipfile

# ----------------------------------------------------------------------------
# 基础：读取 .epru
# ----------------------------------------------------------------------------

def load_docs(epru_path):
    """把 .epru 解析成 [{head, items:[(head, body)]}, ...]"""
    with open(epru_path, encoding='utf-8', errors='replace') as f:
        raw = f.read()

    docs, cur = [], None
    for ln in raw.split('\n'):
        if not ln.strip():
            continue
        parts = ln.split('||')
        head = json.loads(parts[0])
        body = None
        if len(parts) > 1:
            s = parts[1].rstrip('|\r')
            if s.strip():
                try:
                    body = json.loads(s)
                except Exception:
                    body = {'_raw': s[:300]}
        if head.get('type') == 'DOCHEAD':
            cur = {'head': body or head, 'items': []}
            docs.append(cur)
        elif cur is not None:
            cur['items'].append((head, body))
    return docs


def get_doc(docs, doctype):
    for d in docs:
        if d['head'].get('docType') == doctype:
            return d
    return None


# ----------------------------------------------------------------------------
# unpack
# ----------------------------------------------------------------------------

def cmd_unpack(args):
    src = args.src
    out = args.out or 'unpacked'
    os.makedirs(out, exist_ok=True)

    if src.lower().endswith('.eprj2'):
        # SQLite：只能导元数据（正文加密）。复制到临时文件，避免锁住工程
        import shutil
        import tempfile
        tmp = os.path.join(tempfile.gettempdir(), 'eprj2_copy.db')
        shutil.copyfile(src, tmp)
        db = sqlite3.connect(tmp)
        cur = db.cursor()
        info = []
        tables = [r[0] for r in cur.execute(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name").fetchall()]
        for name in tables:
            try:
                n = cur.execute('SELECT COUNT(*) FROM "%s"' % name).fetchone()[0]
            except Exception as e:
                n = 'ERR:%s' % e
            info.append((name, n))
        lines = ['# %s —— SQLite 元数据（正文加密，无法解密）' % os.path.basename(src), '']
        lines += ['%-40s %s' % (n, c) for n, c in info]
        lines += ['', '# 提示：连接性分析请用同目录 _backup/*.epro2']
        dst = os.path.join(out, 'eprj2_tables.txt')
        open(dst, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
        print('wrote', dst)
        return

    if src.lower().endswith('.epro2'):
        with zipfile.ZipFile(src) as z:
            z.extractall(out)
            names = z.namelist()
        print('extracted:', ', '.join(names))
        for n in names:
            if n.lower().endswith('.epru'):
                print('  -> epr 明文：', os.path.join(out, n))
        return

    sys.exit('unsupported file: %s' % src)


# ----------------------------------------------------------------------------
# 原理图网表
# ----------------------------------------------------------------------------

# 实测坐标约定：rot 为顺时针，且没有镜像元件
def _transform(px, py, rot, cx, cy):
    r = rot % 360
    q = {0: (px, py), 90: (py, -px), 180: (-px, -py), 270: (-py, px)}[r]
    return (round(cx + q[0], 3), round(cy + q[1], 3))


class _UF:
    def __init__(self):
        self.p = {}

    def find(self, a):
        self.p.setdefault(a, a)
        while self.p[a] != a:
            self.p[a] = self.p[self.p[a]]
            a = self.p[a]
        return a

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[ra] = rb


def sch_netlist(epru_path):
    docs = load_docs(epru_path)
    sch = get_doc(docs, 'SCH_PAGE')
    syms = {d['head'].get('uuid'): d for d in docs if d['head'].get('docType') == 'SYMBOL'}
    if sch is None:
        sys.exit('no SCH_PAGE found')

    comps = {}
    for h, b in sch['items']:
        if h.get('type') == 'COMPONENT':
            comps[h['id']] = dict(id=h['id'], x=b.get('x'), y=b.get('y'),
                                  rot=b.get('rotation') or 0, attrs={})
    for h, b in sch['items']:
        if h.get('type') == 'ATTR' and b.get('parentId') in comps:
            comps[b['parentId']]['attrs'][b.get('key')] = b.get('value')

    def sym_pins(su):
        d = syms[su]
        pa = collections.defaultdict(dict)
        for h, b in d['items']:
            if h.get('type') == 'ATTR':
                pa[b.get('parentId')][b.get('key')] = b.get('value')
        g = None
        for h, b in d['items']:
            if h.get('type') == 'ATTR' and b.get('key') == 'Global Net Name':
                g = b.get('value')
        out = []
        for h, b in d['items']:
            if h.get('type') == 'PIN':
                a = pa.get(h['id'], {})
                out.append(dict(x=b.get('x'), y=b.get('y'),
                                name=a.get('Pin Name'), num=a.get('Pin Number'), glob=g))
        return out

    segs = []
    for h, b in sch['items']:
        if h.get('type') == 'LINE':
            segs.append((b.get('lineGroup'),
                         (round(b.get('startX'), 3), round(b.get('startY'), 3)),
                         (round(b.get('endX'), 3), round(b.get('endY'), 3))))
    wirenet = {}
    for h, b in sch['items']:
        if h.get('type') == 'ATTR' and b.get('key') == 'NET':
            wirenet[b.get('parentId')] = b.get('value')

    uf = _UF()
    for wid, p1, p2 in segs:
        uf.union(('W', wid), ('P', p1))
        uf.union(('W', wid), ('P', p2))

    named = collections.defaultdict(set)
    for wid, n in wirenet.items():
        if n:
            named[wid].add(n)
    byname = collections.defaultdict(list)
    for wid, names in named.items():
        for n in names:
            byname[n].append(wid)
    for n, wids in byname.items():
        for w in wids[1:]:
            uf.union(('W', wids[0]), ('W', w))

    pins = []
    for c in comps.values():
        su = c['attrs'].get('Symbol')
        if not su or su not in syms:
            continue
        des = c['attrs'].get('Designator') or '[%s]' % (su[:8])
        for p in sym_pins(su):
            pt = _transform(p['x'], p['y'], c['rot'], c['x'], c['y'])
            key = ('N', c['id'], str(p['num']))
            pins.append((des, p['name'], p['num'], c['attrs'].get('Value'), p['glob'], key))
            uf.union(key, ('P', pt))
            if p['glob']:
                uf.union(key, ('GLOBAL', p['glob']))

    groups = collections.defaultdict(lambda: {'pins': [], 'names': set()})
    for t in pins:
        groups[uf.find(t[5])]['pins'].append(t[:5])
    for wid, names in named.items():
        groups[uf.find(('W', wid))]['names'] |= names
    for t in pins:
        if t[4]:
            groups[uf.find(t[5])]['names'].add(t[4])

    # 同名网络合并（net label 语义）
    name2g = collections.defaultdict(list)
    for g, d in groups.items():
        for n in d['names']:
            name2g[n].append(g)
    for n, gs in name2g.items():
        for g in gs[1:]:
            uf.union(g, gs[0])

    final = collections.defaultdict(lambda: {'pins': [], 'names': set()})
    for t in pins:
        final[uf.find(t[5])]['pins'].append(t[:5])
    for wid, names in named.items():
        final[uf.find(('W', wid))]['names'] |= names
    for t in pins:
        if t[4]:
            final[uf.find(t[5])]['names'].add(t[4])

    out = []
    for d in final.values():
        if d['pins']:
            out.append((sorted(d['names']), sorted(set(d['pins']))))
    out.sort(key=lambda t: (-len(t[1]), str(t[0])))
    return out


def cmd_netlist(args):
    nets = sch_netlist(args.epru)
    lines = ['# 原理图网表（自动提取，net 数量：%d）' % len(nets), '']
    for names, pins in nets:
        label = ','.join(names) if names else '(unnamed)'
        lines.append('[%s] (%d): %s' % (
            label, len(pins),
            '; '.join('%s.%s(%s)' % (d, n, num) for d, n, num, val, glob in pins)))
    text = '\n'.join(lines) + '\n'
    if args.out:
        open(args.out, 'w', encoding='utf-8').write(text)
        print('wrote', args.out)
    else:
        print(text)


# ----------------------------------------------------------------------------
# PCB 铜箔
# ----------------------------------------------------------------------------

def _pcb_model(docs):
    pcb = get_doc(docs, 'PCB')
    if pcb is None:
        sys.exit('no PCB doc found')
    fps = {d['head'].get('uuid'): d for d in docs if d['head'].get('docType') == 'FOOTPRINT'}
    attrs = collections.defaultdict(dict)
    for h, b in pcb['items']:
        if h.get('type') == 'ATTR':
            attrs[b.get('parentId')][b.get('key')] = b.get('value')
    comps = {}
    for h, b in pcb['items']:
        if h.get('type') == 'COMPONENT':
            comps[h['id']] = dict(x=b.get('x'), y=b.get('y'), angle=b.get('angle') or 0)

    def pads_of(fu):
        return [dict(num=str(b.get('num')), x=b.get('centerX') or 0, y=b.get('centerY') or 0)
                for h, b in fps[fu]['items'] if h.get('type') == 'PAD']

    def rot(px, py, a):
        a %= 360
        return {0: (px, py), 90: (-py, px), 180: (-px, -py), 270: (py, -px)}[a]

    pads = []
    for cid, c in comps.items():
        fu = attrs[cid].get('Footprint')
        if not fu or fu not in fps:
            continue
        des = attrs[cid].get('Designator') or cid[:6]
        for p in pads_of(fu):
            px, py = rot(p['x'], p['y'], c['angle'])
            pads.append(dict(des=des, num=p['num'], x=c['x'] + px, y=c['y'] + py))

    tracks = collections.defaultdict(list)
    vias = collections.defaultdict(list)
    for h, b in pcb['items']:
        if h.get('type') == 'LINE':
            tracks[b.get('netName')].append((b.get('startX'), b.get('startY'),
                                             b.get('endX'), b.get('endY'), b.get('layerId')))
        elif h.get('type') == 'VIA':
            vias[b.get('netName')].append((b.get('centerX'), b.get('centerY')))

    pours = []
    for h, b in pcb['items']:
        if h.get('type') == 'POUR':
            pours.append(dict(net=b.get('netName'), layer=b.get('layerId'),
                              nfill=len((b.get('pourFill') or [])), path=b.get('path')))
    return dict(comps=comps, attrs=attrs, pads=pads, tracks=tracks, vias=vias, pours=pours)


def _dist_seg(p, a, b):
    px, py = p
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    l2 = dx * dx + dy * dy
    if l2 == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def cmd_pcb(args):
    docs = load_docs(args.epru)
    m = _pcb_model(docs)
    scale = 0.0254  # 1 单位 = 0.0254mm（2.54mm 间距 = 100 单位）

    L = []
    L.append('# PCB 分析报告')
    L.append('')
    L.append('元件数：%d（%s）' % (len(m['comps']),
                                ', '.join(sorted(m['attrs'][c].get('Designator') or '?'
                                                 for c in m['comps']))))
    L.append('走线：%d 段 / 过孔：%d 个 / 铺铜定义：%d 块'
             % (sum(len(v) for v in m['tracks'].values()),
                sum(len(v) for v in m['vias'].values()), len(m['pours'])))
    L.append('')

    L.append('## 各网络走线数量')
    for net, ss in sorted(m['tracks'].items(), key=lambda kv: -len(kv[1])):
        L.append('  %-8s %d 段, %d 过孔' % (net, len(ss), len(m['vias'].get(net, []))))
    L.append('')

    # pad -> 网络（靠“铜箔端点/过孔中心与焊盘中心重合”判定）
    ptnet = collections.defaultdict(set)
    for net, ss in m['tracks'].items():
        for x1, y1, x2, y2, ly in ss:
            ptnet[(round(x1, 1), round(y1, 1))].add(net)
            ptnet[(round(x2, 1), round(y2, 1))].add(net)
    for net, vv in m['vias'].items():
        for x, y in vv:
            ptnet[(round(x, 1), round(y, 1))].add(net)

    L.append('## 焊盘 → 网络（None = 无走线端点，可能靠铺铜）')
    bydes = collections.defaultdict(list)
    for p in m['pads']:
        nets = ptnet.get((round(p['x'], 1), round(p['y'], 1)))
        bydes[p['des']].append((p['num'], sorted(nets) if nets else None, p['x'], p['y']))
    for des in sorted(bydes):
        L.append('  %-8s %s' % (des, '; '.join(
            '%s:%s' % (n, v if v else 'None') for n, v, x, y in bydes[des])))
    L.append('')

    L.append('## 各网络焊盘到铜箔的距离（排查“没连上”的焊盘）')
    L.append('   * 距离单位 mm；>1mm 基本可以认为没有走线连接')
    for des in sorted(bydes):
        for num, net, x, y in bydes[des]:
            if net:
                continue
            best = []
            for n2, ss in m['tracks'].items():
                d = min(_dist_seg((x, y), (s[0], s[1]), (s[2], s[3])) for s in ss)
                best.append((d, n2))
            for n2, vv in m['vias'].items():
                for v in vv:
                    best.append((math.hypot(x - v[0], y - v[1]), n2 + '(via)'))
            best.sort()
            L.append('  %-8s pad %-4s (%.0f,%.0f) 最近铜箔：%s %.1fmm / %s %.1fmm'
                     % (des, num, x, y, best[0][1], best[0][0] * scale,
                        best[1][1], best[1][0] * scale))
    L.append('')

    L.append('## 铺铜')
    for p in m['pours']:
        L.append('  %-6s layer=%s 填充数据=%s %s'
                 % (p['net'], p['layer'], p['nfill'] if p['nfill'] else '空(未生成)',
                    json.dumps(p['path'])[:90]))

    text = '\n'.join(L) + '\n'
    if args.out:
        open(args.out, 'w', encoding='utf-8').write(text)
        print('wrote', args.out)
    else:
        print(text)


# ----------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description='嘉立创EDA专业版工程离线分析')
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('unpack', help='解包 .epro2(zip) 或查看 .eprj2(SQLite) 元数据')
    p.add_argument('src')
    p.add_argument('-o', '--out', default='unpacked')
    p.set_defaults(func=cmd_unpack)

    p = sub.add_parser('netlist', help='从 .epru 提取原理图网表')
    p.add_argument('epru')
    p.add_argument('-o', '--out')
    p.set_defaults(func=cmd_netlist)

    p = sub.add_parser('pcb', help='从 .epru 提取 PCB 铜箔/焊盘网络报告')
    p.add_argument('epru')
    p.add_argument('-o', '--out')
    p.set_defaults(func=cmd_pcb)

    args = ap.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
