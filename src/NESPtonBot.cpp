#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{
    id = 255;
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

    target_id = 255;
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

    act_conn = conn_count;
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
        float x, y;
        memcpy(&x, pos_buf, sizeof(float));
        memcpy(&y, pos_buf + sizeof(float), sizeof(float));

        // copy player info to array
        players[payload].position.x = x;
        players[payload].position.y = y;

        if (!players[payload].active)
        {
            // player is not yet registered -> joined
            Serial.printf("RECV: Player %d joined the game at (%4.0f,%4.0f)\n", payload, x, y);
            players[payload].active = true;
        }
        else
        {
            // player is already registered -> moved
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
        // Serial.println("RECV: shot launched, discarding data.");
        // angle, power = self.connection.receive_struct("dd")
        client.readBytes(pos_buf, 2 * sizeof(double));
        break;

    case 6: // shot end
        // Serial.println("RECV: shot ended, discarding data.");
        discardBytes(&client, 2 * sizeof(double));
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
            planet_mults[i] = new_planet.mass / segment_steps;
        }
        Serial.printf("RECV: planet data for %d planets received\n", payload);
        break;

    default:
        Serial.printf("RECV: UNKNOWN MSG_TYPE (%d)!\n", msg_type);
        break;
    }
}

double NESPtonBot::simShot(double power, double angle, bool approx)
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
    Serial.printf("angle: %4.0f°, ", degrees(angle));
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
                Serial.printf("[ ) ], dist: [%6.1f] [%d]\n", sqrt(min_r2), approx);
                return min_r2;
            }
            if (approx)
                mult = Q_rsqrt(r2);
            else
                mult = 1 / sqrt(r2);

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
            Serial.printf("[ X ], dist: [%6.1f] [%d]\n", 0.0f, approx);
            return 0;
        } else if (r2 > r2_abort_mult*min_r2) {
            Serial.printf("[ %% ], dist: [%6.1f] [%d]\n", sqrt(min_r2), approx);
            return min_r2;
        }

        sub(&temp, &self_pos, &pos);
        r2 = temp.x * temp.x + temp.y * temp.y;
        if (r2 > (player_size + 1.0) * (player_size + 1.0))
        {
            left_source = true;
        }

        if ((pos.x < -margin) || (pos.x > battlefieldW + margin) || (pos.y < -margin) || (pos.y > battlefieldH + margin))
        {
            Serial.printf("[ O ], dist: [%6.1f] [%d]\n", sqrt(min_r2), approx);
            return min_r2;
        }
    }
    Serial.printf("[ . ], dist: [%6.1f] [%d]\n", sqrt(min_r2), approx);
    return min_r2;
}

void NESPtonBot::scanFor(bool *success, Vec2d *launch_params)
{
    update_flag = false;
    *success = false;

    // calculate initial attack angle
    Vec2d target_vec;
    sub(&target_vec, &players[target_id].position, &players[id].position);
    Serial.printf("INITIAL DISTANCE: %5.1f\n", sqrt(radius_sq(&target_vec)));
    double angle, max_angle, min_angle, distance_h, distance_l, d_angle, last_distance_h, last_distance_l;
    bool approx;

    angle = ang(&target_vec);
    last_distance_h = sqrt(radius_sq(&target_vec));
    last_distance_l = last_distance_h;
    for (launch_params->x = min_power; launch_params->x <= max_power; launch_params->x += power_inc)
    {
        Serial.printf("Iterating for power=%4.1f\n", launch_params->x);
        angle = ang(&target_vec);
        max_angle = angle + PI;
        min_angle = angle - PI;
        d_angle = PI / d_angle_divisor;
        distance_h = last_distance_h;
        distance_l = last_distance_l;
        approx = true;

        for (int i = 0; i < max_iterations; i++)
        {
            // check for field change that would make simulation invalid
            if (checkForAbortCondition())
                return;

            d_angle = (max_angle - min_angle) / (d_angle_divisor * 2);
            angle = (max_angle + min_angle) * 0.5f;

            // high shot
            Serial.printf("[%2d] [%6.1f, %6.1f] ", i, degrees(min_angle), degrees(max_angle));
            distance_h = simShot(launch_params->x, angle + d_angle, approx);
            // if this is the first shot that crosses the "no-approximations" threshold, redo the calculations
            if (distance_h <= approx_target_threshold && approx)
            {
                Serial.println("Recalculating without approximations...");
                Serial.printf("[%2d] [%6.1f, %6.1f] ", i, degrees(min_angle), degrees(max_angle));
                distance_h = simShot(launch_params->x, angle + d_angle, false);
                approx = false;
                i = 0;
            }

            // low shot
            Serial.printf("[%2d] [%6.1f, %6.1f] ", i, degrees(min_angle), degrees(max_angle));
            distance_l = simShot(launch_params->x, angle - d_angle, approx);
            // if this is the first shot that crosses the "no-approximations" threshold, redo the calculations
            if (distance_l <= approx_target_threshold && approx)
            {
                Serial.println("Recalculating without approximations...");
                Serial.printf("[%2d] [%6.1f, %6.1f] ", i, degrees(min_angle), degrees(max_angle));
                distance_l = simShot(launch_params->x, angle - d_angle, false);
                approx = false;
                i = 0;
            }


            if (distance_h > distance_l)
            {
                // we were too high
                max_angle = angle;
            }
            else
            {
                // we were too low
                min_angle = angle;
            }

            last_distance_h = distance_h;
            last_distance_l = distance_l;

            if (checkForAbortCondition())
                return;

            if (approx)
                continue;

            if (distance_h == 0)
            {
                *success = true;
                launch_params->y = angle + d_angle;
                return;
            }

            if (distance_l == 0)
            {
                *success = true;
                launch_params->y = angle - d_angle;
                return;
            }
        }
        Serial.println("No trajectory found for this power.");
    }
    Serial.println("No trajectory found, abandoning this target.");
}

bool NESPtonBot::checkForAbortCondition()
{
    Vec2d old_target, old_self;
    memcpy(&old_target, &players[target_id].position, sizeof(Vec2d));
    memcpy(&old_self, &players[id].position, sizeof(Vec2d));

    while (client.peekAvailable() != 0)
        processRecv();

    // field changed or player inactive
    if (update_flag){
        Serial.println("Aborting scan due to field change.");
        return true;
    }

    // target changed position
    if (!(players[target_id].active && players[target_id].position.x == old_target.x && players[target_id].position.y == old_target.y)){
        Serial.println("Aborting scan due to opponent state change.");
        return true;
    }


    // bot changed position
    if (!(players[id].position.x == old_self.x && players[id].position.y == old_self.y)){
        Serial.println("Aborting scan due to own position change.");
        return true;
    }
    return false;
}

void NESPtonBot::targetPlayers()
{
    double min_r2 = INFINITY;
    Vec2d temp;
    double temp_dist;

    target_id = 255;
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
    if (target_id == 255)
        return;
    bool success;
    Serial.printf("begin scan for player %d\n", target_id);
    Vec2d launch_params;
    launch_params.x = 0;
    launch_params.y = 0;
    scanFor(&success, &launch_params);
    if (!success)
        return;
    client.printf("v %2.1f\n%f\n", launch_params.x, degrees(launch_params.y));
    client.flush();
    addToIgnored(ignored, target_id);
}