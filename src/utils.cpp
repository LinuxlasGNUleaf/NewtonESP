#include <utils.h>

int getIgnoredIndex(int *arr, int id)
{
  for (int i = 0; i < max_players; i++)
  {
    if (arr[i] == id)
      return i;
  }
  return -1;
}

bool isIgnored(int *arr, int id)
{
  Serial.print(getIgnoredIndex(arr, id));
  return getIgnoredIndex(arr, id) != -1;
}

void addToIgnored(int *arr, int id)
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

void removeFromIgnored(int *arr, int id)
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