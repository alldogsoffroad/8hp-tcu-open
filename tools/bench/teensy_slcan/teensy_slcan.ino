/* Teensy 4.1 dual-channel SLCAN (Lawicel) adapter for the TCU bench.
 *
 * USB type: "Dual Serial" (Arduino IDE: Tools > USB Type > Dual Serial).
 *   first USB serial port  (Serial)     <-> CAN1  (pins 22 TX / 23 RX)  -> TCU pins 5/6 (CAN-C)
 *   second USB serial port (SerialUSB1) <-> CAN2  (pins  1 TX /  0 RX)  -> TCU pins 3/4 (shifter bus)
 * Each CAN pin pair needs a 3.3 V CAN transceiver (for example SN65HVD230).
 *
 * On Linux each port becomes a SocketCAN interface:
 *   sudo slcand -o -s6 -t hw -S 2000000 /dev/ttyACM0 can0 && sudo ip link set can0 up
 *   sudo slcand -o -s6 -t hw -S 2000000 /dev/ttyACM1 can1 && sudo ip link set can1 up
 *
 * Supported SLCAN commands: S0..S8 (bit rate), O (open), L (open; listen-only is not
 * implemented, see below), C (close), t/T (send standard / extended frame), r/R (remote frame),
 * V (version), N (serial number), F (status flags), Z0/Z1 (timestamps), M/m (acceptance filter:
 * accepted, all frames are passed). Received frames: "tIIILDD..[TTTT]\r" / "TIIIIIIIILDD..\r".
 *
 * Library: FlexCAN_T4 (included with Teensyduino).
 */
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_32> can_a;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_32> can_b;

struct Channel {
  Stream *port;
  bool open;
  bool timestamps;
  uint32_t bitrate;
  char line[40];
  uint8_t len;
  uint32_t rx_count, tx_count, errors;
};

static Channel ch[2];

static const uint32_t kRates[9] = { 10000, 20000, 50000, 100000, 125000, 250000, 500000,
                                    800000, 1000000 };

static void set_rate(int i) {
  if (i == 0) {
    can_a.setBaudRate(ch[0].bitrate);
  } else {
    can_b.setBaudRate(ch[1].bitrate);
  }
}

static bool write_frame(int i, const CAN_message_t &m) {
  return i == 0 ? can_a.write(m) > 0 : can_b.write(m) > 0;
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool parse_hex(const char *s, uint8_t n, uint32_t *out) {
  uint32_t v = 0;
  for (uint8_t k = 0; k < n; k++) {
    int h = hexval(s[k]);
    if (h < 0) return false;
    v = (v << 4) | (uint32_t)h;
  }
  *out = v;
  return true;
}

static void reply(int i, const char *s) { ch[i].port->print(s); }
static void ok(int i) { ch[i].port->write('\r'); }
static void err(int i) { ch[i].port->write('\a'); }

/* t/T/r/R: send a frame */
static void cmd_send(int i, const char *s, bool ext, bool rtr) {
  const uint8_t idlen = ext ? 8 : 3;
  uint32_t id, dlc, b;
  CAN_message_t m;

  if (!ch[i].open || !parse_hex(s + 1, idlen, &id) || !parse_hex(s + 1 + idlen, 1, &dlc) ||
      dlc > 8) {
    err(i);
    return;
  }
  m.id = id;
  m.flags.extended = ext;
  m.flags.remote = rtr;
  m.len = (uint8_t)dlc;
  if (!rtr) {
    for (uint8_t k = 0; k < dlc; k++) {
      if (!parse_hex(s + 2 + idlen + 2 * k, 2, &b)) {
        err(i);
        return;
      }
      m.buf[k] = (uint8_t)b;
    }
  }
  if (write_frame(i, m)) {
    ch[i].tx_count++;
    ch[i].port->write(ext ? 'Z' : 'z');
    ok(i);
  } else {
    ch[i].errors++;
    err(i);
  }
}

static void handle_line(int i) {
  const char *s = ch[i].line;
  char buf[16];

  switch (s[0]) {
    case 'S':
      if (ch[i].open || s[1] < '0' || s[1] > '8') { err(i); break; }
      ch[i].bitrate = kRates[s[1] - '0'];
      set_rate(i);
      ok(i);
      break;
    case 'O':
    case 'L':  /* listen-only would need the FlexCAN LOM bit; opened normally instead */
      set_rate(i);
      ch[i].open = true;
      ok(i);
      break;
    case 'C':
      ch[i].open = false;
      ok(i);
      break;
    case 't': cmd_send(i, s, false, false); break;
    case 'T': cmd_send(i, s, true, false); break;
    case 'r': cmd_send(i, s, false, true); break;
    case 'R': cmd_send(i, s, true, true); break;
    case 'V': reply(i, "V0101\r"); break;
    case 'v': reply(i, "vTCU1\r"); break;
    case 'N': snprintf(buf, sizeof buf, "N%04X\r", i == 0 ? 0xA001 : 0xA002); reply(i, buf); break;
    case 'F': reply(i, "F00\r"); break;
    case 'Z': ch[i].timestamps = s[1] == '1'; ok(i); break;
    case 'M':
    case 'm':
    case 'X':
    case 'W':
    case 'U': ok(i); break;
    default: err(i); break;
  }
}

static void poll_port(int i) {
  while (ch[i].port->available()) {
    char c = (char)ch[i].port->read();
    if (c == '\r') {
      ch[i].line[ch[i].len] = '\0';
      if (ch[i].len) handle_line(i);
      ch[i].len = 0;
    } else if (c != '\n' && ch[i].len < sizeof ch[i].line - 1) {
      ch[i].line[ch[i].len++] = c;
    }
  }
}

static void forward(int i, const CAN_message_t &m) {
  char out[40];
  int n;

  if (!ch[i].open) return;
  ch[i].rx_count++;
  if (m.flags.extended) {
    n = snprintf(out, sizeof out, "%c%08lX%u", m.flags.remote ? 'R' : 'T',
                 (unsigned long)m.id, m.len);
  } else {
    n = snprintf(out, sizeof out, "%c%03lX%u", m.flags.remote ? 'r' : 't',
                 (unsigned long)m.id, m.len);
  }
  if (!m.flags.remote) {
    for (uint8_t k = 0; k < m.len && n < 34; k++) {
      n += snprintf(out + n, sizeof out - n, "%02X", m.buf[k]);
    }
  }
  if (ch[i].timestamps) {
    n += snprintf(out + n, sizeof out - n, "%04lX", (unsigned long)(millis() % 60000u));
  }
  out[n++] = '\r';
  ch[i].port->write(out, n);
}

void setup() {
  ch[0].port = &Serial;
  ch[1].port = &SerialUSB1;
  for (int i = 0; i < 2; i++) {
    ch[i].open = false;
    ch[i].timestamps = false;
    ch[i].bitrate = 500000;
    ch[i].len = 0;
  }
  Serial.begin(2000000);
  SerialUSB1.begin(2000000);
  can_a.begin();
  can_a.setBaudRate(500000);
  can_a.setMaxMB(16);
  can_a.enableFIFO();
  can_b.begin();
  can_b.setBaudRate(500000);
  can_b.setMaxMB(16);
  can_b.enableFIFO();
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  CAN_message_t m;
  static uint32_t led_ms;

  poll_port(0);
  poll_port(1);
  while (can_a.read(m)) forward(0, m);
  while (can_b.read(m)) forward(1, m);
  if (millis() - led_ms > 500u) {     /* heartbeat LED: fast blink when a channel is open */
    led_ms = millis();
    digitalWriteFast(LED_BUILTIN, (ch[0].open || ch[1].open) ? !digitalReadFast(LED_BUILTIN) : LOW);
  }
}
