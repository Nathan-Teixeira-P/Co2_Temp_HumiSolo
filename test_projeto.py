import json, os, pty, shutil, socket, subprocess, sys, threading, time, urllib.error, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import pytest
import analisar_ia as ia

DIR = os.path.dirname(os.path.abspath(__file__))

def rows(co2, solo, temp, n=24):
    return [(co2(i), solo(i), temp(i)) for i in range(n)]

def test_parse_ignora_ruido_serial():
    assert ia.parse("ppm,solo,temp\n800,45.5,25\nlixo\n1,2\n900,40,26\n") == [(800, 45.5, 25), (900, 40, 26)]

def test_setor_saudavel_tem_risco_zero():
    s = ia.setor("A", rows(lambda i: 600 + i % 3, lambda i: 50 + (i % 2), lambda i: 25 + i % 2))
    assert s["risco"] == 0 and s["regras"] == []

def test_setor_critico_aciona_regras():
    s = ia.setor("A", rows(lambda i: 1500 + i, lambda i: 10 + i % 2, lambda i: 35 + i % 2))
    assert s["risco"] == 100
    assert {"Ventilar / acionar exaustao", "Irrigar", "Sombrear / resfriar"} <= set(s["regras"])

def test_previsao_de_cruzamento_do_limite():
    c = ia.canal([float(700 + 20 * i) for i in range(24)], ia.CANAIS["co2"])
    assert c["tendencia"] == pytest.approx(600) and "ja alem" in c["previsao"]
    c = ia.canal([float(500 + 5 * i) for i in range(24)], ia.CANAIS["co2"])
    assert "cruza 1000 ppm em" in c["previsao"]

def test_sensor_travado_e_salto_sao_suspeitos():
    travado = ia.canal([700.0] * 24, ia.CANAIS["co2"])
    assert any("identicas" in s for s in travado["suspeito"])
    salto = ia.canal([600.0] * 12 + [1500.0] + [600.0] * 11, ia.CANAIS["co2"])
    assert any("saltos" in s for s in salto["suspeito"])

def test_temperatura_repetida_nao_e_suspeita():
    assert ia.canal([25.0] * 24, ia.CANAIS["temp"])["suspeito"] == []

def test_correlacao_de_serie_constante_e_omitida():
    s = ia.setor("A", rows(lambda i: 600, lambda i: 50, lambda i: 25))
    assert s["correlacoes"] == {}

def test_processar_rejeita_poucas_leituras():
    with pytest.raises(ValueError):
        ia.processar([(600, 50, 25)] * 2)

def entrada(id, co2, ts="01/01 00:00:00"):
    s = ia.setor("A", rows(lambda i: co2 + i % 3, lambda i: 50 - i % 2, lambda i: 25 + i % 2))
    return {"id": id, "fonte": "demo", "ts": ts, "resultado": {"setores": [s]}}

def test_historico_detecta_alta_de_co2():
    h = ia.historico_local([entrada(i, 600 + 200 * i) for i in range(4)])
    assert h["n"] == 4 and h["canais"]["co2"]["tendencia"] > 0
    assert h["canais"]["co2"]["suspeito_em"] == 0
    assert any("CO2" in i for i in h["insights"])
    assert "#4" in ia.resumo_historico([entrada(i, 600) for i in range(4)])

def test_historico_vazio():
    assert ia.historico_local([])["n"] == 0

class OpenAIFalsa:
    def __init__(self):
        self.status, self.conteudo, self.pedidos = 200, "", []
        outer = self
        class H(BaseHTTPRequestHandler):
            def do_POST(self):
                outer.pedidos.append(json.loads(self.rfile.read(int(self.headers["content-length"]))))
                corpo = json.dumps({"choices": [{"message": {"content": outer.conteudo}}]} if outer.status == 200 else "negado")
                self.send_response(outer.status); self.end_headers(); self.wfile.write(corpo.encode())
            def log_message(self, *a): pass
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.url = f"http://127.0.0.1:{self.httpd.server_port}/v1/chat/completions"

@pytest.fixture
def openai():
    f = OpenAIFalsa()
    yield f
    f.httpd.shutdown(); f.httpd.server_close()

@pytest.fixture
def historico(tmp_path):
    arq = tmp_path / "historico.json"
    arq.write_text(json.dumps([entrada(1, 600), entrada(2, 1400)]))
    return arq

@pytest.fixture
def server(historico, openai):
    if not shutil.which("gcc"): pytest.skip("gcc ausente")
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); porta = s.getsockname()[1]
    url = f"http://127.0.0.1:{porta}"
    env = {**os.environ, "DADOS": str(historico.parent), "PORT": str(porta), "OPENAI_URL": openai.url, "OPENAI_API_KEY": "teste"}
    p = subprocess.Popen([sys.executable, "server.py"], cwd=DIR, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(100):
        try:
            urllib.request.urlopen(url + "/", timeout=1)
            break
        except OSError:
            if p.poll() is not None: pytest.fail(p.stderr.read().decode())
            time.sleep(.1)
    yield url
    p.kill(); p.stderr.close()

def get(url):
    try:
        with urllib.request.urlopen(url) as r: return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()

def post(url, obj):
    req = urllib.request.Request(url, json.dumps(obj).encode(), {"content-type": "application/json"})
    try:
        with urllib.request.urlopen(req) as r: return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())

def itens(server): return json.loads(get(server + "/api/historico")[1])["itens"]

def test_pagina_inicial(server):
    st, body = get(server + "/")
    assert st == 200 and b"<html" in body.lower()

def test_lista_historico(server):
    assert [i["id"] for i in itens(server)] == [1, 2]
    assert itens(server)[1]["risco"] > itens(server)[0]["risco"]

def test_detalhe_historico_e_404(server):
    st, body = get(server + "/api/historico/2")
    assert st == 200 and json.loads(body)["setores"][0]["nome"] == "A"
    assert get(server + "/api/historico/999")[0] == 404
    assert get(server + "/nada")[0] == 404

def test_post_invalido_devolve_400(server):
    req = urllib.request.Request(server + "/api/chat", b"{nao e json", {"content-type": "application/json"})
    with pytest.raises(urllib.error.HTTPError) as e: urllib.request.urlopen(req)
    assert e.value.code == 400
    assert post(server + "/api/chat", {})[0] == 400

def test_run_so_aceita_post(server):
    assert get(server + "/api/run?fonte=demo")[0] == 404

def test_run_demo_usa_laudo_da_ia_e_grava_historico(server, openai):
    openai.conteudo = '{"resumo_geral": "tudo ok"}'
    st, r = post(server + "/api/run?fonte=demo", {})
    assert st == 200 and r["setores"][0]["canais"]["co2"]["valores"] and r["painel"]
    assert r["laudo"] == {"resumo_geral": "tudo ok"} and r["erro_laudo"] is None
    assert openai.pedidos[0]["response_format"] == {"type": "json_object"}
    assert [i["id"] for i in itens(server)][:2] == [1, 2] and len(itens(server)) == 3

def test_run_com_json_invalido_ou_erro_http_da_ia(server, openai):
    openai.conteudo = "nao json"
    assert post(server + "/api/run?fonte=demo", {})[1]["erro_laudo"] == "IA devolveu JSON invalido"
    openai.status = 401
    assert post(server + "/api/run?fonte=demo", {})[1]["erro_laudo"].startswith("Erro 401")

def test_historico_mantem_so_os_ultimos_10(server):
    for _ in range(12): post(server + "/api/run?fonte=demo", {})
    assert len(itens(server)) == 10

def test_historico_corrompido_vira_vazio(server, historico):
    historico.write_text("{quebrado")
    assert itens(server) == []

def test_run_serial_le_leituras_da_porta(server):
    mestre, escravo = pty.openpty()
    os.write(mestre, b"ppm,solo,temp\nlixo\n" + b"".join(f"{600 + i},50,25\n".encode() for i in range(30)))
    st, r = post(server + "/api/run?fonte=serial&porta=" + os.ttyname(escravo), {})
    os.close(mestre); os.close(escravo)
    assert st == 200 and r["setores"][0]["canais"]["co2"]["valores"][0] == 600

def test_run_serial_inexistente_devolve_500(server):
    st, r = post(server + "/api/run?fonte=serial&porta=/dev/inexistente", {})
    assert st == 500 and r["erro"]

def test_chat_envia_pergunta_e_historico_para_a_ia(server, openai):
    openai.conteudo = "regue"
    assert post(server + "/api/chat", {"pergunta": "e agora?", "resumo": "R"}) == (200, {"resposta": "regue", "erro": None})
    sistema, usuario = openai.pedidos[0]["messages"]
    assert "#2" in sistema["content"] and usuario["content"] == "e agora?"

def test_chat_propaga_erro_da_ia(server, openai):
    openai.status = 401
    st, r = post(server + "/api/chat", {"pergunta": "oi"})
    assert st == 500 and r["erro"].startswith("Erro 401")

def test_analise_historico(server, openai):
    openai.conteudo = '{"resumo": "ok"}'
    assert post(server + "/api/historico/analise", {}) == (200, {"analise": {"resumo": "ok"}, "erro": None})

def test_analise_exige_dois_testes(server, historico):
    historico.write_text(json.dumps([entrada(1, 600)]))
    assert post(server + "/api/historico/analise", {})[0] == 400
