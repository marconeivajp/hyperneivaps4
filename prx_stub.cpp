#include <stddef.h>

extern "C" {
    // O PS4 procura essa função quando o PRX é plugado na memória
    int module_start(size_t args, const void *argp) {
        return 0; // 0 significa "Carregado com Sucesso"
    }

    // O PS4 procura essa função quando o PRX é ejetado da memória
    int module_stop(size_t args, const void *argp) {
        return 0;
    }

    // Dummy main para satisfazer o create-fself (raro, mas acontece)
    int main(int argc, char** argv) {
        return 0;
    }
}
