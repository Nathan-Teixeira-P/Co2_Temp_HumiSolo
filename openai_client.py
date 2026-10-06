import json, os, urllib.error, urllib.request

ENV = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
for linha in open(ENV) if os.path.exists(ENV) else []:
    if "=" in linha and not linha.startswith("#"):
        k, v = linha.strip().split("=", 1)
        os.environ.setdefault(k, v)

def openai(messages, json_mode=False):
    corpo = {"model": os.environ.get("OPENAI_MODEL", "gpt-4o-mini"), "messages": messages}
    if json_mode: corpo["response_format"] = {"type": "json_object"}
    try:
        req = urllib.request.Request(
            os.environ.get("OPENAI_URL", "https://api.openai.com/v1/chat/completions"), data=json.dumps(corpo).encode(),
            headers={"authorization": "Bearer " + os.environ["OPENAI_API_KEY"], "content-type": "application/json"})
        return json.load(urllib.request.urlopen(req, timeout=60))["choices"][0]["message"]["content"], None
    except urllib.error.HTTPError as e:
        return None, f"Erro {e.code}: {e.read().decode()}"
    except KeyError:
        return None, "OPENAI_API_KEY nao configurada"
    except Exception as e:
        return None, str(e)
