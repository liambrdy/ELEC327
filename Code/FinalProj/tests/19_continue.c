// expected: 9
// Sum i=1..6, skip even numbers.  1+3+5=9.
int main() {
    int i = 0;
    int s = 0;
    for (i = 1; i <= 6; i = i + 1) {
        if (i % 2 == 0) continue;
        s = s + i;
    }
    return s;
}
