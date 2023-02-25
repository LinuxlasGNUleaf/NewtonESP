#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{
  id = -1;
  update_flag = false;

  players_index = 0;
  planets_index = 0;

  players = new Player[max_players];
  planets = new Planet[max_planets];
  ignored = new int[max_players];

  for (byte i = 0; i < max_players; i++)
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

  while (true)
  {
    Serial.printf("CONN: Connecting to '%s'...", ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.printf("done.\nCONN: Connecting to Server at %s:%d...", server, port);
    while (!client.connect(server, port))
    {
      Serial.print(".");
      delay(10);
    }
    Serial.println("CONN: connected.");

    // set name, dump recv buffer and enable buffer mode
    client.printf("n %s\n", name);
    client.setTimeout(discard_timeout);
    delay(50);
    int bytes_available = client.peekAvailable();
    if (bytes_available > 0)
    {
      Serial.printf("CONN: Response received, discarding %d bytes.\n", bytes_available);
      discard_bytes(client, client.peekAvailable());
      break;
    }
    else
    {
      Serial.printf("CONN: No response received, attempting to reconnect in %ds.\n",reconnect_wait/1000);
      client.stop();
      WiFi.disconnect(false);
      delay(reconnect_wait);
    }
  }
  client.printf("b %d\n", version);
  discard_bytes(client, 2);
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
  recv_int(client, &msg_type);
  recv_int(client, &payload);

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
    remove_from_ignored(ignored, payload);
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
        remove_from_ignored(ignored, payload);
      }
    }

    // copy player info to array
    memcpy(&players[payload], &payload, sizeof(bool));
    memcpy(&players[payload] + 2* sizeof(float), pos_buf, 2* sizeof(float));
    update_flag = true;
    break;

  case 4: // shot finished, DEPRECATED
    Serial.println("RECV: wrong bot protocol, this msg_type (4) should not be received!");
    while (1)
      ;
    break;

  case 5: // shot begin
    Serial.println("RECV: shot launched, discarding data.");
    // angle, velocity = self.connection.receive_struct("dd")
    client.readBytes(pos_buf, 2*sizeof(double));
    break;

  case 6: // shot end
    Serial.println("RECV: shot ended, discarding data.");
    discard_bytes(client, 4 * sizeof(double));
    // receive number of segments & discard segments
    int n;
    recv_int(client, &n);
    discard_bytes(client, n * 2 * sizeof(float));
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
    int byte_count;
    recv_int(client, &byte_count);

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

byte get_ignored_index(int arr[], int id)
{
  for (byte i = 0; i < max_players; i++)
  {
    if (arr[i] == id)
      return i;
  }
  return -1;
}

bool is_ignored(int arr[], int id) { return get_ignored_index(arr, id) != -1; }

void add_to_ignore(int arr[], int id)
{
  if (is_ignored(arr, id))
    return;
  for (byte i = 0; i < max_players; i++)
  {
    if (arr[i] == -1)
    {
      arr[i] = id;
      return;
    }
  }
}

void remove_from_ignored(int arr[], int id)
{
  if (!is_ignored(arr, id))
    return;
  for (byte i = 0; i < max_players; i++)
  {
    if (arr[i] == id)
    {
      arr[i] = -1;
      return;
    }
  }
}

void recv_int(WiFiClient client, int *target)
{
  client.setTimeout(recv_timeout);
  uint8_t buf[sizeof(int)];
  client.readBytes(buf, sizeof(int));
  memcpy(target, buf, sizeof(int));
}

void discard_bytes(WiFiClient client, unsigned int byte_count)
{
  client.setTimeout(discard_timeout);
  uint8_t buf;
  for (unsigned int i = 0; i < byte_count; i++)
    client.readBytes(&buf, 1);
}