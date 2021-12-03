
bool string_compare(const char *fst, const char *snd) {
  int i = 0;
  for (; fst[i] != '\0' && snd[i] != '\0'; i++) {
    if (fst[i] != snd[i]) {
      return false;
    }
  }
  return fst[i] == snd[i];
}
