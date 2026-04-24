// expected: 15
// Sum 1+2+3+4+5 = 15 using a for loop
int main() {
    int s = 0;
    int i = 0;
    for (i = 1; i <= 5; i = i + 1) {
        s = s + i;
    }
    return s;
}
