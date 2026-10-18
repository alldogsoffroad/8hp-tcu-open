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
 * Supported SLCAN commands: S0..S8 (bit rate), O (open), L (open listen-only), C (close), t/T (send standard / extended frame), r/R (remote frame),
 * V (version), N (serial number), F (status flags), Z0/Z1 (timestamps), M/m (acceptance filter:
 * accepted, all frames are passed), E (project extension: error state,
 * reply "E<TEC 2 hex><REC 2 hex><ESR1 8 hex>", then ESR1 is read-cleared),
 * P0/P1/P2 (project extension, kept until reset: P1 = every later O opens LISTEN-ONLY, P2 =
 * listen-only plus automatic bit rate: after 200 ms with bus errors and no valid frame the
 * channel steps through 500k, 1M, 250k, 125k, 800k and reports each change as a marker frame
 * with extended ID 0x1FFFFF00 and data = new rate in kbit/s, 2 bytes big-endian).
 * Listen-only nodes never acknowledge and never send error flags: use them for sniffing.
 * K (project extension, channel must be closed): internal loop-back self-test, nothing goes on
 * the bus; sends one 11-bit and one 29-bit frame to itself, reply "K<std ok><ext ok>" (1/0). Received frames: "tIIILDD..[TTTT]\r" / "TIIIIIIIILDD..\r".
 *
 * Library: FlexCAN_T4 (included with Teensyduino).
 */
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_32> can_a;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_32> can_b;

struct Channel {
  Stream *port;
  bool open;
  uint8_t passive;            /* 0 normal, 1 listen-only, 2 listen-only + automatic bit rate */
  bool listen;                /* currently opened listen-only */
  uint8_t auto_idx;
  uint32_t last_check_ms, good_since_check;
  uint8_t last_rec;
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
  const FLEXCAN_RXTX mode = ch[i].listen ? LISTEN_ONLY : TX;
  if (i == 0) {
    can_a.setBaudRate(ch[0].bitrate, mode);
  } else {
    can_b.setBaudRate(ch[1].bitrate, mode);
  }
}

static const uint32_t kAutoRates[5] = { 500000, 1000000, 250000, 125000, 800000 };

static uint8_t rx_errors(int i) {
  return (uint8_t)(((i == 0 ? FLEXCAN1_ECR : FLEXCAN2_ECR) >> 8) & 0xFF);
}

/* Bus errors since the last call. In listen-only mode FlexCAN does not change the error counters,
 * but it still sets the error flags in ESR1 (bit1/bit0/ack/CRC/form/stuff, bits 15..10), which are
 * cleared by reading ESR1. */
static bool bus_errors_seen(int i) {
  const uint32_t esr = i == 0 ? FLEXCAN1_ESR1 : FLEXCAN2_ESR1;
  return (esr & 0x0000FC00u) != 0u;
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

/* FlexCAN error counters (ECR: TX count bits 7..0, RX count bits 15..8) and status (ESR1) */
static void cmd_error_state(int i) {
  char buf[24];
  const uint32_t ecr = i == 0 ? FLEXCAN1_ECR : FLEXCAN2_ECR;
  const uint32_t esr = i == 0 ? FLEXCAN1_ESR1 : FLEXCAN2_ESR1;
  snprintf(buf, sizeof buf, "E%02lX%02lX%08lX\r", (unsigned long)(ecr & 0xFF),
           (unsigned long)((ecr >> 8) & 0xFF), (unsigned long)esr);
  reply(i, buf);
}
static void ok(int i) { ch[i].port->write('\r'); }
static void err(int i) { ch[i].port->write('\a'); }

/* t/T/r/R: send a frame */
static void cmd_send(int i, const char *s, bool ext, bool rtr) {
  const uint8_t idlen = ext ? 8 : 3;
  uint32_t id, dlc, b;
  CAN_message_t m;

  if (!ch[i].open || ch[i].listen || !parse_hex(s + 1, idlen, &id) || !parse_hex(s + 1 + idlen, 1, &dlc) ||
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

/* K: loop-back self-test of standard and extended reception (no bus traffic) */
static void cmd_selftest(int i) {
  CAN_message_t t, r;
  bool got_std = false, got_ext = false;
  char buf[8];

  if (ch[i].open) { err(i); return; }
  if (i == 0) can_a.enableLoopBack(true); else can_b.enableLoopBack(true);
  for (int k = 0; k < 2; k++) {
    t = CAN_message_t();
    t.id = k == 0 ? 0x123 : 0x18DAF110;
    t.flags.extended = k == 1;
    t.len = 2;
    t.buf[0] = 0x5A;
    t.buf[1] = (uint8_t)k;
    if (i == 0) can_a.write(t); else can_b.write(t);
  }
  for (uint32_t start = millis(); millis() - start < 50u;) {
    if ((i == 0 ? can_a.read(r) : can_b.read(r)) && r.buf[0] == 0x5A) {
      if (r.flags.extended && r.id == 0x18DAF110) got_ext = true;
      if (!r.flags.extended && r.id == 0x123) got_std = true;
    }
  }
  if (i == 0) can_a.enableLoopBack(false); else can_b.enableLoopBack(false);
  snprintf(buf, sizeof buf, "K%d%d\r", got_std ? 1 : 0, got_ext ? 1 : 0);
  reply(i, buf);
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
    case 'L':
      ch[i].listen = s[0] == 'L' || ch[i].passive != 0;
      ch[i].auto_idx = 0;
      for (uint8_t k = 0; k < 5; k++) {
        if (kAutoRates[k] == ch[i].bitrate) ch[i].auto_idx = k;
      }
      set_rate(i);
      ch[i].open = true;
      ch[i].last_check_ms = millis();
      ch[i].good_since_check = 0;
      ch[i].last_rec = rx_errors(i);
      ok(i);
      break;
    case 'P':
      if (s[1] < '0' || s[1] > '2') { err(i); break; }
      ch[i].passive = (uint8_t)(s[1] - '0');
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
    case 'E': cmd_error_state(i); break;
    case 'K': cmd_selftest(i); break;
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
  ch[i].good_since_check++;
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

/* P2: if the bus only produced receive errors since the last check, try the next bit rate */
static void auto_baud(int i) {
  CAN_message_t m;
  uint8_t rec;

  if (!ch[i].open || ch[i].passive != 2 || millis() - ch[i].last_check_ms < 200u) return;
  ch[i].last_check_ms = millis();
  rec = rx_errors(i);
  if (ch[i].good_since_check == 0 && (bus_errors_seen(i) || rec != ch[i].last_rec)) {
    ch[i].auto_idx = (uint8_t)((ch[i].auto_idx + 1) % 5);
    ch[i].bitrate = kAutoRates[ch[i].auto_idx];
    set_rate(i);
    m.id = 0x1FFFFF00;
    m.flags.extended = true;
    m.flags.remote = false;
    m.len = 2;
    m.buf[0] = (uint8_t)((ch[i].bitrate / 1000) >> 8);
    m.buf[1] = (uint8_t)(ch[i].bitrate / 1000);
    forward(i, m);
    (void)bus_errors_seen(i);           /* discard flags raised before the switch */
    rec = rx_errors(i);
  }
  ch[i].last_rec = rec;
  ch[i].good_since_check = 0;
}

/* Accept standard AND extended frames in the RX FIFO. With CTRL2.EACEN = 0 (reset value) the
 * FlexCAN filters always compare the IDE bit, so the "accept all" filter passed only 11-bit
 * frames and every 29-bit frame was dropped silently. Setting EACEN lets the zero masks ignore
 * IDE/RTR as well. CTRL2 may only be written in freeze mode. */
static void accept_all_ids(uint32_t mcr_addr) {
  volatile uint32_t &mcr = *(volatile uint32_t *)mcr_addr;
  volatile uint32_t &ctrl2 = *(volatile uint32_t *)(mcr_addr + 0x34);
  mcr |= 0x40000000u | 0x10000000u;                      /* FRZ + HALT */
  for (uint32_t g = 0; g < 100000u && !(mcr & 0x01000000u); g++) { }   /* FRZ_ACK */
  ctrl2 |= (1u << 16);                                   /* EACEN */
  mcr &= ~0x10000000u;                                   /* leave HALT */
  for (uint32_t g = 0; g < 100000u && (mcr & 0x01000000u); g++) { }
}

void setup() {
  ch[0].port = &Serial;
  ch[1].port = &SerialUSB1;
  for (int i = 0; i < 2; i++) {
    ch[i].open = false;
    ch[i].timestamps = false;
    ch[i].bitrate = 500000;
    ch[i].len = 0;
    ch[i].passive = 0;
    ch[i].listen = false;
  }
  Serial.begin(2000000);
  SerialUSB1.begin(2000000);
  can_a.begin();
  can_a.setBaudRate(500000);
  can_a.setMaxMB(16);
  can_a.enableFIFO();
  can_a.setFIFOFilter(ACCEPT_ALL);
  accept_all_ids(IMXRT_FLEXCAN1_ADDRESS);
  can_b.begin();
  can_b.setBaudRate(500000);
  can_b.setMaxMB(16);
  can_b.enableFIFO();
  can_b.setFIFOFilter(ACCEPT_ALL);
  accept_all_ids(IMXRT_FLEXCAN2_ADDRESS);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  CAN_message_t m;
  static uint32_t led_ms;

  poll_port(0);
  poll_port(1);
  while (can_a.read(m)) forward(0, m);
  while (can_b.read(m)) forward(1, m);
  auto_baud(0);
  auto_baud(1);
  if (millis() - led_ms > 500u) {     /* heartbeat LED: fast blink when a channel is open */
    led_ms = millis();
    digitalWriteFast(LED_BUILTIN, (ch[0].open || ch[1].open) ? !digitalReadFast(LED_BUILTIN) : LOW);
  }
}
