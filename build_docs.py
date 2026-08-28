#!/usr/bin/env python3
"""
build_docs.py — Gera um único ficheiro HTML de documentação a partir de uma pasta de .md

Uso:
    python build_docs.py <pasta_com_md> [output.html]

Exemplos:
    python build_docs.py ./docs
    python build_docs.py ./docs site/docs.html
"""

import os
import re
import sys
import json

# ─── CONFIGURAÇÃO ─────────────────────────────────────────────────────────────

# Mapeia prefixos/nomes de ficheiro para grupos na sidebar.
# Podes editar isto livremente.
GROUP_RULES = [
    (r'^README$',               'Overview',  0),
    (r'^zenpy_base$',           'Language',  1),
    (r'^zenpy_(math|io|os|path|json|time|struct|numpy)$', 'Language', 1),
    (r'^engine_api$',           'Engine',    2),
    (r'^zenpy_(canvas|font|image|audio|gif)$', 'Engine', 2),
    (r'^zenpy_gl\d*$',          'Graphics',  3),
    (r'^zenpy_(gll|glm|glfw|sdl2)$', 'Graphics', 3),
    (r'^zenpy_ray(lib|gui)$',   'Raylib',    4),
    (r'^zenpy_(http|net)$',     'Network',   5),
    (r'.*',                     'Extras',    6),   # fallback
]

# Títulos legíveis para IDs específicos. O resto é gerado automaticamente.
TITLE_OVERRIDES = {
    'README':         'Introduction',
    'engine_api':     'ZenEngine API',
    'zenpy_base':     'Base Library',
    'zenpy_gl':       'OpenGL (ZenGL)',
    'zenpy_gl4':      'OpenGL 4',
    'zenpy_gll':      'GL Loader',
    'zenpy_glm':      'GLM Math',
    'zenpy_glfw':     'GLFW',
    'zenpy_raylib':   'Raylib (RayPy)',
    'zenpy_raygui':   'RayGUI',
    'zenpy_dnn':      'Neural Networks',
    'zenpy_numpy':    'NumPy-style Arrays',
    'zenpy_rectpack': 'Rect Packer',
    'zenpy_sqlite':   'SQLite',
    'zenpy_sdl2':     'SDL2',
    'zenpy_io':       'I/O',
    'zenpy_os':       'OS',
    'zenpy_http':     'HTTP',
    'zenpy_net':      'Network',
    'zenpy_noise':    'Noise',
    'zenpy_canvas':   'Canvas',
    'zenpy_font':     'Font',
    'zenpy_image':    'Image',
    'zenpy_audio':    'Audio',
    'zenpy_gif':      'GIF',
    'zenpy_struct':   'Struct',
    'zenpy_time':     'Time',
    'zenpy_math':     'Math',
    'zenpy_json':     'JSON',
    'zenpy_path':     'Path',
}

# ─── HELPERS ──────────────────────────────────────────────────────────────────

def get_group(doc_id):
    for pattern, group, order in GROUP_RULES:
        if re.match(pattern, doc_id):
            return group, order
    return 'Extras', 99

def get_title(doc_id):
    if doc_id in TITLE_OVERRIDES:
        return TITLE_OVERRIDES[doc_id]
    return doc_id.replace('zenpy_', '').replace('_', ' ').title()

def js_escape(content):
    content = content.replace('\\', '\\\\')
    content = content.replace('`',  '\\`')
    content = content.replace('${', '\\${')
    return content

def slugify(text):
    text = re.sub(r'<[^>]+>', '', text)
    text = re.sub(r'[^\w\s-]', '', text.lower())
    text = re.sub(r'[\s_]+', '-', text)
    text = re.sub(r'-+', '-', text).strip('-')
    return text

# ─── LEITURA DOS FICHEIROS ────────────────────────────────────────────────────

def load_docs(folder):
    docs = []
    for fname in sorted(os.listdir(folder)):
        if not fname.endswith('.md'):
            continue
        doc_id = fname[:-3]
        path = os.path.join(folder, fname)
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
        group, order = get_group(doc_id)
        docs.append({
            'id':      doc_id,
            'title':   get_title(doc_id),
            'group':   group,
            'order':   order,
            'content': content,
        })
    docs.sort(key=lambda d: (d['order'], d['title']))
    return docs

# ─── BUILD SIDEBAR HTML ───────────────────────────────────────────────────────

def build_sidebar(docs):
    groups = {}
    for d in docs:
        groups.setdefault(d['group'], []).append(d)

    html = ''
    for group_name, group_docs in groups.items():
        gid = slugify(group_name)
        html += f'''
    <div class="sidebar-section">
      <div class="sgheader" onclick="toggleGroup(this)">
        {group_name}
        <span class="chevron">▾</span>
      </div>
      <div id="sg-{gid}">
'''
        for d in group_docs:
            html += (
                f'        <a class="doc-item" onclick="showDoc(\'{d["id"]}\')" data-id="{d["id"]}">\n'
                f'          <span class="dot"></span>{d["title"]}\n'
                f'        </a>\n'
                f'        <div class="section-nav" id="nav-{d["id"]}"></div>\n'
            )
        html += '      </div>\n    </div>\n'
    return html

# ─── BUILD JS DOCS ARRAY ──────────────────────────────────────────────────────

def build_js_docs(docs):
    entries = []
    for d in docs:
        escaped = js_escape(d['content'])
        entries.append(
            f'  {{ id: {json.dumps(d["id"])}, title: {json.dumps(d["title"])}, '
            f'group: {json.dumps(d["group"])}, content: `{escaped}` }}'
        )
    return '[\n' + ',\n'.join(entries) + '\n]'

# ─── TEMPLATE HTML ────────────────────────────────────────────────────────────

def build_html(docs, folder_name):
    sidebar_html = build_sidebar(docs)
    js_docs      = build_js_docs(docs)
    first_id     = docs[0]['id'] if docs else 'README'
    n_docs       = len(docs)

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>{folder_name} — Docs</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Syne:wght@400;600;700&family=JetBrains+Mono:wght@400;500&family=DM+Sans:ital,wght@0,300;0,400;0,500;1,400&display=swap" rel="stylesheet">
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/atom-one-dark.min.css">
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/python.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
<style>
*,*::before,*::after{{box-sizing:border-box;margin:0;padding:0}}
:root{{
  --bg:#0c0e14;--bg2:#12151e;--bg3:#1a1f2e;
  --border:#252a3a;--border2:#303550;
  --text:#c8d0e8;--text2:#7a84a8;--text3:#444d6a;
  --amber:#f59e0b;--amber2:#fbbf24;
  --amber-bg:rgba(245,158,11,0.07);--amber-border:rgba(245,158,11,0.22);
  --blue:#60a5fa;--green:#34d399;--heading:#eef0f8;
  --sidebar-w:268px;--top-h:52px;
}}
html{{scroll-behavior:smooth}}
body{{background:var(--bg);color:var(--text);font-family:'DM Sans',system-ui,sans-serif;font-size:15px;line-height:1.7;display:flex;flex-direction:column;min-height:100vh}}

/* TOP BAR */
.topbar{{position:fixed;top:0;left:0;right:0;z-index:200;height:var(--top-h);background:var(--bg2);border-bottom:1px solid var(--border);display:flex;align-items:center}}
.topbar-logo{{width:var(--sidebar-w);display:flex;align-items:center;gap:10px;padding:0 18px;border-right:1px solid var(--border);height:100%;flex-shrink:0}}
.logo-mark{{width:28px;height:28px;background:var(--amber);border-radius:6px;display:flex;align-items:center;justify-content:center;font-family:'Syne',sans-serif;font-weight:700;font-size:13px;color:#0c0e14;flex-shrink:0}}
.logo-text{{font-family:'Syne',sans-serif;font-size:16px;font-weight:700;color:var(--heading)}}
.logo-text span{{color:var(--amber)}}
.topbar-center{{flex:1;padding:0 20px;display:flex;align-items:center}}
.search-wrap{{display:flex;align-items:center;gap:8px;background:var(--bg3);border:1px solid var(--border);border-radius:8px;padding:0 12px;width:340px;height:34px;transition:border-color .15s}}
.search-wrap:focus-within{{border-color:var(--amber-border)}}
.search-wrap svg{{width:14px;height:14px;color:var(--text3);flex-shrink:0}}
.search-wrap input{{background:none;border:none;outline:none;color:var(--text);font-family:inherit;font-size:13.5px;width:100%}}
.search-wrap input::placeholder{{color:var(--text3)}}
.kbd{{padding:2px 6px;background:var(--bg3);border:1px solid var(--border2);border-radius:4px;font-size:10px;font-family:'JetBrains Mono',monospace;color:var(--text3)}}
.topbar-right{{padding-right:20px}}
.badge{{padding:3px 10px;background:var(--amber-bg);border:1px solid var(--amber-border);border-radius:20px;font-size:11px;font-family:'JetBrains Mono',monospace;color:var(--amber);white-space:nowrap}}

/* LAYOUT */
.layout{{display:flex;margin-top:var(--top-h);min-height:calc(100vh - var(--top-h))}}

/* SIDEBAR */
.sidebar{{width:var(--sidebar-w);flex-shrink:0;background:var(--bg2);border-right:1px solid var(--border);position:fixed;top:var(--top-h);bottom:0;overflow-y:auto;scrollbar-width:thin;scrollbar-color:var(--border2) transparent}}
.sidebar::-webkit-scrollbar{{width:4px}}
.sidebar::-webkit-scrollbar-thumb{{background:var(--border2);border-radius:4px}}
.sidebar-section{{padding:4px 0}}
.sidebar-section+.sidebar-section{{border-top:1px solid var(--border)}}
.sgheader{{padding:10px 14px 4px;font-size:10px;font-weight:500;letter-spacing:1.3px;text-transform:uppercase;color:var(--text3);font-family:'JetBrains Mono',monospace;cursor:pointer;display:flex;align-items:center;justify-content:space-between;user-select:none;transition:color .15s}}
.sgheader:hover{{color:var(--text2)}}
.chevron{{transition:transform .2s;display:inline-block}}
.sgheader.collapsed .chevron{{transform:rotate(-90deg)}}
.doc-item{{display:flex;align-items:center;gap:8px;padding:6px 14px;font-size:13px;color:var(--text2);cursor:pointer;border-left:2px solid transparent;transition:all .12s;user-select:none}}
.doc-item:hover{{color:var(--text);background:rgba(255,255,255,.03)}}
.doc-item.active{{color:var(--amber);background:var(--amber-bg);border-left-color:var(--amber)}}
.dot{{width:5px;height:5px;border-radius:50%;background:var(--border2);flex-shrink:0;transition:background .12s}}
.doc-item.active .dot{{background:var(--amber)}}
.section-nav{{overflow:hidden;max-height:0;transition:max-height .3s ease}}
.section-nav.open{{max-height:3000px}}
.section-link{{display:block;padding:4px 14px 4px 28px;font-size:12px;color:var(--text3);cursor:pointer;transition:color .1s;font-family:'JetBrains Mono',monospace;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;user-select:none}}
.section-link:hover{{color:var(--amber2)}}
.section-link.active{{color:var(--amber)}}

/* MAIN */
.main{{margin-left:var(--sidebar-w);flex:1;min-width:0}}
.content-area{{max-width:860px;margin:0 auto;padding:48px 48px 96px}}

/* SEARCH OVERLAY */
.search-overlay{{display:none;position:fixed;top:var(--top-h);left:var(--sidebar-w);right:0;bottom:0;background:var(--bg);z-index:100;overflow-y:auto}}
.search-overlay.visible{{display:block}}
.search-inner{{max-width:700px;margin:0 auto;padding:36px 48px 80px}}
.search-header{{font-family:'Syne',sans-serif;font-size:18px;color:var(--heading);margin-bottom:20px}}
.search-header em{{color:var(--amber);font-style:normal}}
.res-item{{padding:14px 18px;margin-bottom:8px;background:var(--bg2);border:1px solid var(--border);border-radius:9px;cursor:pointer;transition:border-color .15s,background .15s}}
.res-item:hover{{border-color:var(--amber-border);background:var(--bg3)}}
.res-doc{{font-size:10.5px;color:var(--amber);font-family:'JetBrains Mono',monospace;margin-bottom:3px;text-transform:uppercase;letter-spacing:.5px}}
.res-title{{font-size:14px;color:var(--heading);font-weight:500}}
.res-snip{{font-size:12.5px;color:var(--text2);margin-top:4px;line-height:1.5}}
mark{{background:rgba(245,158,11,.2);color:var(--amber2);border-radius:2px;padding:0 2px}}
.no-results{{color:var(--text3);font-size:14px;padding:20px 0}}

/* MARKDOWN */
.md-content{{display:none}}
.md-content.active{{display:block}}
.md-content h1{{font-family:'Syne',sans-serif;font-size:28px;font-weight:700;color:var(--heading);letter-spacing:-.5px;padding-bottom:18px;border-bottom:1px solid var(--border);margin-bottom:32px;line-height:1.2}}
.md-content h2{{font-family:'Syne',sans-serif;font-size:20px;font-weight:600;color:var(--heading);margin:56px 0 14px;padding-top:56px;border-top:1px solid var(--border);letter-spacing:-.2px;scroll-margin-top:calc(var(--top-h) + 20px)}}
.md-content h3{{font-family:'Syne',sans-serif;font-size:14.5px;font-weight:600;color:var(--amber);margin:28px 0 10px}}
.md-content h4{{font-size:12px;font-weight:500;color:var(--text2);font-family:'JetBrains Mono',monospace;margin:20px 0 8px;text-transform:uppercase;letter-spacing:.8px}}
.md-content p{{margin-bottom:14px}}
.md-content a{{color:var(--blue);text-decoration:none}}
.md-content a:hover{{text-decoration:underline}}
.md-content strong{{color:var(--heading);font-weight:500}}
.md-content hr{{border:none;border-top:1px solid var(--border);margin:40px 0}}
.md-content blockquote{{border-left:3px solid var(--amber);padding:10px 18px;margin:16px 0;background:var(--amber-bg);border-radius:0 6px 6px 0;color:var(--text2);font-size:14px}}
.md-content ul,.md-content ol{{padding-left:22px;margin-bottom:14px}}
.md-content li{{margin-bottom:4px}}
.md-content :not(pre)>code{{font-family:'JetBrains Mono',monospace;font-size:12.5px;background:var(--bg3);border:1px solid var(--border);color:var(--amber2);padding:2px 6px;border-radius:4px}}
.md-content pre{{margin:14px 0 22px;border-radius:10px;overflow:hidden;border:1px solid var(--border);position:relative}}
.md-content pre .hljs{{padding:18px 20px;font-family:'JetBrains Mono',monospace;font-size:13px;line-height:1.6;background:#0d1117;border-radius:10px}}
.copy-btn{{position:absolute;top:9px;right:9px;background:var(--bg3);border:1px solid var(--border2);color:var(--text2);border-radius:5px;padding:3px 9px;font-size:11px;font-family:'JetBrains Mono',monospace;cursor:pointer;opacity:0;transition:opacity .15s,color .15s,border-color .15s}}
.md-content pre:hover .copy-btn{{opacity:1}}
.copy-btn:hover{{color:var(--amber);border-color:var(--amber-border)}}
.copy-btn.ok{{color:var(--green);border-color:rgba(52,211,153,.4)}}
.md-content table{{width:100%;border-collapse:collapse;margin:14px 0 22px;font-size:13.5px}}
.md-content thead tr{{background:var(--bg3);border-bottom:1px solid var(--border2)}}
.md-content thead th{{padding:8px 14px;text-align:left;font-weight:500;color:var(--text2);font-family:'JetBrains Mono',monospace;font-size:11px;letter-spacing:.5px;text-transform:uppercase;white-space:nowrap}}
.md-content tbody tr{{border-bottom:1px solid var(--border);transition:background .1s}}
.md-content tbody tr:hover{{background:var(--bg3)}}
.md-content tbody tr:last-child{{border-bottom:none}}
.md-content td{{padding:8px 14px;vertical-align:top}}
.md-content td:first-child code{{color:var(--amber2);background:none;border:none;padding:0;font-size:13px}}
.doc-footer{{margin-top:56px;padding-top:18px;border-top:1px solid var(--border);font-size:11.5px;font-family:'JetBrains Mono',monospace;color:var(--text3);display:flex;justify-content:space-between;flex-wrap:wrap;gap:8px}}

@media(max-width:768px){{
  .sidebar{{display:none}}
  .main{{margin-left:0}}
  .content-area{{padding:24px 20px 60px}}
  .search-overlay{{left:0}}
  .topbar-logo{{width:auto}}
  .search-wrap{{width:180px}}
}}
</style>
</head>
<body>

<header class="topbar">
  <div class="topbar-logo">
    <div class="logo-mark">D</div>
    <span class="logo-text"><span>{folder_name}</span></span>
  </div>
  <div class="topbar-center">
    <div class="search-wrap">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><path d="m21 21-4.35-4.35"/></svg>
      <input type="text" id="searchInput" placeholder="Search all docs…" autocomplete="off" spellcheck="false">
      <span class="kbd">⌘K</span>
    </div>
  </div>
  <div class="topbar-right">
    <span class="badge">{n_docs} modules</span>
  </div>
</header>

<div class="layout">
  <nav class="sidebar">
{sidebar_html}
  </nav>

  <main class="main">
    <div class="search-overlay" id="searchOverlay">
      <div class="search-inner">
        <div class="search-header">Results for <em id="sqLabel"></em></div>
        <div id="searchList"></div>
      </div>
    </div>
    <div class="content-area" id="contentArea"></div>
  </main>
</div>

<script>
const docs = {js_docs};

const renderer = new marked.Renderer();
renderer.heading = function(token) {{
  const text  = typeof token === 'object' ? (token.text || '') : token;
  const level = typeof token === 'object' ? token.depth : arguments[1];
  const slug  = String(text).toLowerCase()
    .replace(/<[^>]+>/g,'').replace(/[^\\w\\s-]/g,'')
    .replace(/\\s+/g,'-').replace(/-+/g,'-').trim();
  return `<h${{level}} id="${{slug}}">${{text}}</h${{level}}>`;
}};
marked.setOptions({{ renderer, gfm: true, breaks: false }});

const contentArea = document.getElementById('contentArea');

docs.forEach(doc => {{
  const el = document.createElement('div');
  el.className = 'md-content';
  el.id = 'doc-' + doc.id;
  el.innerHTML = marked.parse(doc.content);
  el.querySelectorAll('pre').forEach(pre => {{
    const btn = document.createElement('button');
    btn.className = 'copy-btn'; btn.textContent = 'copy';
    btn.onclick = () => {{
      const code = pre.querySelector('code');
      navigator.clipboard.writeText(code ? code.innerText : pre.innerText).then(() => {{
        btn.textContent = 'copied!'; btn.classList.add('ok');
        setTimeout(() => {{ btn.textContent = 'copy'; btn.classList.remove('ok'); }}, 1800);
      }});
    }};
    pre.appendChild(btn);
  }});
  el.querySelectorAll('pre code').forEach(b => hljs.highlightElement(b));
  contentArea.appendChild(el);
  buildNav(doc);
}});

function buildNav(doc) {{
  const nav = document.getElementById('nav-' + doc.id);
  if (!nav) return;
  const headings = [];
  doc.content.split('\\n').forEach(line => {{
    const m = line.match(/^## (.+)/);
    if (m) {{
      const t = m[1];
      const slug = t.toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/-+/g,'-').trim();
      headings.push({{ t, slug }});
    }}
  }});
  nav.innerHTML = headings.map(h =>
    `<span class="section-link" data-slug="${{h.slug}}" data-doc="${{doc.id}}" onclick="jumpTo('${{doc.id}}','${{h.slug}}',this)">${{h.t}}</span>`
  ).join('');
}}

let currentDoc = null;

function showDoc(id, noScroll) {{
  if (currentDoc === id && !noScroll) return;
  currentDoc = id;
  document.querySelectorAll('.md-content').forEach(e => e.classList.remove('active'));
  document.querySelectorAll('.doc-item').forEach(e => e.classList.remove('active'));
  document.querySelectorAll('.section-nav').forEach(e => e.classList.remove('open'));
  const docEl = document.getElementById('doc-' + id);
  if (docEl) docEl.classList.add('active');
  const navEl = document.getElementById('nav-' + id);
  if (navEl) navEl.classList.add('open');
  const item = document.querySelector(`.doc-item[data-id="${{id}}"]`);
  if (item) {{ item.classList.add('active'); item.scrollIntoView({{ block:'nearest' }}); }}
  hideSearch();
  if (!noScroll) window.scrollTo(0, 0);
}}

function jumpTo(docId, slug, linkEl) {{
  if (currentDoc !== docId) {{ showDoc(docId, true); setTimeout(() => _scroll(slug, linkEl), 60); }}
  else _scroll(slug, linkEl);
}}

function _scroll(slug, linkEl) {{
  const t = document.getElementById(slug);
  if (t) window.scrollTo({{ top: t.getBoundingClientRect().top + scrollY - 72, behavior: 'smooth' }});
  if (linkEl) {{
    document.querySelectorAll('.section-link').forEach(l => l.classList.remove('active'));
    linkEl.classList.add('active');
  }}
}}

window.addEventListener('scroll', () => {{
  const active = document.querySelector('.md-content.active');
  if (!active) return;
  let last = null;
  active.querySelectorAll('h2[id]').forEach(h => {{ if (h.getBoundingClientRect().top <= 80) last = h; }});
  if (last) document.querySelectorAll('.section-link').forEach(l =>
    l.classList.toggle('active', l.dataset.slug === last.id && l.dataset.doc === currentDoc)
  );
}}, {{ passive: true }});

function toggleGroup(header) {{
  header.classList.toggle('collapsed');
  const body = header.nextElementSibling;
  if (body) body.style.display = body.style.display === 'none' ? '' : 'none';
}}

const searchInput   = document.getElementById('searchInput');
const searchOverlay = document.getElementById('searchOverlay');
const searchList    = document.getElementById('searchList');
const sqLabel       = document.getElementById('sqLabel');

const searchIndex = [];
docs.forEach(doc => {{
  doc.content.split('\\n').forEach((line, i, lines) => {{
    if (line.trim()) searchIndex.push({{ docId: doc.id, docTitle: doc.title, line, i, lines }});
  }});
}});

function getSection(lines, idx) {{
  for (let i = idx; i >= 0; i--) {{
    const m = lines[i].match(/^## (.+)/);
    if (m) return m[1];
  }}
  return null;
}}

let timer;
searchInput.addEventListener('input', () => {{
  clearTimeout(timer);
  timer = setTimeout(() => runSearch(searchInput.value.trim()), 120);
}});

function runSearch(q) {{
  if (q.length < 2) {{ hideSearch(); return; }}
  const ql = q.toLowerCase();
  const re = new RegExp(q.replace(/[.*+?^${{}}()|[\\]\\\\]/g,'\\\\$&'), 'gi');
  const seen = new Set();
  const results = [];
  searchIndex.forEach(e => {{
    if (!e.line.toLowerCase().includes(ql)) return;
    const section = getSection(e.lines, e.i);
    const key = e.docId + '|' + (section || '');
    if (seen.has(key)) return;
    seen.add(key);
    results.push({{ docId: e.docId, docTitle: e.docTitle, section, snip: e.line.trim().replace(/^#+\\s*/,'').slice(0,100) }});
  }});

  sqLabel.textContent = '"' + q + '"';
  searchList.innerHTML = '';
  if (!results.length) {{
    searchList.innerHTML = '<div class="no-results">No results found.</div>';
  }} else {{
    results.slice(0, 50).forEach(r => {{
      const div = document.createElement('div');
      div.className = 'res-item';
      div.innerHTML =
        `<div class="res-doc">${{r.docTitle}}</div>` +
        `<div class="res-title">${{(r.section||'—').replace(re, m => '<mark>'+m+'</mark>')}}</div>` +
        `<div class="res-snip">${{r.snip.replace(re, m => '<mark>'+m+'</mark>')}}</div>`;
      div.onclick = () => {{
        searchInput.value = '';
        hideSearch();
        showDoc(r.docId, true);
        setTimeout(() => {{
          if (r.section) {{
            const slug = r.section.toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/-+/g,'-').trim();
            _scroll(slug, null);
          }} else window.scrollTo(0, 0);
        }}, 60);
      }};
      searchList.appendChild(div);
    }});
  }}
  searchOverlay.classList.add('visible');
}}

function hideSearch() {{ searchOverlay.classList.remove('visible'); }}

searchInput.addEventListener('keydown', e => {{
  if (e.key === 'Escape') {{ searchInput.value = ''; hideSearch(); }}
}});
document.addEventListener('keydown', e => {{
  if ((e.metaKey || e.ctrlKey) && e.key === 'k') {{ e.preventDefault(); searchInput.focus(); }}
}});

showDoc('{first_id}');
</script>
</body>
</html>"""

# ─── MAIN ─────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    folder = sys.argv[1].rstrip('/\\')
    if not os.path.isdir(folder):
        print(f"Erro: '{folder}' não é uma pasta válida.")
        sys.exit(1)

    output = sys.argv[2] if len(sys.argv) >= 3 else os.path.join(folder, 'docs.html')

    folder_name = os.path.basename(os.path.abspath(folder))

    print(f"📂  A ler markdown de: {folder}")
    docs = load_docs(folder)
    if not docs:
        print("Erro: nenhum ficheiro .md encontrado.")
        sys.exit(1)

    print(f"📄  {len(docs)} ficheiros encontrados:")
    for d in docs:
        print(f"    [{d['group']:10s}]  {d['id']}")

    print(f"\n⚙️   A gerar HTML…")
    html = build_html(docs, folder_name)

    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, 'w', encoding='utf-8') as f:
        f.write(html)

    size_kb = os.path.getsize(output) // 1024
    print(f"✅  Gerado: {output}  ({size_kb} KB)")

if __name__ == '__main__':
    main()
