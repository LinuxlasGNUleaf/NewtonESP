#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */) {
  id = -1;
  update_flag = false;

  players_index = 0;
  planets_index = 0;

  players = new Player[max_players];
  planets = new Planet[max_planets];
  ignored = new int[max_players];

  for (byte i = 0; i < max_players; i++) {
    ignored[i] = -1;
    players[i].active = false;
  }
}

void NESPtonBot::init() { Serial.begin(115200); }

void NESPtonBot::connect() {
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present! Abort.");
    while (true)
      ;
  }
  Serial.printf("Connecting to '%s'...", ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("done.\nConnecting to Server at %s:%d...", server, port);
  while (!client.connect(server, port)) {
    Serial.print(".");
  }
  Serial.println("connected.");

  // set name, dump recv buffer and enable buffer mode
  client.printf("n %s\n", name);
  client.setTimeout(discard_timeout);
  client.readString();
  client.printf("b %d\n", version);
}

void NESPtonBot::processRecv() {
  // receive msg_id and payload
  client.setTimeout(recv_timeout);
  uint8_t buf[2 * sizeof(int)];
  client.readBytes(buf, sizeof(int));

  // extract from buffer
  int msg_type = buf[0];
  int payload = buf[sizeof(int)];

  switch (msg_type) {
  case 1: // bot join
    id = payload;
    update_flag = true;
    Serial.printf("Own ID received: %d\n", id);
    break;

  case 2: // player leave
    // mark player inactive
    players[payload].active = false;
    remove_from_ignored(ignored, payload);
    Serial.printf("RECV: Player %d left the game.\n", payload);

  case 3: // player joined / moved
    uint8_t pos_buf[pos_size];
    client.readBytes(pos_buf, pos_size);

    if (!players[payload].active) {
      // player is not yet registered -> joined
      Serial.printf("RECV: Player %d joined the game at (%d,%d)\n", payload,
                    (int)round(pos_buf[0]), (int)round(pos_buf[pos_size/2]));
      players[payload].active = true;

    } else {
      // player is already registered -> moved
      Serial.printf("RECV: Player %d moved to (%d,%d)\n", payload, (int)round(pos_buf[0]),
                    (int)round(pos_buf[pos_size / 2]));

      // was bot moved?
      if (payload == id) {
        for (int i = 0; i < max_players; i++)
          ignored[i] = -1;
      } else {
        // try to remove opponent from ignored list
        remove_from_ignored(ignored, payload);
      }
    }

    // copy player info to array
    memcpy(&players[payload], &payload, sizeof(bool));
    memcpy(&players[payload] + pos_size, pos_buf, pos_size);
    update_flag = true;
    break;

  case 4: // shot finished, DEPRECATED
    Serial.println("wrong bot protocol, this msg_type (4) should not be received!");
    while (1);
    break;

  case 5: // shot begin
    // angle, velocity = self.connection.receive_struct("dd")
    client.readBytes(pos_buf, pos_size);
    break;

  case 6: // shot end
    // receive angle and velocity
    uint8_t discard_buf[pos_size];
    client.readBytes(discard_buf, pos_size);

    // receive number of segments & discard segments
    uint8_t n_buf[sizeof(int)];
    client.readBytes(n_buf, sizeof(int));
    int n;
    
    memcpy(&n, n_buf, sizeof(int));
    for (int i = 0; i < n; i++){
      client.readBytes(discard_buf, sizeof(float)*2);
    }
    break;

  case 7: // game mode, deprecated
    Serial.println("wrong bot protocol, this msg_type (7) should not be received!");
    while (1);
    break;

  case 8: // own energy
    uint8_t energy_buf[sizeof(double)];
    client.readBytes(energy_buf, sizeof(double));
    memcpy(&energy, energy_buf, sizeof(double));
    break;

  case 9: // planet info
    uint8_t count_buf[sizeof(int)];
    client.readBytes(count_buf, sizeof(int));
    int count;
    memcpy(count, count_buf)
    /*
    # discard planet byte count
    self.connection.receive_struct("I")

    self.planets = []
    for i in range(payload):
        x, y, radius, mass = self.connection.receive_struct("dddd")
        self.planets.append(Planet(x, y, radius, mass, i))
    self.msg(f"planet data for {len(self.planets)} planets received")
    tmp = self.planets[-1]
    self.update_simulation()

      break;
      */

  default:
    Serial.println("UNKNOWN MSG_TYPE!");
    break;
  }
}

byte get_ignored_index(int arr[], int id) {
  for (byte i = 0; i < max_players; i++) {
    if (arr[i] == id)
      return i;
  }
  return -1;
}

bool is_ignored(int arr[], int id) { return get_ignored_index(arr, id) != -1; }

void add_to_ignore(int arr[], int id) {
  if (is_ignored(arr, id))
    return;
  for (byte i = 0; i < max_players; i++) {
    if (arr[i] == -1) {
      arr[i] = id;
      return;
    }
  }
}

void remove_from_ignored(int arr[], int id) {
  if (!is_ignored(arr, id))
    return;
  for (byte i = 0; i < max_players; i++) {
    if (arr[i] == id) {
      arr[i] = -1;
      return;
    }
  }
}

static void readInt(WiFiClient client, int* target){
  uint8_t buf[sizeof(int)];
  client.readBytes(buf, sizeof(int));
  int ret_val;
  memcpy(target, buf, sizeof(int));
}

static void readPos(WiFiClient client, int* target){
  uint8_t buf[sizeof(int)];
  client.readBytes(buf, sizeof(int));
  int ret_val;
  memcpy(target, buf, sizeof(int));
}