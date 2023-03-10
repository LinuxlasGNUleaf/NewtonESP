#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{
    id = -1;
    update_flag = false;

    players_index = 0;
    planets_index = 0;

    players = new Player[max_players];
    planets = new Planet[num_planets];
    planet_mults = new double[num_planets];
    ignored = new int[max_players];

    for (uint8_t i = 0; i < max_players; i++)
    {
        ignored[i] = -1;
        players[i].active = false;
    }
}

void NESPtonBot::init()
{
    Serial.begin(115200);
    delay(1500);
    Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz());
}

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
        start_time = millis();
        while (client.peekAvailable() < discard_bytes && millis() < start_time + wait_timeout)
        {
            delay(50);
        }
        if (client.peekAvailable() >= discard_bytes)
        {
            Serial.printf("CONN: Response received, discarding %d bytes.\n", discard_bytes);
            discardBytes(&client, client.peekAvailable());
            break;
        }
        else
        {
            Serial.println("CONN: No response received, giving up.");
            client.stop();
            WiFi.disconnect(false);
        }
    }
    client.printf("b %d\n", version);
    discardBytes(&client, 2);
    Serial.println();
}

void NESPtonBot::processRecv(bool silent)
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
        if (!silent)
            Serial.printf("RECV: Own ID (%d) received.\n", id);
        break;

    case 2: // player leave
        // mark player inactive
        players[payload].active = false;
        removeFromIgnored(ignored, payload);
        if (!silent)
            Serial.printf("RECV: Player %d left the game.\n", payload);
        break;

    case 3: // player joined / moved
        uint8_t pos_buf[2 * sizeof(float)];
        client.readBytes(pos_buf, 2 * sizeof(float));
        float x, y;
        memcpy(&x, pos_buf, sizeof(float));
        memcpy(&y, pos_buf + sizeof(float), sizeof(float));

        // copy player info to array
        players[payload].position.x = x;
        players[payload].position.y = y;

        if (!players[payload].active)
        {
            // player is not yet registered -> joined
            if (!silent)
                Serial.printf("RECV: Player %d joined the game at (%4.0f,%4.0f)\n", payload, x, y);
            players[payload].active = true;
        }
        else
        {
            // player is already registered -> moved
            if (!silent)
                Serial.printf("RECV: Player %d moved to (%4.0f,%4.0f)\n", payload, x, y);

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
        break;

    case 4: // shot finished, DEPRECATED
        Serial.println("RECV: wrong bot protocol, this msg_type (4) should not be received!");
        while (1)
            ;
        break;

    case 5: // shot begin
        if (!silent)
            Serial.println("RECV: shot launched, discarding data.");
        // angle, power = self.connection.receive_struct("dd")
        client.readBytes(pos_buf, 2 * sizeof(double));
        break;

    case 6: // shot end
        if (!silent)
            Serial.println("RECV: shot ended, discarding data.");
        discardBytes(&client, 2 * sizeof(double));
        // receive number of segments & discard segments
        int n;
        recvInt(&client, &n);
        discardBytes(&client, n * 2 * sizeof(float));
        break;

    case 7: // game mode, deprecated
        if (!silent)
            Serial.println("RECV: wrong bot protocol, this msg_type (7) should not be received!");
        while (1)
            ;
        break;

    case 8: // own energy
        uint8_t energy_buf[sizeof(double)];
        client.readBytes(energy_buf, sizeof(double));
        memcpy(&energy, energy_buf, sizeof(double));
        if (!silent)
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
            planet_mults[i] = new_planet.mass / segment_steps;
        }
        if (!silent)
            Serial.printf("RECV: planet data for %d planets received\n", payload);
        break;

    default:
        Serial.printf("RECV: UNKNOWN MSG_TYPE (%d)!\n", msg_type);
        break;
    }
}

double NESPtonBot::simShot(uint8_t target_id, double angle, double power)
{
    Vec2d target_pos, self_pos, pos;
    // copy target position from players array
    memcpy(&target_pos, &players[target_id].position, sizeof(Vec2d));

    // set self_pos and pos to copies of the position of the bot
    memcpy(&self_pos, &players[id].position, sizeof(Vec2d));
    memcpy(&pos, &self_pos, sizeof(Vec2d));

    Vec2d velocity;
    velocity.x = power * cos(angle);
    velocity.y = power * -sin(angle);

    Vec2d temp;
    double min_r2 = (pos.x - target_pos.x) * (pos.x - target_pos.x) + (pos.y - target_pos.y) * (pos.y - target_pos.y);
    Serial.printf("angle: %3.0f??, ", degrees(angle));
    bool left_source = false;

    double r2, mult;
    for (unsigned int seg_i = 0; seg_i < max_segments; seg_i++)
    {
        if (seg_i % 100 == 0)
            ESP.wdtFeed();

        // iterate over planets
        for (byte planet_i = 0; planet_i < num_planets; planet_i++)
        {
            // calculate the difference vectore between pos and the current planet
            sub(&temp, &planets[planet_i].position, &pos);
            r2 = radius_sq(&temp);
            if (r2 <= planets[planet_i].radius * planets[planet_i].radius)
            {
                Serial.print("[ ) ], ");
                return min_r2;
            }
            mult = Q_rsqrt(r2);

            mul(&temp, &temp, mult * mult * mult * planet_mults[planet_i]);
            add(&velocity, &velocity, &temp);
        }
        mul(&temp, &velocity, 1.0f / segment_steps);
        add(&pos, &pos, &temp);

        sub(&temp, &target_pos, &pos);
        r2 = radius_sq(&temp);
        min_r2 = min(min_r2, r2);
        if ((r2 <= player_size * player_size) && left_source)
        {
            Serial.print("[ + ], ");
            return 0;
        }

        sub(&temp, &self_pos, &pos);
        r2 = temp.x * temp.x + temp.y * temp.y;
        if (r2 > (player_size + 1.0) * (player_size + 1.0))
        {
            left_source = true;
        }

        if ((pos.x < -margin) || (pos.x > battlefieldW + margin) || (pos.y < -margin) || (pos.y > battlefieldH + margin))
        {
            Serial.print("[ O ], ");
            return min_r2;
        }
    }
    Serial.print("[ . ], ");
    return min_r2;
}

void NESPtonBot::scanFor(uint8_t target_id, bool *success, Vec2d *launch_params)
{
    update_flag = false;
    *success = false;

    // calculate initial attack angle
    Vec2d target_vec;
    sub(&target_vec, &players[target_id].position, &players[id].position);
    double direct_angle = angle(&target_vec);
    double d_angle = 0;
    while (d_angle < broad_scan_stop)
    {
        d_angle += broad_tangent_inc;
        
    }
    

    return;
}

bool NESPtonBot::checkForRelevantUpdate(uint8_t target_id)
{
    Vec2d old_target, old_self;
    memcpy(&old_target, &players[target_id].position, sizeof(Vec2d));
    memcpy(&old_self, &players[id].position, sizeof(Vec2d));

    while (client.peekAvailable() != 0)
        processRecv(true);

    // field changed or player inactive
    if (update_flag || !players[target_id].active)
        return true;

    // target changed position
    if (players[target_id].position.x != old_target.x || players[target_id].position.y != old_target.y)
        return true;

    // bot changed position
    if (players[id].position.x != old_self.x || players[id].position.y != old_self.y)
        return true;

    return false;
}

void NESPtonBot::targetPlayers()
{
    double min_r2 = INFINITY;
    Vec2d temp;
    double temp_dist;
    int target_id = -1;

    for (uint8_t i = 0; i < max_players; i++)
    {
        if (id == i || !players[i].active || isIgnored(ignored, i))
            continue;

        sub(&temp, &players[i].position, &players[id].position);
        temp_dist = radius_sq(&temp);
        if (temp_dist < min_r2)
        {
            min_r2 = temp_dist;
            target_id = i;
        }
    }
    if (target_id == -1)
        return;
    bool success;
    Serial.printf("scan for player %d.\n", target_id);
    Vec2d launch_params;
    launch_params.x = 0;
    launch_params.y = 0;
    scanFor(target_id, &success, &launch_params);
    if (!success)
        return;
    client.printf("v %2.1f\n%f\n", launch_params.x, degrees(launch_params.y));
    addToIgnored(ignored, target_id);
}