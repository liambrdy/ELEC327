// expected: 15
// Sum 1+2+3+4+5 = 15
int main() {
    int i = 1;
    int s = 0;
    while (i <= 5) {
        s = s + i;
        i = i + 1;
    }
    return s;
}
