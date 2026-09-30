# Robot T-Rex

Cabeça robótica de T-Rex: o ESP32-C3 Super Mini cria o Wi-Fi `robotrex` e serve a página de controle, e o Arduino Uno controla o servo da boca, o LED NeoPixel e o DFPlayer Mini.

- `trex_esp32c3/`: web server. Envia `ABRIR`, `FECHAR` ou `ANIMAR` pela UART1 (GPIO21 -> RX do Uno).
- `trex_uno/`: controle dos atuadores. Também aceita os comandos antigos (`SERVO_90`, `SERVO_0`, `SERVO_ANIMAR`).

## Ajustes (no topo de `trex_uno.ino`)

| Constante | Função |
|---|---|
| `POS_ABERTA` / `POS_FECHADA` | Ângulos da boca (trocados: 12° = aberta, 70° = fechada) |
| `MORDIDAS_MS[]` | Instantes das mordidas, em ms a partir do início do som |
| `LATENCIA_AUDIO_MS` | Atraso entre o comando de play e o som sair |
| `DURACAO_SOM_MS` | Duração do rugido |
| `USAR_BUSY` | 1 para sincronizar pelo pino BUSY do DFPlayer (ligado no pino 6) |

O som deve estar no cartão SD em `01/001.mp3`.
