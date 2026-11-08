import sys, time, subprocess, os
from playwright.sync_api import sync_playwright
root = sys.argv[1]; out = sys.argv[2]; w = int(sys.argv[3]) if len(sys.argv) > 3 else 1280; h = int(sys.argv[4]) if len(sys.argv) > 4 else 720
script = sys.argv[5] if len(sys.argv) > 5 else ''
srv = subprocess.Popen(['python3', '-m', 'http.server', '8123', '--directory', root], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1)
try:
    with sync_playwright() as p:
        b = p.chromium.launch(args=['--use-angle=swiftshader', '--use-gl=angle', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'])
        pg = b.new_page(viewport={'width': w, 'height': h})
        logs = []
        pg.on('console', lambda m: logs.append(m.type + ': ' + m.text))
        pg.on('pageerror', lambda e: logs.append('PAGEERROR: ' + str(e)))
        pg.goto('http://localhost:8123/demo/index.html')
        try:
            pg.wait_for_function('window.engine && window.engine.time > 1.5', timeout=60000)
        except Exception as e:
            print('TIMEOUT'); [print(l) for l in logs[:20]]; pg.screenshot(path=out); b.close(); raise SystemExit(1)
        if script: pg.evaluate(script)
        time.sleep(2.5)
        pg.screenshot(path=out)
        print(pg.evaluate('({fps: engine.fps, draws: engine.renderer.stats.draws, tris: engine.renderer.stats.tris})'))
        for l in logs[:15]: print(l)
        b.close()
finally:
    srv.terminate()
