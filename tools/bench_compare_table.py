#!/usr/bin/env python3
# Turns the raw outputs of tools/bench_compare.sh into the tables of docs/COMPARACAO.md (markdown on stdout).
#   usage: bench_compare_table.py <raw dir> [<raw dir>...]   (several: only the start-up times pool the runs of all of them, the first has the rest)
import glob
import os
import re
import statistics
import sys

raws = sys.argv[1:]
raw = raws[0]
LIBS = ['igui-soft', 'igui-gl', 'imgui', 'fltk', 'gtk']
NAMES = {'igui-soft': 'iGUI soft', 'igui-gl': 'iGUI GL', 'imgui': 'Dear ImGui', 'fltk': 'FLTK', 'gtk': 'GTK 3'}
SCENES = 'ABCDEF'
SCENE_NAMES = {'A': 'A empty window', 'B': 'B form, 200 widgets', 'C': 'C list, 100000 rows', 'D': 'D tree, 10000 nodes',
               'E': 'E text editor, 5 MB', 'F': 'F 50 windows/panels'}


def kv(path):
    d = {}
    try:
        for line in open(path):
            m = re.match(r'^([A-Za-z0-9_]+)=(.+)$', line.strip())
            if m:
                try:
                    d[m.group(1)] = float(m.group(2))
                except ValueError:
                    d[m.group(1)] = m.group(2)
    except OSError:
        pass
    return d


def runs(lib, mode, scene, dirs=None):
    return [kv(p) for r in (dirs or raws[:1]) for p in sorted(glob.glob('%s/%s/%s.%s.[0-9]*.txt' % (r, lib, mode, scene)))]


def med(lib, mode, scene, key):
    vals = [r[key] for r in runs(lib, mode, scene, raws if key.startswith('startup_') else None) if key in r and isinstance(r[key], float)]
    if not vals:
        return None
    # a start-up is one event per run and a loaded machine only ever makes it longer: the best run; everything else is a median
    return min(vals) if key.startswith('startup_') else statistics.median(vals)


def icount(lib, scene):
    """{phase: instructions}: the REGION/region lines, the second sample of a phase when it has two"""
    names, counts = {}, {}
    try:
        for line in open(next(('%s/%s/icount.%s.txt' % (r, lib, scene) for r in raws if os.path.exists('%s/%s/icount.%s.txt' % (r, lib, scene))), '/nonexistent')):
            m = re.match(r'REGION (\d+) (\S+)', line)
            if m:
                names[int(m.group(1))] = m.group(2)
            m = re.match(r'region (\d+): (\d+)', line)
            if m:
                counts[int(m.group(1))] = int(m.group(2))
    except OSError:
        pass
    out = {}
    for k, n in counts.items():
        out[names.get(k, str(k))] = n
    return out


def fmt(v, digits=1, thousands=False):
    if v is None:
        return 'n/a'
    if thousands:
        return '{:,.0f}'.format(v).replace(',', ' ')
    return ('{:.%df}' % digits).format(v)


def table(title, rowkeys, rowname, cell, note=None, libs=None):
    libs = libs or [l for l in LIBS if any(os.path.isdir('%s/%s' % (r, l)) for r in raws)]
    print('\n**%s**\n' % title)
    print('| %s | %s |' % (rowname, ' | '.join(NAMES[l] for l in libs)))
    print('|---|%s|' % '|'.join(['---:'] * len(libs)))
    for r in rowkeys:
        cells = [cell(l, r) for l in libs]
        if all(c == 'n/a' for c in cells):
            continue
        print('| %s | %s |' % (SCENE_NAMES.get(r, r), ' | '.join(cells)))
    if note:
        print('\n' + note)


def scene_table(title, key, digits=1, scale=1.0, mode='frames', thousands=False, note=None, scenes=SCENES):
    table(title, scenes, 'scene', lambda l, s: fmt(None if med(l, mode, s, key) is None else med(l, mode, s, key) * scale, digits, thousands), note)


print('# Tables\n')
print('Medians of the runs of each cell (start-up: the best run; instruction counts: one run, they do not vary). n/a: the library has no such measure or the scene does not apply.')

print('\n## 1. Startup to the first frame presented')
scene_table('Wall time, ms (from main() to the end of the first presented frame)', 'startup_wall_ms', 0)
scene_table('CPU time of the process, ms (all threads)', 'startup_cpu_ms', 0)

print('\n## 2. Memory, idle after the first frame')
scene_table('PSS, MB (/proc/self/smaps_rollup)', 'pss_kb', 1, 1 / 1024.0)
scene_table('RSS, MB', 'rss_kb', 1, 1 / 1024.0)
scene_table('Heap in use, MB (malloc: every allocation of the process, the GL driver included)', 'heap_live_bytes', 2, 1 / 1048576.0)
scene_table('Threads', 'threads', 0)


def per_item(l, s, base_scene, count):
    a = med(l, 'frames', base_scene, 'heap_live_bytes')
    b = med(l, 'frames', s, 'heap_live_bytes')
    return fmt(None if a is None or b is None else (b - a) / count, 0)


def per_item_pss(l, s, count):
    a = med(l, 'frames', 'A', 'pss_kb')
    b = med(l, 'frames', s, 'pss_kb')
    return fmt(None if a is None or b is None else (b - a) * 1024.0 / count, 0)


print('\n**Bytes per item: the heap of the scene minus the heap of the empty window, over the number of items (PSS in the second line of each library is the same difference in PSS)**\n')
libs_present = [l for l in LIBS if any(os.path.isdir('%s/%s' % (r, l)) for r in raws)]
print('| item | %s |' % ' | '.join(NAMES[l] for l in libs_present))
print('|---|%s|' % '|'.join(['---:'] * len(libs_present)))
for s, label, n in (('B', 'widget (B, 200)', 200), ('C', 'row (C, 100000)', 100000), ('D', 'tree node (D, 10100)', 10100),
                    ('E', 'text line (E, 91000, 55 bytes each)', 91000), ('F', 'window or panel (F, 50)', 50)):
    print('| %s, heap | %s |' % (label, ' | '.join(per_item(l, s, 'A', n) for l in libs_present)))
    print('| %s, PSS | %s |' % (label, ' | '.join(per_item_pss(l, s, n) for l in libs_present)))

print('\n## 3. Building the scene (heap calls, bytes and time)')
scene_table('Heap calls to build the scene, thousands', 'build_allocs', 1, 0.001)
scene_table('Heap bytes allocated while building, MB', 'build_bytes', 2, 1 / 1048576.0)
scene_table('Time to build the scene, ms', 'build_ms', 1)

print('\n## 4. One frame')
PHASES = [('idle', 'nothing changed'), ('hover', 'the mouse moving over the widgets'), ('scroll', 'the mouse wheel'), ('click', 'a click'), ('typing', 'a key typed')]
for ph, what in PHASES:
    ss = ''.join(s for s in SCENES if any(med(l, 'frames', s, ph + '_gui_us') is not None for l in libs_present))
    if not ss:
        continue
    ic = lambda l, s, ph=ph: icount(l, s).get(ph)
    table('%s: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)' % ph, ss, 'scene',
          lambda l, s, ic=ic: fmt(ic(l, s), 0, True))
    scene_table('%s: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)' % ph, ph + '_gui_us', 0, 1.0, scenes=ss)
    scene_table('%s: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)' % ph, ph + '_total_us', 0, 1.0, scenes=ss)
    scene_table('%s: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)' % ph, ph + '_proc_us', 0, 1.0, scenes=ss)
    scene_table('%s: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)' % ph, ph + '_xsrv_us', 0, 1.0, scenes=ss)
    scene_table('%s: heap calls per frame in the GUI side' % ph, ph + '_gui_allocs', 0, 1.0, scenes=ss)
    if ph == 'idle':
        scene_table('Draw calls of the frame (what the library hands the backend)', 'idle_draw_calls', 0)
        scene_table('Vertices of the frame', 'idle_vertices', 0, thousands=True)

print('\n## 5. Scene D: expand and collapse all (100 groups of 99 nodes)')
scene_table('Expand all: the call, microseconds', 'expand_all_call_us', 0, scenes='D')
scene_table('Expand all: the next frame, GUI side, microseconds', 'expand_all_frame_gui_us', 0, scenes='D')
scene_table('Collapse all: the call, microseconds', 'collapse_all_call_us', 0, scenes='D')
scene_table('Collapse all: the next frame, GUI side, microseconds', 'collapse_all_frame_gui_us', 0, scenes='D')
ic_rows = []
table('Expand all and collapse all: instructions of the call', ['expand_all_call', 'collapse_all_call'], 'phase', lambda l, s: fmt(icount(l, 'D').get(s), 0, True))

print('\n## 6. Idle behaviour (scene B, the library\'s own main loop, 10 s, nothing happens; vsync on)')
table('Idle', ['idle_cpu_pct', 'idle_wakeups_per_s', 'idle_frames_per_s'], 'measure',
      lambda l, k: fmt(med(l, 'idlecpu', 'B', k), 1))

print('\n## 7. Input latency (scene B, an injected click on a checkbox until the changed widget is drawn and presented)')
table('Latency', ['latency_frames_med', 'latency_ms_med', 'latency_ms_min'], 'measure', lambda l, k: fmt(med(l, 'latency', 'B', k), 1))

print('\n## 8. Binary and library sizes (stripped)')
sizes = {}
try:
    for line in open(next((r + '/sizes.txt' for r in raws if os.path.exists(r + '/sizes.txt')), '/nonexistent')):
        p = line.split()
        if len(p) == 4:
            sizes[p[0]] = (int(p[1]), int(p[2]), int(p[3]))
except OSError:
    pass
print('\n| file | bytes | shared libraries linked | bytes of those beyond libc, libm, libstdc++ |\n|---|---:|---:|---:|')
for k, (b, n, e) in sorted(sizes.items()):
    print('| %s | %s | %s | %s |' % (k, fmt(b, 0, True), n if n else '', fmt(e, 0, True) if e else ''))
