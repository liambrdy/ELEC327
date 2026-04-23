// expected: 17
// Compound assignment (+=) on a struct field.
typedef struct counter_t {
    int value;
    int step;
} counter_t;

int main() {
    counter_t c;
    c.value = 5;
    c.step  = 3;
    c.value += c.step;   // 5+3=8
    c.value += 9;        // 8+9=17
    return c.value;
}
