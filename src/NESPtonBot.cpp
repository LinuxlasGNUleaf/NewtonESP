#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{
    id = -1;
    update_flag = false;
    idle = false;

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
        removeFromIgnored(ignored, payload);

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
        // angle, power = self.connection.receive_struct("dd")
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

double NESPtonBot::simShot(uint8_t target_id, double angle, double power)
{
    Vec2d target_pos, self_pos, pos;
    target_pos = players[target_id].position;
    self_pos = players[id].position;
    memcpy(&pos, &self_pos, sizeof(Vec2d));

    double min_dist = sqrt((pos.x - target_pos.x) * (pos.x - target_pos.x) + (pos.y - target_pos.y) * (pos.y - target_pos.y));
    bool left_source = true; // false;

    Vec2d temp, velocity;
    velocity.x = power * cos(angle);
    velocity.y = power * -sin(angle);

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
            add(&velocity, &velocity, &temp);
        }
        div(&temp, &velocity, segment_steps);
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

Vec2d NESPtonBot::scanFor(uint8_t target_id, bool *success)
{
    update_flag = false;
    *success = false;
    // scan different power levels
    double power = power_default;
    for (int pow_i = 0; pow_i < power_count; pow_i++)
    {
        power = power_default + power_changes[pow_i];

        Serial.printf("Starting broad scan with pow=%2.0f...\n", power);
        // broad scan of all angles
        double results[broad_steps];

        double angle_inc = (2 * PI) / broad_steps;
        for (int ang_i = 0; ang_i < broad_steps; ang_i++)
        {
            results[ang_i] = simShot(target_id, angle_inc * ang_i, power);
        }

        double best_angles[broad_test_candidates];
        int skip_after = -1;
        // repeat for each test candidate
        for (int best_i = 0; best_i < broad_test_candidates; best_i++)
        {
            if (skip_after >= 0)
            {
                best_angles[best_i] = -1;
                continue;
            }
            // find the lowest distance in array
            double current_min = results[0];
            for (int res_i = 1; res_i < broad_test_candidates; res_i++)
            {
                if (results[res_i] < current_min)
                {
                    current_min = results[res_i];
                    best_i = res_i;
                }
            }
            // if the lowest value is above broad_distance_max, abandon search
            if (current_min > broad_distance_max)
            {
                best_angles[best_i] = -1;
                skip_after = best_i;
            }
            // save the best current angle to best_angles and "delete" it from the results array
            best_angles[best_i] = angle_inc * best_i;
            results[best_i] = INFINITY;
        }
        int test_angle_count = (skip_after >= 0) ? skip_after + 1 : broad_test_candidates;

        if (skip_after == 0)
        {
            Serial.printf("Broad scan with pow=%2.0f yielded no results.\n", power);
            continue;
        }
        else
        {
            Serial.printf("Broad scan with pow=%2.0f yielded %d viable angles.\n", power, test_angle_count);
            if (checkForRelevantUpdate(target_id))
            {
                Serial.println("Relevant update occured, aborting scan.\n");
                return Vec2d();
            }
        }

        for (int test_i = 0; test_i < test_angle_count; test_i++)
        {
            // sweep test angle
            double test_start_angle = best_angles[test_i] - angle_inc;
            double test_angle_inc = (2 * angle_inc) / fine_steps;
            double angle;
            for (int sweep_i = 0; sweep_i < fine_steps; sweep_i++)
            {
                angle = test_start_angle + test_angle_inc * sweep_i;
                if (simShot(target_id, angle, power) == 0)
                {
                    Serial.printf("Found trajectory with these parameters: pow=%2.0f, angle=%5.2f/n", power, angle);
                    Vec2d ret;
                    ret.x = power;
                    ret.y = angle;
                    if (checkForRelevantUpdate(target_id))
                    {
                        Serial.println("Relevant update occured, aborting scan.\n");
                        return Vec2d();
                    }
                    *success = true;
                    return ret;
                }
            }
        }
    }
    Serial.printf("Failed to find a trajectory for player %d, ignoring this palyer until update.\n", target_id);
    addToIgnored(ignored, target_id);
    return Vec2d();
}

bool NESPtonBot::checkForRelevantUpdate(uint8_t target_id)
{
    Vec2d old_pos = players[target_id].position;
    while (client.peekBuffer() != 0)
        processRecv();

    // field changed or player inactive
    if (update_flag || !players[target_id].active)
        return true;

    // player changed position
    if (players[target_id].position.x != old_pos.x || players[target_id].position.y != old_pos.y)
        return true;

    return false;
}

void NESPtonBot::targetPlayers()
{
    double min_dist = INFINITY;
    Vec2d temp;
    int target_id = -1;
    for (int i = 0; i < max_players; i++)
    {
        Serial.print(ignored[i]);
    }
    Serial.println();
    for (int i = 0; i < max_players; i++)
    {
        if (id == i || players[i].active)
            continue;
        if (isIgnored(ignored, i))
        {
            // Serial.printf("Player %d is on the ignored list.\n", i);
            continue;
        }
        Serial.print('.');
        sub(&temp, &players[i].position, &players[id].position);
        if (norm(&temp) < min_dist)
        {
            min_dist = norm(&temp);
            target_id = i;
        }
    }
    if (target_id == -1)
    {
        if (idle)
            return;
        idle = true;
        Serial.println("No viable opponents.");
        return;
    }
    idle = false;
    bool success;
    Serial.printf("Starting scan for player %d.\n", target_id);
    Vec2d launch_params = scanFor(target_id, &success);
    if (!success)
        return;
    client.printf("v %f\n%f\n", launch_params.x, launch_params.y);
    addToIgnored(ignored, target_id);
}