#include <stdio.h>
#include <string.h>

#include "build/include/sdkconfig.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

char* hex = "0123456789abcdef";

int hexlookup(char h)
{
  for (int i = 0; i < 16; ++i)
  {
    if (hex[i] == h)
    {
      return i;
    }
  }
  return 0;
}

#define LENGTH 8

int main(int argc, char* argv[])
{
  char mac[LENGTH + 1];
  char password[LENGTH + 1];
  char secret[LENGTH + 1];

  if (argc < 2)
  {
    return -1;
  }

  memset(secret, 0, sizeof(secret));
  memcpy(secret, CONFIG_ESP_WIFI_AP_PASSWORD, LENGTH);

  for (int password_idx = 1; password_idx < argc; ++password_idx)
  {
    strncpy(mac, argv[password_idx], LENGTH);

    memset(password, 0, sizeof(password));
    for (int i = 0; i < LENGTH; ++i)
    {
      password[i] = hex[hexlookup(mac[i]) ^ hexlookup(secret[i])];
    }

    printf("MAC: %s Password: %s\n", mac, password);
  }
}
