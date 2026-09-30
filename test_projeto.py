"""pytest -q  — sem mocks: logica pura direto, server.py como processo real (sem chamar a OpenAI)."""
import json, os, subprocess, sys, time, urllib.error, urllib.request
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
    # sobe 20 ppm por amostra de 2 s => 600/min; termina em 1160, alem do limite
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
    assert h["canais"]["co2"]["alem_limite_em"] >= 1
    assert any("CO2" in i for i in h["insights"])
    assert "#4" in ia.resumo_historico([entrada(i, 600) for i in range(4)])


def test_historico_vazio():
    assert ia.historico_local([])["n"] == 0


# ---- server.py de verdade (HTTP real, DADOS em pasta temporaria) ----
@pytest.fixture(scope="module")
def server(tmp_path_factory):
    dados = tmp_path_factory.mktemp("dados")
    (dados / "historico.json").write_text(json.dumps([entrada(1, 600), entrada(2, 1400)]))
    p = subprocess.Popen([sys.executable, "server.py"], cwd=DIR, env={**os.environ, "DADOS": str(dados)},
                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(100):
        try:
            urllib.request.urlopen("http://127.0.0.1:3000/", timeout=1)
            break
        except OSError:
            if p.poll() is not None:
                pytest.fail(p.stderr.read().decode())
            time.sleep(.1)
    yield "http://127.0.0.1:3000"
    p.kill()


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


def test_pagina_inicial(server):
    st, body = get(server + "/")
    assert st == 200 and b"<html" in body.lower()


def test_lista_historico(server):
    st, body = get(server + "/api/historico")
    d = json.loads(body)
    assert st == 200 and [i["id"] for i in d["itens"]] == [1, 2]
    assert d["itens"][1]["risco"] > d["itens"][0]["risco"]


def test_detalhe_historico_e_404(server):
    st, body = get(server + "/api/historico/2")
    assert st == 200 and json.loads(body)["setores"][0]["nome"] == "A"
    assert get(server + "/api/historico/999")[0] == 404
    assert get(server + "/nada")[0] == 404
