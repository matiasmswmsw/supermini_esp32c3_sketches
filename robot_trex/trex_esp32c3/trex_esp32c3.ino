// Cabeça robótica T-Rex - ESP32-C3 Super Mini
// Cria o ponto de acesso "robotrex", serve a página de controle e envia
// comandos ao Arduino Uno pela UART1 (TX: GPIO21 -> RX do Uno).

#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "robotrex";
const char* password = "senha123456";

WebServer server(80);

#define RX_PIN 20
#define TX_PIN 21

HardwareSerial ArduinoSerial(1);

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Controle Robot T-Rex</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #121212; color: #ffffff; text-align: center; margin: 0; padding: 20px; }
    h1 { color: #ff3333; margin-bottom: 30px; }
    .btn { display: block; width: 80%; max-width: 300px; margin: 15px auto; padding: 18px; font-size: 18px; font-weight: bold; color: white; border: none; border-radius: 12px; cursor: pointer; }
    .btn:active { transform: scale(0.97); }
    .btn-abrir { background-color: #28a745; }
    .btn-fechar { background-color: #dc3545; }
    .btn-animar { background-color: #ff8c00; }
    #status { margin-top: 20px; color: #aaaaaa; min-height: 1.2em; }
  </style>
</head>
<body>
  <h1>🦖 T-REX CONTROLE</h1>
  <button class="btn btn-abrir" onclick="enviarComando('ABRIR', this)">📖 ABRIR BOCA</button>
  <button class="btn btn-animar" onclick="enviarComando('ANIMAR', this)">🔥 ANIMAR E MORDER</button>
  <button class="btn btn-fechar" onclick="enviarComando('FECHAR', this)">🔒 FECHAR BOCA</button>
  <div id="status"></div>
  <script>
    function enviarComando(cmd, btn) {
      const st = document.getElementById('status');
      st.textContent = 'Enviando: ' + btn.textContent + '...';
      fetch('/cmd?val=' + cmd)
        .then(r => r.ok ? r.text() : Promise.reject(r.status))
        .then(() => st.textContent = 'OK: ' + btn.textContent)
        .catch(() => st.textContent = 'Erro ao enviar comando');
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", HTML_PAGE); }

void handleCmd() {
  String val = server.arg("val");
  if (val == "ABRIR" || val == "FECHAR" || val == "ANIMAR") {
    ArduinoSerial.print(val);
    ArduinoSerial.print('\n');
    Serial.println("Comando enviado: " + val);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Comando invalido");
  }
}

void setup() {
  Serial.begin(115200);
  ArduinoSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();
  Serial.print("Acesse http://");
  Serial.println(WiFi.softAPIP());
}

void loop() { server.handleClient(); }
