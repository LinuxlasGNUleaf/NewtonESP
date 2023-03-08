#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{
  id = -1;
  update_flag = false;

  players_index = 0;
  planets_index = 0;

  players = new Player[max_players];
  planets = new Planet[num_planets];
  ignored = new int[max_players];

  for (uint8_t i = 0; i < max_players; i++)
  {
    ignored[i] = -1;
    players[i].active = false;
  }
}

void NESPtonBot::init() { Serial.begin(115200); }

void NESPtonBot::connect()
{
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("CONN: WiFi shield not present! Abort.");
    while (true)
      ;
  }

  act_conn = -1;
  while (true)
  {
    act_conn = (act_conn + 1) % conn_count;
    if (WiFi.scanNetworks(false, false, 0, (uint8 *)ssid[act_conn]) == 0)
    {
      Serial.printf("CONN: Network '%s' not reachable. Skipping...\n", ssid[act_conn]);
      continue;
    }
    Serial.printf("CONN: Connecting to NETWORK '%s'...", ssid[act_conn]);
    WiFi.begin(ssid[act_conn], pass[act_conn]);
    unsigned int start_time = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_time < conn_establish_timeout)
      delay(50);
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.printf("failed after %ds.\n", conn_establish_timeout / 1000);
      continue;
    }

    Serial.printf("done.\nCONN: Connecting to SERVER at %s:%d...", server[act_conn], port);
    start_time = millis();
    while (!client.connect(server[act_conn], port) && millis() - start_time < conn_establish_timeout)
      delay(50);
    if (!client.connected())
    {
      Serial.printf("failed after %ds.\n", conn_establish_timeout / 1000);
      continue;
    }
    Serial.println("connected.");

    // set name, dump recv buffer and enable buffer mode
    client.printf("n %s\n", name);
    client.setTimeout(discard_timeout);
    delay(50);
    int bytes_available = client.peekAvailable();
    if (bytes_available > 0)
    {
      Serial.printf("CONN: Response received, discarding %d bytes.\n", bytes_available);
      discardBytes(&client, client.peekAvailable());
      break;
    }
    else
    {
      Serial.printf("CONN: No response received, attempting to reconnect in %ds.\n", reconnect_wait / 1000);
      client.stop();
      WiFi.disconnect(false);
      delay(reconnect_wait);
    }
  }
  client.printf("b %d\n", version);
  discardBytes(&client, 2);
  Serial.println();
}

void NESPtonBot::processRecv()
{
  // receive msg_id and payload
  client.setTimeout(recv_timeout);
  if (!client.peekAvailable())
  {
    return;
  }
  // extract from buffer
  int msg_type, payload;
  recvInt(&client, &msg_type);
  recvInt(&client, &payload);

  switch (msg_type)
  {
  case 1: // bot join
    id = payload;
    update_flag = true;
    Serial.printf("RECV: Own ID (%d) received.\n", id);
    break;

  case 2: // player leave
    // mark player inactive
    players[payload].active = false;
    removeFromIgnored(ignored, payload);
    Serial.printf("RECV: Player %d left the game.\n", payload);
    break;

  case 3: // player joined / moved
    uint8_t pos_buf[2 * sizeof(float)];
    client.readBytes(pos_buf, 2 * sizeof(float));

    if (!players[payload].active)
    {
      // player is not yet registered -> joined
      Serial.printf("RECV: Player %d joined the game at (%d,%d)\n", payload,
                    (int)round(pos_buf[0]), (int)round(pos_buf[sizeof(float)]));
      players[payload].active = true;
    }
    else
    {
      // player is already registered -> moved
      Serial.printf("RECV: Player %d moved to (%d,%d)\n", payload, (int)round(pos_buf[0]),
                    (int)round(pos_buf[sizeof(float)]));

      // was bot moved?
      if (payload == id)
      {
        for (int i = 0; i < max_players; i++)
          ignored[i] = -1;
      }
      else
      {
        // try to remove opponent from ignored list
        removeFromIgnored(ignored, payload);
      }
    }

    // copy player info to array
    memcpy(&players[payload], &payload, sizeof(bool));
    memcpy(&players[payload] + 2 * sizeof(float), pos_buf, 2 * sizeof(float));
    break;

  case 4: // shot finished, DEPRECATED
    Serial.println("RECV: wrong bot protocol, this msg_type (4) should not be received!");
    while (1)
      ;
    break;

  case 5: // shot begin
    Serial.println("RECV: shot launched, discarding data.");
    // angle, velocity = self.connection.receive_struct("dd")
    client.readBytes(pos_buf, 2 * sizeof(double));
    break;

  case 6: // shot end
    Serial.println("RECV: shot ended, discarding data.");
    discardBytes(&client, 4 * sizeof(double));
    // receive number of segments & discard segments
    int n;
    recvInt(&client, &n);
    discardBytes(&client, n * 2 * sizeof(float));
    break;

  case 7: // game mode, deprecated
    Serial.println("RECV: wrong bot protocol, this msg_type (7) should not be received!");
    while (1)
      ;
    break;

  case 8: // own energy
    uint8_t energy_buf[sizeof(double)];
    client.readBytes(energy_buf, sizeof(double));
    memcpy(&energy, energy_buf, sizeof(double));
    Serial.printf("RECV: own energy updated to %f.\n", energy);
    break;

  case 9: // planet info
    update_flag = true;
    int byte_count;
    recvInt(&client, &byte_count);

    Planet new_planet;
    uint8_t new_planet_buf[4 * sizeof(double)];
    for (int i = 0; i < payload; i++)
    {
      client.readBytes(new_planet_buf, 4 * sizeof(double));
      memcpy(&new_planet, new_planet_buf, 4 * sizeof(double));
      planets[i] = new_planet;
    }
    Serial.printf("RECV: planet data for %d planets received\n", payload);
    break;

  default:
    Serial.printf("RECV: UNKNOWN MSG_TYPE (%d)!\n", msg_type);
    break;
  }
}

double NESPtonBot::simShot(uint8_t target_id, double angle, double velocity)
{
  Vec2d target_pos, self_pos, pos;
  target_pos = players[target_id].position;
  self_pos = players[id].position;
  memcpy(&pos, &self_pos, sizeof(Vec2d));

  double min_dist = sqrt((pos.x - target_pos.x) * (pos.x - target_pos.x) + (pos.y - target_pos.y) * (pos.y - target_pos.y));
  bool left_source = true; // false;

  Vec2d temp, speed;
  speed.x = velocity * cos(angle);
  speed.y = velocity * -sin(angle);

  double l;
  double multiplier;
  for (unsigned int seg_i = 0; seg_i < max_segments; seg_i++)
  {

    for (byte planet_i = 0; planet_i < num_planets; planet_i++)
    {
      sub(&temp, &planets[planet_i].position, &pos);
      l = norm(&temp);

      if (l <= planets[planet_i].radius)
      {
        return min_dist;
      }

      multiplier = planets[planet_i].mass / (l * l * l * segment_steps);
      mul(&temp, &temp, multiplier);
      add(&speed, &speed, &temp);
    }
    div(&temp, &speed, segment_steps);
    add(&pos, &pos, &temp);

    sub(&temp, &target_pos, &pos);
    l = norm(&temp);
    min_dist = min(min_dist, l);
    if ((l <= player_size) && left_source)
    {
      return 0;
    }

    /* cutting this check for performance boost
    sub(&temp, &self_pos, &pos);
    l = norm(&temp);
    if (l > (player_size + 1.0))
    {
      left_source = true;
    }
    */

    if ((pos.x < -margin) || (pos.x > battlefieldW + margin) || (pos.y < -margin) || (pos.y > battlefieldH + margin))
    {
      return min_dist;
    }
  }
  return min_dist;
}

uint8_t getIgnoredIndex(int arr[], int id)
{
  for (uint8_t i = 0; i < max_players; i++)
  {
    if (arr[i] == id)
      return i;
  }
  return -1;
}

bool isIgnored(int arr[], int id) { return getIgnoredIndex(arr, id) != -1; }

void addToIgnored(int arr[], int id)
{
  if (isIgnored(arr, id))
    return;
  for (uint8_t i = 0; i < max_players; i++)
  {
    if (arr[i] == -1)
    {
      arr[i] = id;
      return;
    }
  }
}

void removeFromIgnored(int arr[], int id)
{
  if (!isIgnored(arr, id))
    return;
  for (uint8_t i = 0; i < max_players; i++)
  {
    if (arr[i] == id)
    {
      arr[i] = -1;
      return;
    }
  }
}

void recvInt(WiFiClient *client, int *target)
{
  client->setTimeout(recv_timeout);
  uint8_t buf[sizeof(int)];
  client->readBytes(buf, sizeof(int));
  memcpy(target, buf, sizeof(int));
}

void discardBytes(WiFiClient *client, unsigned int byte_count)
{
  client->setTimeout(discard_timeout);
  uint8_t buf = 0;
  for (unsigned int i = 0; i < byte_count; i++)
  {
    client->readBytes(&buf, 1);
  }
}