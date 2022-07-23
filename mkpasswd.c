#include <stdio.h>
#include <string.h>

#include "build/include/sdkconfig.h"

#define LENGTH 8
#define MIN(a, b) ((a) < (b) ? (a) : (b))

char secret[LENGTH];

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

char buffer[LENGTH + 1];

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    return -1;
  }

  memcpy(secret, CONFIG_ESP_WIFI_AP_PASSWORD, LENGTH);

  for (int password_idx = 1; password_idx < argc; ++password_idx)
  {
    memset(buffer, 0, sizeof(buffer));
    strncpy(buffer, argv[password_idx], LENGTH);

    for (int i = 0; i < LENGTH; ++i)
    {
      int in = hexlookup(argv[password_idx][i]);
      int sec = hexlookup(secret[i]);
      int out = in ^ sec;

      printf("%x", out);
    }

    printf("\n");
  }
}
