#!/usr/bin/env python3
"""Frontend local: python3 server.py  ->  http://127.0.0.1:3000
POST /api/run   ?fonte=demo | ?fonte=serial&porta=/dev/ttyACM0
POST /api/chat  {"pergunta", "resumo"}   (teste na tela + historico)
GET  /api/historico[/id] · POST /api/historico/analise   (ultimos 10 testes em dados/historico.json)
"""
import datetime, glob, json, os, random, subprocess, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse
import analisar_ia as ia

DIR = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(DIR, "ecotech_sensor")
AMOSTRAS = 24
TOTAL = AMOSTRAS * len(ia.NOMES)
PORTA = (sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")) or ["/dev/ttyACM0"])[0]  # porta do Arduino detectada
DADOS = os.environ.get("DADOS", os.path.join(DIR, "dados"))
ARQ = os.path.join(DADOS, "historico.json")
MAX_HIST = 10
trava = threading.Lock()
trava_serial = threading.Lock()  # so uma leitura por vez na porta

def carregar():
    try: return json.load(open(ARQ, encoding="utf-8"))
    except (OSError, ValueError): return []

def salvar(fonte, r):
    with trava:
        h = carregar()
        h.append({"id": int(datetime.datetime.now().timestamp() * 1000), "fonte": fonte, "resultado": r,
                  "ts": datetime.datetime.now().strftime("%d/%m %H:%M:%S")})
        os.makedirs(DADOS, exist_ok=True)
        tmp = ARQ + ".tmp"
        json.dump(h[-MAX_HIST:], open(tmp, "w", encoding="utf-8"), ensure_ascii=False)
        os.replace(tmp, ARQ)  # troca atomica: nunca deixa o historico pela metade

def serie(tipo, base, passo, ruido, lo, hi):
    n = AMOSTRAS
    if tipo == "estavel": return [base + random.uniform(-ruido, ruido) for _ in range(n)]
    if tipo == "tendencia": return [base + i * passo + random.uniform(-ruido, ruido) for i in range(n)]
    if tipo == "travado": return [base + i * 0.2 for i in range(6)] + [base + 1] * (n - 6)
    return [random.choice([lo, hi]) + random.uniform(-ruido, ruido) for _ in range(n)]  # ruidoso

def demo():
    """24 linhas ppm,solo,temp; cada canal sorteia um comportamento."""
    sorteio = lambda: random.choices(["estavel", "tendencia", "travado", "ruidoso"], [40, 35, 12, 13])[0]
    linhas = []
    for _ in ia.NOMES:
        co2 = serie(sorteio(), random.randint(500, 900), random.randint(12, 25), 25, 420, 1500)
        solo = serie(sorteio(), random.randint(38, 60), -random.uniform(.6, 1.2), 1, 5, 90)
        temp = serie(sorteio(), random.randint(22, 30), random.uniform(.15, .35), .3, 15, 40)
        linhas += [(round(a), round(min(max(b, 0), 100), 1), round(c)) for a, b, c in zip(co2, solo, temp)]
    return linhas

def serial(porta, prazo=300):
    subprocess.run(["stty", "-F", porta, "9600", "raw", "-echo"], check=True)
    p = subprocess.Popen(["cat", porta], stdout=subprocess.PIPE, text=True)
    timer = threading.Timer(prazo, p.kill)
    timer.start()
    texto = ""
    try:
        for l in p.stdout:
            texto += l
            if len(ia.parse(texto)) >= TOTAL: break
    finally:
        timer.cancel(); p.kill()
    rows = ia.parse(texto)[:TOTAL]
    if len(rows) < TOTAL: raise RuntimeError(f"Serial devolveu {len(rows)} de {TOTAL} leituras (tempo esgotado?)")
    return rows

def rodar(fonte, porta):
    if fonte == "serial":
        if not trava_serial.acquire(blocking=False): raise RuntimeError("Ja existe uma leitura serial em andamento")
        try: rows = serial(porta)
        finally: trava_serial.release()
    else:
        rows = demo()
    # o programa em C (entrega da disciplina) continua sendo o painel oficial de CO2
    c = subprocess.run([BIN], input="".join(f"{int(r[0])}\n" for r in rows), capture_output=True, text=True, timeout=30)
    r = ia.processar(rows, c.stdout.strip())
    salvar(fonte, r)
    return r

class H(BaseHTTPRequestHandler):
    def responder(self, code, tipo, corpo):
        self.send_response(code); self.send_header("content-type", tipo); self.end_headers(); self.wfile.write(corpo)

    def json(self, code, obj): self.responder(code, "application/json", json.dumps(obj).encode())

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            html = open(os.path.join(DIR, "index.html"), encoding="utf-8").read().replace("/dev/ttyACM0", PORTA)
            return self.responder(200, "text/html; charset=utf-8", html.encode())
        if u.path == "/api/historico":
            h = carregar()
            itens = [{"id": e["id"], "ts": e["ts"], "fonte": e["fonte"],
                      "risco": max(s["risco"] for s in e["resultado"]["setores"])} for e in h]
            return self.json(200, {"itens": itens, "local": ia.historico_local(h), "canais": ia.CANAIS})
        if u.path.startswith("/api/historico/"):
            for e in carregar():
                if str(e["id"]) == u.path.rsplit("/", 1)[1]: return self.json(200, e["resultado"])
            return self.json(404, {"erro": "teste nao encontrado"})
        self.responder(404, "text/plain", b"nao encontrado")

    def do_POST(self):
        u = urlparse(self.path)
        try: corpo = json.loads(self.rfile.read(int(self.headers.get("content-length", 0))) or b"{}")
        except ValueError: return self.json(400, {"erro": "JSON invalido"})
        if u.path == "/api/run":
            q = parse_qs(u.query)
            try: return self.json(200, rodar(q.get("fonte", ["demo"])[0], q.get("porta", [PORTA])[0]))
            except Exception as e: return self.json(500, {"erro": str(e)})
        if u.path == "/api/chat":
            if not isinstance(corpo, dict) or not corpo.get("pergunta"): return self.json(400, {"erro": "campo 'pergunta' obrigatorio"})
            txt, erro = ia.chat(corpo["pergunta"], corpo.get("resumo", ""), ia.resumo_historico(carregar()))
            return self.json(200 if txt else 500, {"resposta": txt, "erro": erro})
        if u.path == "/api/historico/analise":
            h = carregar()
            if len(h) < 2: return self.json(400, {"erro": "Preciso de pelo menos 2 testes no historico."})
            txt, erro = ia.laudo_historico(h)
            return self.json(200 if txt else 500, {"analise": txt, "erro": erro})
        self.responder(404, "text/plain", b"nao encontrado")

    def log_message(self, *a): pass

def main():
    subprocess.run(["gcc", "-Wall", "-Wextra", "-std=c11", "-o", BIN, os.path.join(DIR, "ecotech_co2_sensor.c")], check=True)
    porta = int(os.environ.get("PORT", 3000))
    print(f"http://127.0.0.1:{porta}")
    ThreadingHTTPServer((os.environ.get("HOST", "127.0.0.1"), porta), H).serve_forever()  # so localhost: a chave OpenAI nunca sai da maquina

if __name__ == "__main__":
    main()
