// REQUIRED_ARGS: -w

import gcc.builtins;

char *bug260(char *buffer)
{
  return __builtin_strcat(&buffer[0], "Li");
}
