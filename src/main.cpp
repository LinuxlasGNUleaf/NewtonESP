#include <NESPtonBot.h>

NESPtonBot bot;

void setup()
{
    bot.init();
    bot.connect();
}

void loop()
{
    bot.processRecv();
    delay(50);
}