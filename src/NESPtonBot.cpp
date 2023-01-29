#include <NESPtonBot.h>

NESPtonBot::NESPtonBot(/* args */)
{

}

void NESPtonBot::init()
{
    Serial.begin(115200);
}

void NESPtonBot::connect()
{
    if (WiFi.status() == WL_NO_SHIELD)
    {
        Serial.println("WiFi shield not present! Abort.");
        while (true);
    }
    Serial.print("Connecting to "+String(ssid)+"...");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("done.\nConnecting to Server at ");
    Serial.print(String(server) + ":" + String(port) + "...");
    while (!client.connect(server, port))
    {
        Serial.print(".");
    }
    Serial.println("connected.");

    // set name, dump recv buffer and enable buffer mode
    client.print("n "+String(name)+"\n");
    client.setTimeout(discard_timeout);
    client.readString();
    client.print("b "+String(version)+"\n");
}
