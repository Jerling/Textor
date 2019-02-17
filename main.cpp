#include "include/textor.h"

int main(int argc, char *argv[]) {
  auto textor = textor::editor();
  if (argc > 1)
    return textor.run(argv[1]);
  else
    return textor.run();
}
