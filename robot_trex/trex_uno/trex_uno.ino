// Cabeça robótica T-Rex - Arduino Uno
// Recebe comandos do ESP32-C3 pela Serial (pino 0) e controla servo, LED e DFPlayer.
//
// Comandos aceitos (terminados em '\n'):
//   ABRIR   (ou SERVO_90)      -> abre a boca, sem som e sem LED
//   FECHAR  (ou SERVO_0)       -> fecha a boca e mantém fechada
//   ANIMAR  (ou SERVO_ANIMAR)  -> LED vermelho + rugido + mordidas sincronizadas
//
// Todo o código é não bloqueante (sem delay() no loop): um comando novo
// interrompe a animação em andamento na hora.

#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- Pinos ----------------
#define PIN_SERVO   9
#define PIN_LED     5
#define NUM_LEDS    1
#define PIN_DF_RX   10   // RX do Uno <- TX do DFPlayer
#define PIN_DF_TX   11   // TX do Uno -> RX do DFPlayer (resistor de 1k em série)

// Opcional: pino BUSY do DFPlayer (fica LOW enquanto o som toca).
// Se ligar o BUSY no pino abaixo, mude USAR_BUSY para 1: a animação começa
// exatamente quando o som começa e termina quando o som acaba.
#define USAR_BUSY   0
#define PIN_DF_BUSY 6

// ---------------- Calibração do servo ----------------
// Antes estava POS_ABERTA=70 / POS_FECHADA=12, mas na montagem real
// 70° fecha a boca. Por isso os valores foram trocados.
#define POS_ABERTA   12
#define POS_FECHADA  70
#define MS_POR_GRAU  4    // velocidade dos comandos ABRIR/FECHAR

// ---------------- Áudio ----------------
#define VOLUME             12     // 0..30
#define PASTA_SOM          1      // pasta "01" no cartão SD
#define ARQUIVO_SOM        1      // arquivo "001.mp3"
#define DURACAO_SOM_MS     5000   // duração do rugido
#define LATENCIA_AUDIO_MS  200    // atraso entre o comando e o som sair (sem BUSY)

// ---------------- Coreografia da mordida ----------------
// Instantes (ms, a partir do início do som) em que cada mordida começa.
// Ajuste para coincidir com os picos do rugido.
const uint16_t MORDIDAS_MS[] = { 300, 1200, 2100, 3000, 3900 };
const uint8_t  NUM_MORDIDAS  = sizeof(MORDIDAS_MS) / sizeof(MORDIDAS_MS[0]);
#define T_FECHAR_MS   150   // boca fechando (rápido)
#define T_SEGURAR_MS  100   // boca fechada
#define T_ABRIR_MS    400   // boca reabrindo (mais devagar)

// Brilho do LED vermelho durante a animação: fraco com a boca aberta,
// máximo no instante da mordida.
#define BRILHO_MIN  60
#define BRILHO_MAX  255

Servo servoBoca;
Adafruit_NeoPixel fita(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);
SoftwareSerial dfSerial(PIN_DF_RX, PIN_DF_TX);
DFRobotDFPlayerMini dfPlayer;

enum Estado { PARADO, ESPERANDO_SOM, ANIMANDO };
Estado estado = PARADO;

int posAtual = POS_ABERTA;
int posAlvo  = POS_ABERTA;
unsigned long tUltimoPasso = 0;
unsigned long tInicio = 0;   // quando o comando ANIMAR chegou
unsigned long tSom    = 0;   // quando o som (estimado ou via BUSY) começou
int brilhoAtual = -1;

char bufCmd[24];
uint8_t lenCmd = 0;

// ---------------- LED ----------------
void ledVermelho(int brilho) {
  if (brilho == brilhoAtual) return;   // evita show() desnecessário
  brilhoAtual = brilho;
  fita.setPixelColor(0, fita.Color(brilho, 0, 0));
  fita.show();
}

void ledDesligar() {
  brilhoAtual = 0;
  fita.clear();
  fita.show();
}

// ---------------- Servo ----------------
void escreverServo(int pos) {
  if (pos != posAtual) {
    servoBoca.write(pos);
    posAtual = pos;
  }
}

// Movimento suave e não bloqueante até posAlvo (usado por ABRIR/FECHAR)
void atualizarMovimento() {
  if (posAtual == posAlvo) return;
  unsigned long agora = millis();
  if (agora - tUltimoPasso < MS_POR_GRAU) return;
  tUltimoPasso = agora;
  escreverServo(posAtual + (posAlvo > posAtual ? 1 : -1));
}

// Curva suave (smoothstep) 0..1
float suave(float x) {
  return x * x * (3 - 2 * x);
}

// Quanto a boca está fechada (0 = aberta, 1 = fechada) no instante t do som
float fechamentoNoTempo(unsigned long t) {
  for (uint8_t i = 0; i < NUM_MORDIDAS; i++) {
    if (t < MORDIDAS_MS[i]) break;
    unsigned long d = t - MORDIDAS_MS[i];
    if (d < T_FECHAR_MS) return suave((float)d / T_FECHAR_MS);
    d -= T_FECHAR_MS;
    if (d < T_SEGURAR_MS) return 1.0;
    d -= T_SEGURAR_MS;
    if (d < T_ABRIR_MS) return 1.0 - suave((float)d / T_ABRIR_MS);
  }
  return 0.0;
}

// ---------------- Ações ----------------
void pararAnimacao() {
  if (estado != PARADO) dfPlayer.stop();
  estado = PARADO;
  ledDesligar();
}

void irPara(int pos) {
  pararAnimacao();
  posAlvo = pos;
}

void iniciarAnimacao() {
  // Começa sempre com a boca aberta e LED fraco
  escreverServo(POS_ABERTA);
  posAlvo = POS_ABERTA;
  ledVermelho(BRILHO_MIN);

  dfPlayer.playFolder(PASTA_SOM, ARQUIVO_SOM);
  tInicio = millis();
  estado = ESPERANDO_SOM;
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (estado == ESPERANDO_SOM) {
#if USAR_BUSY
    bool somComecou = digitalRead(PIN_DF_BUSY) == LOW;
    // Se o BUSY não baixar em 1 s, segue mesmo assim
    if (somComecou || agora - tInicio > 1000) {
#else
    if (agora - tInicio >= LATENCIA_AUDIO_MS) {
#endif
      tSom = agora;
      estado = ANIMANDO;
    }
    return;
  }

  if (estado == ANIMANDO) {
    unsigned long t = agora - tSom;
    bool acabou = t >= DURACAO_SOM_MS;
#if USAR_BUSY
    // Termina quando o som realmente acaba (com folga de 300 ms na partida)
    if (t > 300 && digitalRead(PIN_DF_BUSY) == HIGH) acabou = true;
#endif
    if (acabou) {
      estado = PARADO;
      ledDesligar();
      posAlvo = POS_ABERTA;   // termina com a boca aberta
      return;
    }

    float f = fechamentoNoTempo(t);
    escreverServo(POS_ABERTA + (int)((POS_FECHADA - POS_ABERTA) * f + 0.5));
    posAlvo = posAtual;
    ledVermelho(BRILHO_MIN + (int)((BRILHO_MAX - BRILHO_MIN) * f));
  }
}

// ---------------- Comandos ----------------
void executarComando(const char* cmd) {
  if (!strcmp(cmd, "ABRIR") || !strcmp(cmd, "SERVO_90")) {
    irPara(POS_ABERTA);
  } else if (!strcmp(cmd, "FECHAR") || !strcmp(cmd, "SERVO_0")) {
    irPara(POS_FECHADA);
  } else if (!strcmp(cmd, "ANIMAR") || !strcmp(cmd, "SERVO_ANIMAR")) {
    iniciarAnimacao();
  }
}

// Leitura não bloqueante da Serial, linha a linha
void lerSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lenCmd > 0) {
        bufCmd[lenCmd] = '\0';
        executarComando(bufCmd);
        lenCmd = 0;
      }
    } else if (lenCmd < sizeof(bufCmd) - 1) {
      bufCmd[lenCmd++] = c;
    }
  }
}

void setup() {
  Serial.begin(9600);
  dfSerial.begin(9600);

  fita.begin();
  ledDesligar();

  // Escreve a posição antes do attach para o servo não dar um tranco até 90°
  servoBoca.write(POS_ABERTA);
  servoBoca.attach(PIN_SERVO);

#if USAR_BUSY
  pinMode(PIN_DF_BUSY, INPUT_PULLUP);
#endif

  // isACK = false: os comandos ao DFPlayer não ficam esperando resposta,
  // então play/stop saem na hora e não travam o loop.
  delay(1000);                       // tempo do DFPlayer inicializar
  dfPlayer.begin(dfSerial, false, false);
  delay(200);
  dfPlayer.volume(VOLUME);           // volume enviado uma única vez
}

void loop() {
  lerSerial();
  if (estado == PARADO) {
    atualizarMovimento();
  } else {
    atualizarAnimacao();
  }
}
