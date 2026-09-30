# Estufa do tio Valdir — Roteiro de apresentação (8 partes)

**Ideia em uma frase:** um Arduino lê CO₂, umidade do solo e temperatura; um servidor Python analisa os dados (previsão, saúde do sensor, risco, regras) e uma IA explica e prioriza; tudo aparece num painel web, e o programa em C da disciplina continua sendo o "painel oficial" de CO₂.

## Fluxo ponta a ponta (todos devem saber)

```
Sensores ──► Arduino ──serial 9600──► server.py ──► ecotech_sensor (C)  ─► painel de CO₂
 MH-Z19B      (1 linha           │                                     
 + solo        "ppm,solo,temp"    ├──► analisar_ia.py: estatística + risco + regras
 capacitivo    a cada 2 s)        ├──► OpenAI: laudo (JSON) e chat
                                  ├──► dados/historico.json (últimos 10 testes)
                                  └──► index.html (navegador, http://127.0.0.1:3000)
```

Sem hardware existe o modo **Demonstração**: o `server.py` gera 24 leituras simuladas no mesmo formato.

| Parte | Tema | Arquivos |
|---|---|---|
| 1 | Visão geral, requisitos e infraestrutura | `run.sh`, `Dockerfile`, `docker-compose*.yml`, `.gitignore` |
| 2 | Hardware e firmware Arduino | `arduino/*` |
| 3 | Programa em C da entrega (matriz 2D) | `ecotech_co2.c`, `co2.md` |
| 4 | Programa em C com sensor real | `ecotech_co2_sensor.c` |
| 5 | Análise local (o "cérebro" sem IA) | `analisar_ia.py` (`canal`, `setor`) |
| 6 | Camada de IA e histórico analítico | `analisar_ia.py` (`openai`, `laudo`, `chat`, `historico_local`) |
| 7 | Servidor e API | `server.py` |
| 8 | Painel web, testes e qualidade | `index.html`, `test_projeto.py` |

---

## Parte 1 — Visão geral e infraestrutura

**O que dizer**
- Problema: estufa precisa monitorar CO₂ (ar), umidade do solo (irrigação) e temperatura (estresse térmico) e decidir ações.
- Origem: começou como exercício da disciplina (vetor 1D de 5 leituras em `co2.md`, do professor Valdir Prado), evoluiu para matriz 2D (`ecotech_co2.c`), depois para hardware real + IA + web.
- Só usa **biblioteca padrão** do Python (sem `pip install`) e `gcc`. A única dependência externa é a API da OpenAI.
- **Como rodar:** `./run.sh` (confere gcc, Python ≥ 3.10 e `OPENAI_API_KEY` no `.env`, detecta a porta serial, abre o navegador) ou `docker compose up --build`. Com sensor: `docker compose -f docker-compose.yml -f docker-compose.serial.yml up --build` (mapeia o dispositivo `/dev/ttyACM0`).
- **Segurança:** o servidor escuta só em `127.0.0.1` (a chave OpenAI nunca sai da máquina); no Docker a porta é publicada como `127.0.0.1:3000:3000`; a chave vem do `.env`, que está no `.gitignore` e no `.dockerignore` (não entra na imagem).
- `dados/` é volume: o histórico sobrevive a reinícios.

**Perguntas prováveis:** *Por que não usar framework (Flask, React)?* Para o tamanho do projeto a stdlib basta e não há instalação. *Por que compilar o C dentro do servidor?* Para o painel oficial sempre refletir o código-fonte atual.

---

## Parte 2 — Hardware e firmware Arduino

**Sensores**
- **MH-Z19B** (CO₂ por infravermelho NDIR, 400–5000 ppm), ligado por UART: TX→pino 10, RX→pino 11 usando `SoftwareSerial` (a serial de hardware do Uno já é usada pelo cabo USB).
- **Umidade do solo capacitivo** em `A0`.
- **Temperatura:** vem do próprio MH-Z19B (byte 4 da resposta, valor − 40). É a temperatura do módulo, resolução de 1 °C e erro de alguns graus. Para precisão, trocar por DS18B20/DHT22.

**Dois sketches**
- `mhz19_co2.ino` (original): imprime **só o ppm**, um por linha. Usado com `ecotech_co2_sensor.c` sozinho.
- `ecotech_multi.ino` (atual): imprime `ppm,solo_pct,temp_c`, ex.: `812,47.3,26`. É o que o `server.py` lê.

**Como a leitura funciona (protocolo)**
1. Limpa o buffer, envia o comando de 9 bytes `FF 01 86 00 00 00 00 00 79`.
2. Espera até 1 s por 9 bytes de resposta.
3. Valida: cabeçalho `FF 86` e **checksum** (`0xFF − soma(bytes 1..7) + 1`). Se algo falha, **não imprime nada** (nunca inventa valor).
4. ppm = `byte2 × 256 + byte3`.

**Solo:** `solo% = 100 × (SOLO_SECO − leitura) / (SOLO_SECO − SOLO_MOLHADO)`, limitado a 0–100. Constantes atuais: seco = 620 (ar), molhado = 310 (água). **Precisam ser calibradas para cada sensor/solo.** `TEMP_OFFSET` corrige contra um termômetro.

**Ciclo:** `setup` espera 10 s de aquecimento (o ideal seriam ~3 min); `loop` amostra a cada 2 s.

**Perguntas prováveis:** *Por que o valor de temperatura é pouco confiável?* É do módulo, não do ambiente. *E se o sensor travar?* O checksum descarta pacote ruim; travamento lógico é detectado na Parte 5.

---

## Parte 3 — Programa em C da entrega (`ecotech_co2.c`)

É a **entrega do LXP** (Atividade Prática 5, Aula 07 — vetores bidimensionais). Falta preencher o **nome do aluno** no cabeçalho antes de enviar.

**Conceitos cobrados (as "diretrizes")**
1. **Memória estática e sem globais:** `float historico_co2[SENSORES][HORAS]` e `char nomes[SENSORES][TAM_NOME]` na `main` (3 setores × 24 horas; nomes: Estufa Alpha, Canteiro Sul, Setor Irrigado).
2. **Simulação do hardware:** `srand(time(NULL))` **uma vez** no topo da `main`; laços aninhados preenchem com `rand() % 1701 + 300` (300–2000 ppm). O legado usava `% 1700` e nunca chegava a 2000.
3. **A `main` só orquestra:** delega para `analisar_co2(matriz, nomes, linhas, colunas)`. A 2ª dimensão `[HORAS]` é obrigatória no parâmetro para o compilador calcular o salto de memória entre linhas.
4. **Painel humanizado:** imprime o **nome** do setor, nunca o índice.

**O que `analisar_co2` calcula**
- Por setor: **média**, contagem de **leituras acima de 1000 ppm** e **status** (`IDEAL` < 1000 ≤ `ALERTA` < 1300 ≤ `CRITICO`, decidido pela média, com `strcpy`).
- Global: **pico** com rastreamento triplo (valor + setor + coluna), inicializado com `matriz[0][0]` e não com 0.
- Resumo: **setor mais comprometido** (maior média) e **recomendação** (exaustão imediata se a média ≥ 1300).

**Decisões que valem citar**
- Dois limiares diferentes: `LIMITE_CO2 = 1000` (uma leitura) e `MEDIA_CRITICA = 1300` (média). A média de 24 sorteios fica perto de ~1150, então exigir 1500 tornaria "CRITICO" quase inalcançável.
- **Armadilha do acumulador:** `soma` é zerada dentro do laço externo, senão um setor herda o CO₂ do anterior.
- A recomendação segue o setor de **maior média**, não o do pico, para o painel não se contradizer.

---

## Parte 4 — Programa em C com sensor real (`ecotech_co2_sensor.c`)

**Não é a entrega do LXP** (o sandbox não tem serial e a diretriz exige `rand`). É a demonstração com hardware.

**Ideia central:** `analisar_co2` tem o **mesmo corpo e assinatura** da Parte 3. Só a *origem do dado* mudou: `rand()` virou leitura do sensor. Isso mostra o ganho da arquitetura (a função especialista não sabe de onde vem o dado).

**Diferenças**
- `SENSORES = 1` ("Estufa do tio Valdir"); cada coluna agora é uma **amostra de 2 s**, não uma hora (o nome `HORAS` foi mantido só por paridade; o rótulo impresso diz "amostra").
- Sem `srand/rand`. Lê **números da entrada padrão** (`scanf`).
- `ler_amostra`: aceita 200–10000 ppm; valor fora da faixa é **contado como descartado** e ignorado; texto onde deveria haver número (ruído de serial) é engolido caractere a caractere (senão `scanf` travaria em laço infinito). **Nunca inventa valor** para completar a matriz.
- `coletar_historico`: retorna 0 se a entrada acabar antes de completar 24 leituras → `main` sai com erro e **não gera painel**.
- Progresso vai para **stderr**, painel para **stdout** (dá para redirecionar só o painel).

**Uso manual:** `stty -F /dev/ttyACM0 9600 raw -echo; cat /dev/ttyACM0 | ./ecotech_sensor`. Teste sem Arduino: `python3 -c "import random;[print(random.randint(400,1700)) for _ in range(72)]" | ./ecotech_sensor` (com 1 setor só 24 números são usados).

**Integração:** o `server.py` compila este arquivo para o binário `ecotech_sensor` e alimenta a entrada padrão com a coluna de CO₂ das leituras, guardando a saída como "painel do programa em C".

---

## Parte 5 — Análise local (`analisar_ia.py`, sem IA)

Roda **sempre**, mesmo sem internet. Entrada: linhas `(ppm, solo%, temp°C)`. Formato lido por `parse` (ignora linhas que não têm 3 números).

**Configuração por canal (`CANAIS`)**

| Canal | Limite | Sentido | Salto suspeito | Faixa física |
|---|---|---|---|---|
| CO₂ | 1000 ppm | ruim quando **sobe** | > 400 | 200–10000 |
| Solo | 30 % | ruim quando **cai** | > 25 | 0–100 |
| Temperatura | 32 °C | ruim quando **sobe** | > 5 | −10–60 |

**`canal(v, c)` — por canal calcula:**
- **Tendência** por mínimos quadrados (reta `a·i + b`), expressa em unidade/min (`a × 60 / 2`, pois são 2 s entre amostras).
- **Previsão:** já além do limite; ou "cruza X em ~N min" extrapolando a reta; ou "não deve cruzar".
- **Saúde do sensor (`suspeito`):** saltos maiores que o limite entre amostras, ≥ 5 leituras idênticas seguidas (**travado**; desligado para temperatura, cuja resolução de 1 °C faz repetir valor ser normal) e valores fora da faixa física.
- Média, mínimo, máximo e quantas leituras passaram do limite.

**`setor(...)` — combina os três canais:**
- **Risco 0–100** (penalidades simples e auditáveis): CO₂ média ≥ 1300 → +40, ≥ 1000 → +20; solo média < 20 → +40, < 30 → +30; temperatura média > 32 → +25; +10 por canal com sensor suspeito; teto 100.
- **Regras determinísticas** (a IA só explica): ventilar se CO₂ ≥ 1000; irrigar se solo < 30, ou "programar irrigação" se está secando e vai cruzar; sombrear se temp > 32; "verificar sensor" para cada canal suspeito.
- **Correlações** de Pearson entre CO₂×temp, CO₂×solo, temp×solo (omitidas quando a série é constante).

**Ponto forte para defender:** decisões críticas são regras determinísticas e testáveis; a IA nunca decide sozinha. **Limite conhecido:** os limiares são valores genéricos e devem ser ajustados por cultura.

---

## Parte 6 — Camada de IA e histórico analítico

**Chamada à OpenAI (`openai()`):** `urllib` puro para `chat/completions`, modelo `gpt-4o-mini` (configurável em `OPENAI_MODEL`), chave em `OPENAI_API_KEY`. Erros (sem chave, HTTP, rede) viram texto de erro, **nunca derrubam a análise local**.

**Três usos**
1. **`laudo(resumo, painel)`:** recebe o resumo textual da análise local + o painel do programa em C e devolve **JSON** (`resumo_geral`, `setores[urgencia, diagnostico, acoes]`, `insights`). Usa `response_format=json_object`; se vier JSON inválido, devolve erro tratado.
2. **`chat(pergunta, resumo, historico)`:** perguntas livres respondidas **só com os dados** do teste atual e do histórico.
3. **`laudo_historico(hist)`:** analisa evolução dos testes; avisa se o histórico é de demonstração.

**Prompt de sistema (`SISTEMA`):** persona de agrônomo, descreve os sensores (MH-Z19B, capacitivo, temperatura interna com erro), limites e instrui: **leitura de sensor suspeito não é confiável**.

**Histórico analítico local (`historico_local`)** — sem IA: por canal, média de cada teste, tendência entre testes (`slope`), quantos testes ficaram além do limite, quantos tiveram sensor suspeito. Gera *insights* textuais (ex.: "CO₂ subindo X ppm por teste", "problema recorrente: vale checar o hardware" se suspeito em ≥ 3 testes, "risco geral em alta"). `resumo_historico` monta o texto enviado à IA.

**Perguntas prováveis:** *A IA pode errar?* Sim, por isso as ações por regra aparecem separadas das sugeridas pela IA. *O que a IA acrescenta?* Relações entre variáveis e priorização em linguagem natural.

---

## Parte 7 — Servidor e API (`server.py`)

`ThreadingHTTPServer` com o handler `H`. Iniciado em `main()` (compila o C e sobe a porta `PORT`, padrão 3000; `HOST` padrão `127.0.0.1`).

| Rota | Função |
|---|---|
| `GET /` | serve o `index.html` (troca `/dev/ttyACM0` pela porta detectada) |
| `POST /api/run?fonte=demo\|serial&porta=…` | coleta, roda o C, analisa, chama IA, salva no histórico |
| `POST /api/chat` | `{pergunta, resumo}` → resposta da IA |
| `GET /api/historico` | lista de testes + análise local do histórico |
| `GET /api/historico/<id>` | reabre um teste salvo |
| `POST /api/historico/analise` | laudo da IA sobre o histórico (mínimo 2 testes) |

**Fonte de dados**
- **Demo (`demo()`)**: 24 amostras; cada canal sorteia um comportamento (`estavel` 40%, `tendencia` 35%, `travado` 12%, `ruidoso` 13%) para exercitar previsão e detecção de sensor defeituoso.
- **Serial (`serial()`)**: configura a porta com `stty 9600 raw -echo`, lê com `cat` até juntar as leituras necessárias (24), timeout de 300 s. Se vierem menos, erro explícito. Um **lock** garante uma leitura serial por vez.

**`rodar()`:** obtém as linhas → executa o binário C com o CO₂ → `ia.processar` → `salvar` → devolve o resultado.

**Histórico:** `dados/historico.json` mantém só os **últimos 10 testes**. Gravação com lock e **troca atômica** (escreve `.tmp` e `os.replace`), então o arquivo nunca fica pela metade.

**Robustez:** POST com JSON inválido ou sem `pergunta` → HTTP 400; erros no `/api/run` → HTTP 500 com mensagem; `/api/run` é POST porque tem efeito colateral.

---

## Parte 8 — Painel web, testes e qualidade

**Front-end (`index.html`, um arquivo, HTML + CSS + JS puros, sem bibliotecas)**
- Cabeçalho: escolha **Demonstração** ou **Sensor real** (mostra o campo da porta), botão **Analisar**.
- `render(r)` monta: **índice de risco** com medidor, **Resumo da IA** e insights, um **cartão por canal** (média, gráfico SVG com limite tracejado e reta de tendência, mínimo/pico/tendência por minuto, previsão e saúde do sensor), **ações por regra** e **correlações**, **ações sugeridas pela IA**, **chat** com perguntas prontas, e blocos recolhíveis com a análise enviada à IA e o painel do programa em C.
- `estado()` define o selo do canal: fora do limite (vermelho), sensor suspeito / passou agora / tendência de risco (amarelo), normal (verde).
- **Histórico:** lista clicável dos últimos 10 testes (reabre o teste), mini-gráficos por canal e de risco, padrões calculados e botão "Analisar histórico com IA".
- Acessibilidade e UX: `aria-label`/`role`, tema claro/escuro automático, `prefers-reduced-motion`, responsivo. Texto vindo de IA/servidor passa por `esc()` (evita injeção de HTML).

**Testes (`test_projeto.py`, pytest, 16 testes, sem mocks)**
- Lógica pura direto: `parse`, risco e regras (setor saudável × crítico), previsão de cruzamento, sensor travado/salto, temperatura repetida não suspeita, correlação constante, poucas leituras, histórico.
- Servidor **real** em subprocesso (porta livre, dados em pasta temporária): página inicial, histórico, detalhe e 404, POST inválido (400), `/api/run` só por POST e uma execução demo completa incluindo o programa C real. A OpenAI é deixada inalcançável de propósito para provar que a análise local funciona sem IA.

**Melhorias já feitas nesta revisão:** chave ausente não derruba mais `/api/run`; validação de POST (400); `/api/run` virou POST; lock e timeout na serial; `main()` e `PORT`; condição de "travado" legível; regressão linear reaproveitada.

**Próximos passos (para a pergunta "o que falta?")**
- Dividir `analisar_ia.py` em módulos (`analise`, `ia`), mover o simulador e organizar pastas (`c/`, `docs/`, `tests/`).
- Calibrar o sensor de solo e considerar um sensor de temperatura dedicado.
- Ajustar limites por cultura; suportar vários setores de verdade (hoje `NOMES` tem um só).
- Compilar o C no build do Docker e rodar como usuário não-root; fazer o primeiro commit.
- Preencher o nome do aluno nos arquivos `.c`/`.ino` antes de entregar.

---

# Material de estudo

Para cada parte: **conceitos que você precisa dominar**, **como explicar em linguagem simples** e **exemplo para mostrar**. Estude o roteiro acima e depois esta seção.

## Estudo — Parte 1: Visão geral e infraestrutura

**Conceitos**
- **Cliente–servidor:** o navegador (cliente) pede coisas por HTTP; o `server.py` (servidor) responde. Tudo roda na sua máquina, em `127.0.0.1` (localhost = "este computador"), porta 3000.
- **Biblioteca padrão (stdlib):** o que já vem com o Python (`http.server`, `json`, `statistics`, `urllib`). Vantagem: nada para instalar.
- **Variável de ambiente e `.env`:** jeito de guardar segredos (a chave da OpenAI) fora do código. O `.env` nunca vai para o Git nem para a imagem Docker.
- **Docker:** *imagem* = receita empacotada do programa; *contêiner* = a imagem rodando; *volume* = pasta compartilhada com o computador (usada para o histórico não se perder).

**Como explicar:** "Nosso sistema é uma cadeia: sensor → Arduino → servidor → análise → IA → tela. Cada elo faz uma coisa só, e se um falha (por exemplo, a IA), os outros continuam."

**Por que escutar só em localhost?** Se abrisse para a rede, qualquer um na rede poderia usar nossa chave da OpenAI através do servidor.

## Estudo — Parte 2: Hardware e Arduino

**Conceitos**
- **Comunicação serial (UART):** dois fios (TX envia, RX recebe) trocando bytes em uma velocidade combinada, o *baud rate* (9600 bits/s). TX de um liga no RX do outro (cruzado).
- **SoftwareSerial:** o Arduino Uno só tem uma serial de hardware, que já é usada pelo cabo USB. Criamos uma serial "de mentira" nos pinos 10 e 11 para falar com o sensor.
- **Sensor NDIR:** mede CO₂ passando luz infravermelha por uma câmara; o CO₂ absorve um comprimento de onda específico, e quanto mais absorve, mais CO₂ há. Precisa aquecer (~3 min) para estabilizar.
- **Checksum:** um número calculado a partir dos outros bytes, usado para detectar corrupção. Se o checksum recebido não bate com o calculado, o pacote é descartado.
- **Dois bytes formam um número:** `ppm = byte2 × 256 + byte3` (o primeiro é a parte "alta", como as centenas em 1 e 23 → 123, só que na base 256).
- **Entrada analógica (`analogRead`):** o Arduino converte tensão em número de 0 a 1023. O sensor de solo capacitivo dá valor **alto no ar seco** e **baixo na água**.
- **Calibração linear:** transforma a leitura crua em porcentagem: `solo% = 100 × (SECO − leitura) / (SECO − MOLHADO)`. Exemplo com SECO=620 e MOLHADO=310: leitura 465 → 100 × 155 / 310 = **50 %**.
- **Timeout com `millis()`:** esperamos até 1 s pela resposta sem travar o Arduino para sempre.

**Como explicar o formato:** "O Arduino manda uma linha de texto a cada 2 s: `812,47.3,26` = 812 ppm de CO₂, 47,3 % de umidade do solo, 26 °C. Formato simples que qualquer programa lê."

**Ponto de honestidade:** a temperatura vem de dentro do sensor de CO₂ (que esquenta), então serve como referência, não como medida precisa do ar da estufa.

## Estudo — Parte 3: C da entrega (`ecotech_co2.c`)

**Conceitos**
- **Matriz 2D:** `float m[3][24]` = 3 linhas (setores) × 24 colunas (horas). Cada linha é um setor.
- **Memória contígua (row-major):** o C guarda a matriz linha após linha, em sequência. Para achar `m[i][j]`, o compilador calcula `i × 24 + j`. Por isso, ao passar a matriz para uma função, **a 2ª dimensão é obrigatória** (`float matriz[][HORAS]`): sem ela o compilador não sabe quanto pular para a próxima linha. A 1ª dimensão pode ficar vazia.
- **`rand()`/`srand()`:** `srand(time(NULL))` escolhe a "semente" a partir da hora atual. **Chamar uma vez só**, no começo. Se chamar dentro de uma função chamada várias vezes no mesmo segundo, a semente repete e os números saem iguais.
- **`rand() % 1701 + 300`:** `% 1701` dá 0–1700; somando 300 vira 300–2000.
- **Acumulador:** variável que soma valores (`soma += ...`). **Tem que ser zerada para cada setor**, dentro do laço externo; senão o setor 2 começa com a soma do setor 1.
- **Rastreio de máximo:** guardar o maior valor e *onde* ele ocorreu (setor e coluna). Começa com `matriz[0][0]` e não com 0, porque 0 poderia "ganhar" indevidamente em dados negativos.
- **`strcpy` e `printf` formatado:** `strcpy(status, "ALERTA")` copia texto para um vetor de char. `%-15s` alinha texto à esquerda em 15 colunas; `%12.2f` imprime float com 2 casas.
- **Sem variáveis globais:** tudo declarado na `main` e passado por parâmetro. Facilita entender e testar.

**Exemplo para mostrar (status):** média 1150 → `ALERTA` (≥ 1000 e < 1300); média 1320 → `CRITICO`; média 800 → `IDEAL`.

**Por que dois limiares?** Uma leitura acima de 1000 já é "tóxica", mas o status do setor depende da **média**. Como a média de números sorteados entre 300 e 2000 fica perto de 1150, se o crítico fosse 1500 nunca apareceria. Por isso 1300.

**Como explicar a arquitetura:** "A `main` só prepara os dados e chama a especialista. A função `analisar_co2` só analisa. Isso separa 'de onde vem o dado' de 'o que fazer com ele'."

## Estudo — Parte 4: C com sensor real

**Conceitos**
- **stdin / stdout / stderr:** três canais de todo programa. Entrada padrão (o que chega), saída padrão (resultado) e saída de erro (mensagens de progresso). Usar `stderr` para progresso permite salvar só o painel: `./ecotech_sensor > painel.txt`.
- **Pipe (`|`):** liga a saída de um programa à entrada de outro. `cat /dev/ttyACM0 | ./ecotech_sensor`: o `cat` despeja o que vem do Arduino e nosso programa lê.
- **`/dev/ttyACM0`:** no Linux, a porta do Arduino é um arquivo. `stty -F ... 9600 raw -echo` ajusta velocidade e desliga tratamento de texto do terminal.
- **Retorno do `scanf`:** devolve quantos itens leu. `1` = leu o número; `0` = veio texto no lugar; `EOF` = acabou a entrada. Se `0`, o texto não é consumido, então **precisamos ler um caractere** (`getchar`) para avançar, senão o laço nunca termina.
- **Validação:** valores fora de 200–10000 ppm são descartados e contados. **Nunca inventamos dado** para completar a matriz: ou temos as 24 leituras, ou não geramos painel (retorna erro).

**Como explicar o ganho:** "A função `analisar_co2` é idêntica à da entrega. Trocamos `rand()` por sensor real e ela não mudou uma linha. É a prova de que separar coleta de análise funciona."

## Estudo — Parte 5: Análise local

**Conceitos e como explicar**

1. **Média, mínimo, máximo:** resumem os dados. A média mostra o nível típico; o pico mostra o pior momento.

2. **Regressão linear (mínimos quadrados):** acha a reta que melhor passa pelos pontos. Fórmula da inclinação: `a = Σ(i − ī)(y − ȳ) / Σ(i − ī)²`. A inclinação `a` diz **quanto o valor muda por amostra**. Como são 2 s por amostra, `a × 60 / 2` = variação **por minuto**.

3. **Previsão de cruzamento do limite:** resolve a reta para chegar no limite. Exemplo: CO₂ sobe 5 ppm/amostra e a última leitura é 615; faltam 385 ppm → 385 / 5 = 77 amostras × 2 s = 154 s ≈ **2,6 min**. A tendência é 5 × 60 / 2 = **150 ppm/min**.

4. **Sentido do limite:** CO₂ e temperatura são ruins quando **sobem**; umidade do solo é ruim quando **cai**. O campo `sentido` (+1/−1) permite usar a mesma lógica para os três.

5. **Saúde do sensor (por que existe):** sensor barato falha de três jeitos e detectamos cada um:
   - **Salto:** diferença enorme entre duas amostras seguidas (ex.: CO₂ pulando mais de 400 ppm em 2 s é fisicamente improvável).
   - **Travado:** 5 ou mais leituras idênticas seguidas. Para temperatura isso é desligado, porque a resolução é 1 °C e repetir é normal.
   - **Fora da faixa física:** ex.: umidade acima de 100 %.

6. **Risco 0–100 (regra de pontos):** cada problema soma pontos. Exemplo: CO₂ médio 1100 (+20), solo médio 25 % (+30), temperatura normal (0), sem sensor suspeito (0) → **risco 50**. Sensor suspeito soma +10 por canal. O teto é 100. **É simples de propósito**: dá para auditar e explicar cada ponto.

7. **Correlação de Pearson:** número de −1 a +1. +1 = sobem juntas; −1 = uma sobe, outra desce; perto de 0 = sem relação. Se uma série é constante, não existe correlação (divisão por zero), por isso é omitida.

8. **Regras determinísticas vs IA:** as ações críticas (ventilar, irrigar, sombrear, verificar sensor) vêm de `if`s previsíveis. A IA só explica e prioriza.

**Pergunta clássica:** "Por que não usar só IA?" → IA pode errar ou variar; regras dão o mesmo resultado sempre e funcionam offline. Usamos IA onde ela é forte: linguagem natural e relacionar variáveis.

## Estudo — Parte 6: Camada de IA e histórico

**Conceitos**
- **LLM (modelo de linguagem):** o `gpt-4o-mini` gera texto a partir de um texto de entrada (*prompt*). Não "conhece" nossos dados: nós **enviamos** os números resumidos.
- **Mensagens `system` e `user`:** `system` define o papel e as regras (agrônomo, limites, "sensor suspeito não é confiável"); `user` traz os dados ou a pergunta.
- **Saída em JSON (`json_object`):** obrigamos a IA a responder num formato fixo (`resumo_geral`, `setores`, `insights`) para o front-end montar a tela sem "adivinhar" texto. Se vier JSON inválido, mostramos erro em vez de quebrar.
- **Ancoragem nos dados:** no chat, a instrução é "responda usando só estes dados". Reduz invenção (*alucinação*).
- **Degradação graciosa:** sem chave, sem internet ou API fora do ar, a análise local segue funcionando e a tela mostra o erro da IA num aviso. Testamos isso de propósito.
- **Privacidade:** o que vai à OpenAI é o **resumo textual** (médias, tendências, ações) e o painel do C, não o histórico bruto completo.

**Histórico local (sem IA):** para cada canal calcula a média de cada teste e a tendência **entre testes** (mesma regressão, agora sobre os testes). Gera frases como "CO₂ subindo X ppm por teste" ou "sensor suspeito em 3 de 5 testes: vale checar o hardware". Depois a IA pode comentar isso em cima.

**Ponto de atenção:** os testes com fonte `demo` são simulados; a IA é instruída a avisar quando o histórico é de demonstração.

## Estudo — Parte 7: Servidor e API

**Conceitos**
- **API HTTP e métodos:** `GET` = ler (não muda nada); `POST` = executar uma ação. `/api/run` é `POST` porque **grava histórico e usa a serial**.
- **Códigos de status:** 200 = deu certo; 400 = pedido inválido (culpa de quem pediu); 404 = não existe; 500 = erro do servidor.
- **`ThreadingHTTPServer`:** atende vários pedidos ao mesmo tempo, cada um numa *thread*. Isso cria risco de **condição de corrida** (duas threads mexendo no mesmo recurso).
- **Lock (trava):** garante que só uma thread por vez use algo. Usamos um lock para gravar o histórico e outro para a serial: uma segunda leitura serial recebe a mensagem "Ja existe uma leitura serial em andamento".
- **Gravação atômica:** escrevemos em `historico.json.tmp` e depois `os.replace` troca o arquivo de uma vez. Se o programa cair no meio, o arquivo bom antigo continua intacto.
- **`subprocess`:** o Python roda outros programas: `gcc` (compilar), `stty` (ajustar serial), `cat` (ler serial) e o binário `ecotech_sensor` (painel de CO₂).
- **Timeout:** a leitura serial tem um relógio de 300 s; se o Arduino não mandar as 24 leituras, dá erro claro em vez de travar.
- **Modo demo:** `demo()` gera curvas realistas. Cada canal sorteia um comportamento (estável, tendência, travado, ruidoso) para mostrar todas as funções, inclusive detecção de sensor defeituoso, sem precisar de hardware.

**Sequência de um clique em "Analisar" (decorar):**
1. Front-end faz `POST /api/run?fonte=demo`.
2. Servidor obtém 24 linhas (demo ou serial).
3. Manda o CO₂ para o programa em C → texto do painel.
4. `analisar_ia.processar` calcula tudo e chama a IA.
5. Resultado é salvo em `dados/historico.json` (mantém 10).
6. Devolve JSON → a tela desenha.

## Estudo — Parte 8: Painel web, testes e qualidade

**Conceitos**
- **`fetch` e `async/await`:** o navegador chama a API sem recarregar a página; `await` espera a resposta.
- **SVG:** gráficos desenhados com código (linhas, polígonos, textos), sem biblioteca. Nossos gráficos convertem valor → posição com uma função de escala (`x(i)`, `y(valor)`).
- **`esc()` e XSS:** o texto da IA ou do servidor é inserido em HTML. Sem escapar `&`, `<`, `>`, um texto malicioso poderia executar código na página (XSS). `esc()` neutraliza isso.
- **CSS responsivo:** `grid` com `auto-fit`, variáveis CSS (`--brand`, `--bad`), tema claro/escuro via `prefers-color-scheme`, animação reduzida via `prefers-reduced-motion`.
- **Acessibilidade:** `role="img"` e `aria-label` nos gráficos, `aria-live` no chat, cores nunca são a única informação (sempre há texto: "Fora do limite", "Normal").

**Testes com pytest (por que "sem mock")**
- **Mock** = objeto falso que finge ser algo real. Pode esconder bugs (o teste passa, mas o sistema real falha).
- Nossos testes usam: **funções reais** (análise, `parse`), o **servidor real** subindo em processo separado com porta livre e pasta temporária, e o **programa C real** compilado.
- Única coisa "desligada": a OpenAI, apontando o proxy para uma porta morta. Não é mock, é provar que a **análise funciona sem IA** e não gastar a chave.
- Cobertura: risco e regras, previsão, sensor travado/salto/temperatura, correlação, histórico, rotas HTTP, erro 400, `/api/run` só por POST, execução demo completa.

**Como rodar e mostrar:** `python3 -m pytest -q test_projeto.py` → `16 passed`.

---

## Glossário rápido

| Termo | Significado curto |
|---|---|
| ppm | partes por milhão: unidade de concentração de CO₂ (ar externo ≈ 400; acima de 1000 é ruim) |
| NDIR | sensor de CO₂ por absorção de luz infravermelha |
| UART / serial | comunicação byte a byte por dois fios, com velocidade combinada (baud) |
| Checksum | número de verificação que detecta bytes corrompidos |
| Calibração | ajustar a leitura crua do sensor para valores reais |
| Matriz 2D | tabela linhas × colunas na memória |
| stdin/stdout/stderr | entrada, saída e erro padrão de um programa |
| Pipe | liga a saída de um programa à entrada de outro |
| Regressão linear | reta que melhor se ajusta aos pontos; sua inclinação é a tendência |
| Correlação | −1 a +1, mede se duas variáveis variam juntas |
| Prompt | texto de instrução e dados enviado ao modelo de IA |
| Alucinação | a IA afirmar algo que não está nos dados |
| Degradação graciosa | perder uma função (IA) sem derrubar o resto |
| Lock | trava para uma thread por vez usar um recurso |
| Gravação atômica | trocar arquivo de uma vez para nunca ficar pela metade |
| XSS | inserir código malicioso numa página pela falta de escape de texto |
| Mock | objeto falso usado em testes no lugar do real |

---

## Perguntas difíceis (todos devem saber responder)

1. **Por que 1000 ppm?** É o limiar herdado do exercício original e comum como "ar ruim" em ambientes fechados. Em `CANAIS` é uma constante fácil de ajustar por cultura.
2. **A previsão é confiável?** É uma extrapolação linear em uma janela de ~48 s. Serve como alerta de tendência, não como previsão exata.
3. **E se o sensor estiver mentindo?** Detectamos saltos, valores travados e fora de faixa; o risco sobe (+10) e a IA é avisada de que aquele canal não é confiável.
4. **Por que dois programas em C?** `ecotech_co2.c` é a entrega da disciplina (simula com `rand`, 3 setores). `ecotech_co2_sensor.c` usa o sensor real (1 setor). A função de análise é a mesma.
5. **Por que a IA não decide sozinha?** Consistência e funcionamento offline: as regras críticas são determinísticas e testadas.
6. **O que acontece sem internet?** Tudo funciona, exceto o laudo, o chat e a análise do histórico por IA; a tela avisa o motivo.
7. **Onde ficam os dados?** `dados/historico.json`, últimos 10 testes, gravação atômica.
8. **Como escalar para várias estufas?** `NOMES` aceita vários setores e o front já desenha um bloco por setor; falta o firmware enviar vários sensores e ajustar `resumo_historico`, que hoje usa só o primeiro setor.
9. **Quais as limitações?** Calibração do solo por sensor, temperatura interna do módulo de CO₂, limites genéricos, dependência da OpenAI para o texto.
10. **O que melhoraria?** Sensor de temperatura dedicado, dividir `analisar_ia.py` em módulos, limites por cultura, primeiro commit e rodar o Docker sem root.

---

## Dicas para a apresentação (8 pessoas)

- **Passagem de bastão:** cada parte termina dizendo o que entrega para a próxima ("agora o Arduino já mandou `812,47.3,26`; quem recebe é o servidor"). A ordem das partes segue o caminho do dado.
- **Demonstração ao vivo:** abrir `./run.sh`, escolher **Demonstração**, clicar **Analisar** e mostrar risco, gráficos, ações por regra, chat e histórico. Deixar um teste salvo antes, caso a internet falhe.
- **Plano B:** se a OpenAI não responder, mostrar que a análise local continua e usar isso como argumento de robustez.
- **Tempo:** ~3–4 minutos por parte. Cada um explica **o que faz, por que foi assim e um exemplo**, sem ler código linha a linha.
- **Todos** devem saber o fluxo completo do início do documento e a sequência do clique em "Analisar" (Parte 7).
