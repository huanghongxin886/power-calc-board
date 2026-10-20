#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_site.py —— 把仓库生成一个自包含的浏览页（单文件 HTML，图片内嵌 base64）

用法：
    python tools/make_site.py            # 生成 ./view.html
    python tools/make_site.py -o x.html

设计目标：不依赖服务器、不依赖 CDN、双击就能看（file:// 也能用）。
"""

import argparse
import base64
import html
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TEXT_EXT = {'.md', '.txt', '.py', '.json', '.json2', '.yml', '.yaml', '.sh', '.ps1', '.cfg', '.ini'}
IMG_EXT = {'.png', '.jpg', '.jpeg', '.webp', '.gif', '.bmp'}


# --------------------------------------------------------------------------
# 极简 markdown → html（够用即可：标题/列表/表格/代码块/引用/分隔线/行内语法）
# --------------------------------------------------------------------------

def inline(s):
    s = html.escape(s)
    # 图片 ![alt](src)
    def img(m):
        alt, src = m.group(1), m.group(2)
        return '<img alt="%s" src="%s">' % (alt, src)
    s = re.sub(r'!\[([^\]]*)\]\(([^)]+)\)', img, s)
    # 链接 [text](href)
    def link(m):
        text, href = m.group(1), m.group(2)
        return '<a href="%s">%s</a>' % (href, text)
    s = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', link, s)
    s = re.sub(r'`([^`]+)`', r'<code>\1</code>', s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', s)
    s = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<em>\1</em>', s)
    return s


def md2html(md):
    lines = md.split('\n')
    out = []
    i = 0
    n = len(lines)
    while i < n:
        ln = lines[i]

        if ln.startswith('```'):
            lang = ln[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].startswith('```'):
                buf.append(lines[i])
                i += 1
            i += 1
            out.append('<pre class="code"><code>%s</code></pre>' % html.escape('\n'.join(buf)))
            continue

        if re.match(r'^\s*\|', ln) and i + 1 < n and re.match(r'^\s*\|[\s:|-]+\|\s*$', lines[i + 1]):
            header = [c.strip() for c in ln.strip().strip('|').split('|')]
            i += 2
            rows = []
            while i < n and re.match(r'^\s*\|', lines[i]):
                rows.append([c.strip() for c in lines[i].strip().strip('|').split('|')])
                i += 1
            t = ['<table><thead><tr>']
            t += ['<th>%s</th>' % inline(c) for c in header]
            t.append('</tr></thead><tbody>')
            for r in rows:
                t.append('<tr>' + ''.join('<td>%s</td>' % inline(c) for c in r) + '</tr>')
            t.append('</tbody></table>')
            out.append(''.join(t))
            continue

        m = re.match(r'^(#{1,6})\s+(.*)$', ln)
        if m:
            lvl = len(m.group(1))
            out.append('<h%d>%s</h%d>' % (lvl, inline(m.group(2)), lvl))
            i += 1
            continue

        if re.match(r'^\s*([-*+])\s+', ln):
            items = []
            while i < n and re.match(r'^\s*([-*+])\s+', lines[i]):
                items.append(re.sub(r'^\s*[-*+]\s+', '', lines[i]))
                i += 1
            out.append('<ul>' + ''.join('<li>%s</li>' % inline(x) for x in items) + '</ul>')
            continue

        if re.match(r'^\s*\d+\.\s+', ln):
            items = []
            while i < n and re.match(r'^\s*\d+\.\s+', lines[i]):
                items.append(re.sub(r'^\s*\d+\.\s+', '', lines[i]))
                i += 1
            out.append('<ol>' + ''.join('<li>%s</li>' % inline(x) for x in items) + '</ol>')
            continue

        if ln.startswith('>'):
            buf = []
            while i < n and lines[i].startswith('>'):
                buf.append(lines[i].lstrip('>').strip())
                i += 1
            out.append('<blockquote>%s</blockquote>' % inline(' '.join(buf)))
            continue

        if re.match(r'^\s*---+\s*$', ln):
            out.append('<hr>')
            i += 1
            continue

        if not ln.strip():
            i += 1
            continue

        buf = []
        while i < n and lines[i].strip() and not re.match(r'^(#{1,6}\s|```|\s*\||\s*[-*+]\s|\s*\d+\.\s|>)', lines[i]):
            buf.append(lines[i])
            i += 1
        out.append('<p>%s</p>' % inline(' '.join(buf)))

    return '\n'.join(out)


# --------------------------------------------------------------------------

def git(*args):
    try:
        return subprocess.check_output(['git'] + list(args), cwd=ROOT,
                                       stderr=subprocess.DEVNULL).decode('utf-8', 'replace').strip()
    except Exception:
        return ''


def list_files():
    out = git('ls-files')
    if out:
        return [p for p in out.split('\n') if p.strip()]
    files = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in ('.git', 'build', '__pycache__')]
        for f in filenames:
            files.append(os.path.relpath(os.path.join(dirpath, f), ROOT).replace('\\', '/'))
    return sorted(files)


def build(out_path):
    files = list_files()
    readme = ''
    for cand in ('README.md', 'readme.md'):
        if cand in files:
            readme = cand
            break

    log = git('log', '--pretty=format:%h  %ad  %s', '--date=format:%Y-%m-%d %H:%M')

    sections = []
    nav = []
    for f in files:
        ext = os.path.splitext(f)[1].lower()
        label = html.escape(f)
        if ext in IMG_EXT:
            with open(os.path.join(ROOT, f), 'rb') as fh:
                b64 = base64.b64encode(fh.read()).decode()
            mime = 'image/jpeg' if ext in ('.jpg', '.jpeg') else 'image/' + ext.lstrip('.')
            body = '<img class="shot" src="data:%s;base64,%s">' % (mime, b64)
            nav.append('<li><a href="#%s" data-t="%s">%s <span class="tag">img</span></a></li>'
                       % (html.escape(f), html.escape(f), label))
        elif ext in TEXT_EXT:
            try:
                txt = open(os.path.join(ROOT, f), encoding='utf-8').read()
            except Exception:
                txt = '(无法按 UTF-8 读取)'
            if ext == '.md':
                body = '<div class="md">%s</div>' % md2html(txt)
            else:
                body = '<pre class="code"><code>%s</code></pre>' % html.escape(txt)
            nav.append('<li><a href="#%s" data-t="%s">%s</a></li>'
                       % (html.escape(f), html.escape(f), label))
        else:
            size = os.path.getsize(os.path.join(ROOT, f))
            body = '<p class="dim">二进制文件（%.1f KB），不在页面内展示。</p>' % (size / 1024.0)
            nav.append('<li><a href="#%s" data-t="%s">%s <span class="tag">bin</span></a></li>'
                       % (html.escape(f), html.escape(f), label))
        sections.append('<section id="%s" data-t="%s"><h2 class="path">%s</h2>%s</section>'
                        % (html.escape(f), html.escape(f), html.escape(f), body))

    doc = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>功率计算器总 · 仓库浏览</title>
<style>
 :root{--bg:#0f1216;--pnl:#161b22;--line:#283040;--fg:#d7dde7;--dim:#8b97a8;--acc:#4ea1ff;}
 *{box-sizing:border-box}
 body{margin:0;font:14px/1.7 -apple-system,"Segoe UI","Microsoft YaHei",sans-serif;background:var(--bg);color:var(--fg);display:flex;height:100vh;overflow:hidden}
 aside{width:290px;flex:0 0 290px;background:var(--pnl);border-right:1px solid var(--line);overflow:auto;padding:14px 10px}
 aside .brand{font-weight:700;font-size:15px;margin:0 6px 4px}
 aside .sub{color:var(--dim);font-size:12px;margin:0 6px 12px;line-height:1.5;white-space:pre-wrap}
 ul{list-style:none;margin:0;padding:0}
 aside li a{display:block;padding:4px 8px;border-radius:6px;color:var(--fg);text-decoration:none;font-size:13px;word-break:break-all}
 aside li a:hover{background:#1f2632}
 aside li a.on{background:#1d3a5c;color:#fff}
 .tag{font-size:10px;color:var(--dim);border:1px solid var(--line);border-radius:4px;padding:0 4px}
 main{flex:1;overflow:auto;padding:26px 34px 80px}
 section{display:none;max-width:1000px}
 section:first-of-type{display:block}
 section:target{display:block}
 h1,h2,h3,h4{line-height:1.35}
 h1{font-size:24px;border-bottom:1px solid var(--line);padding-bottom:8px}
 h2{font-size:19px;margin-top:28px}
 h2.path{font-size:13px;color:var(--dim);font-family:ui-monospace,Consolas,monospace;font-weight:400;margin:0 0 14px}
 h3{font-size:16px}
 code{background:#1f2632;border-radius:4px;padding:1px 5px;font-family:ui-monospace,Consolas,monospace;font-size:12.5px}
 pre.code{background:#11161d;border:1px solid var(--line);border-radius:8px;padding:14px;overflow:auto}
 pre.code code{background:none;padding:0;font-size:12.5px;line-height:1.6}
 table{border-collapse:collapse;margin:14px 0;width:100%;font-size:13px}
 th,td{border:1px solid var(--line);padding:7px 10px;text-align:left;vertical-align:top}
 th{background:#1b2230}
 blockquote{margin:14px 0;padding:10px 14px;border-left:3px solid var(--acc);background:#141b25;color:#c8d2e0;border-radius:0 8px 8px 0}
 a{color:var(--acc)}
 hr{border:none;border-top:1px solid var(--line);margin:22px 0}
 img{max-width:100%;border-radius:8px;border:1px solid var(--line);margin:10px 0}
 img.shot{max-width:420px}
 .dim{color:var(--dim)}
 .hint{color:var(--dim);font-size:12px;margin-top:40px}
</style></head><body>
<aside>
  <div class="brand">功率计算器总</div>
  <div class="sub">本地 Git 仓库浏览页
分支 main · {{NFILES}} 个文件</div>
  <ul>{{NAV}}</ul>
  <div class="sub" style="margin-top:18px;border-top:1px solid var(--line);padding-top:12px">最近提交
{{LOG}}</div>
</aside>
<main>{{BODY}}
<div class="hint">← 左侧点文件切换。本页由 <code>tools/make_site.py</code> 生成，单文件自包含。</div>
</main>
<script>
 document.querySelectorAll('aside a').forEach(function(a){
   a.addEventListener('click', function(){
     document.querySelectorAll('aside a').forEach(function(x){x.classList.remove('on')});
     a.classList.add('on');
   });
 });
 var first=document.querySelector('aside a'); if(first) first.classList.add('on');
</script>
</body></html>"""

    doc = (doc.replace('{{NFILES}}', str(len(files)))
              .replace('{{NAV}}', '\n'.join(nav))
              .replace('{{BODY}}', '\n'.join(sections))
              .replace('{{LOG}}', html.escape(log)))

    open(out_path, 'w', encoding='utf-8').write(doc)
    print('wrote', out_path, '(%d files, %.1f KB)' % (len(files), os.path.getsize(out_path) / 1024.0))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('-o', '--out', default=os.path.join(ROOT, 'view.html'))
    build(os.path.abspath(ap.parse_args().out))
