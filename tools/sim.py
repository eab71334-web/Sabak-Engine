"""Sabak simulator: drive the engine headlessly (Chromium + software GL).
Usage: python3 tools/sim.py <page.html> '<json commands>' [outdir] [width height]
Example: python3 tools/sim.py demo/index.html '[["pause"],["press","KeyW"],["step",90],["shot"],["state"]]' /tmp/out
Every ["shot"] command is saved to outdir/shot_N.png; other results are printed as JSON."""
import sys, json, time, base64, subprocess, os
from playwright.sync_api import sync_playwright
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
page = sys.argv[1]; cmds = json.loads(sys.argv[2]); out = sys.argv[3] if len(sys.argv) > 3 else '/tmp/sim'
w = int(sys.argv[4]) if len(sys.argv) > 4 else 844; h = int(sys.argv[5]) if len(sys.argv) > 5 else 390
os.makedirs(out, exist_ok=True)
srv = subprocess.Popen(['python3', '-m', 'http.server', '8124', '--directory', root], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1)
try:
    with sync_playwright() as p:
        b = p.chromium.launch(args=['--use-angle=swiftshader', '--use-gl=angle', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'])
        pg = b.new_page(viewport={'width': w, 'height': h})
        logs = []
        pg.on('console', lambda m: logs.append(m.type + ': ' + m.text)); pg.on('pageerror', lambda e: logs.append('PAGEERROR: ' + str(e)))
        pg.goto('http://localhost:8124/' + page)
        pg.wait_for_function('window.sabak && window.engine && window.engine.time > 1', timeout=90000)
        n = 0
        for c in cmds:
            r = pg.evaluate('(c) => window.sabak.run([c])[0]', c)
            if c[0] == 'shot' and 'result' in r:
                f = os.path.join(out, f'shot_{n}.png'); open(f, 'wb').write(base64.b64decode(r['result'].split(',')[1])); n += 1; r['result'] = f
            print(json.dumps(r))
        for l in logs[:10]: print(l)
        b.close()
finally:
    srv.terminate()
