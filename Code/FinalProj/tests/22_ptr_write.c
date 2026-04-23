// expected: 99
// Write through a pointer (*p = v) and read back via the global.
int g;

int set(int *p, int v) {
    *p = v;
    return v;
}

int main() {
    set(&g, 99);
    return g;
}
