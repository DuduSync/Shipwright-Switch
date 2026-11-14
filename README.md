# 🎮 DuduSync — Zelda: Ocarina of Time PT-BR no Switch

> Projeto pessoal do **DuduSync**: trazer a **dublagem oficial PT-BR e os visuais do remake de OoT do Switch 2** para o **Ship of Harkinian (SoH)** rodando no **Nintendo Switch 1**.

## O que é

Este fork do [Shipwright-Switch](https://github.com/timschneeb/Shipwright-Switch) é a base do meu port **PT-BR** de Ocarina of Time:

- **Dublagem oficial PT-BR** extraída do remake do Switch 2 (vozes `USpt` do `.nup`)
- **Textos PT-BR oficiais do remake** nas cutscenes dubladas (legenda = voz)
- **Texturas HD/3DS**: OoT Reloaded + Djipi's 3DS Experience
- **Pipeline** de mapeamento por `CS_TEXT` do decomp, validação com **Parakeet v3** (STT local no celular)
- **Builds automáticos** via GitHub Actions → `soh.nro` + `soh.o2r`

## Status

- ✅ Intro dublada e sincronizada (aprovada)
- ✅ Medalhões, Espada Mestra, Lenda da Triforce e Ending
- 🔄 Expansão para as demais cutscenes (V1 + V2)
- ✅ 35 OTRs do Djipi instalados; HD Reloaded 4 GB
- ⚠️ NPCs: o remake só dublou cutscenes; não há voz de NPC nos arquivos oficiais

## Como funciona

1. Extração do `.nup` (NupExplorer): vozes BWAV + MSBT PT-BR
2. Mapeamento voz → `textId` do OoT via `CS_TEXT` do decomp (`zeldaret/oot`)
3. Geração de `voices/ptbr/<TID>_<página>.wav` (48 kHz)
4. Hook no SoH (`DuduSyncVoice`) + mixer no `libultraship` (Switch só tem 1 device SDL)
5. Texto PTBR trocado pelo oficial do remake nas caixas dubladas
6. CI no GitHub Actions gera o build; instalação via FTP (Sphaira)

## Build

O workflow **`generate-builds`** roda no push da branch **`dudu-voice`** e publica o artifact `soh-nx` (via nightly.link). O `soh.o2r` é gerado separadamente.

## Créditos

- [Harbour Masters / Shipwright](https://github.com/HarbourMasters/Shipwright) — o port
- [timschneeb / Shipwright-Switch](https://github.com/timschneeb/Shipwright-Switch) — port do Switch
- [zeldaret/oot](https://github.com/zeldaret/oot) — decomp e CS_TEXT
- [Djipi's 3DS Experience](https://gamebanana.com/mods/477979), [OoT Reloaded](https://github.com/GhostlyDark/OoT-Reloaded-SoH), [Shipwright-PTBR](https://github.com/GeraldoCruzeiro/Shipwright-PTBR)
- [sherpa-onnx / Parakeet v3](https://github.com/k2-fsa/sherpa-onnx) — STT local

## Aviso

Projeto **fan-made**, sem fins comerciais. **Não distribui ROM nem assets originais**; requer uma cópia legal de Ocarina of Time. Todo o crédito aos autores originais.
