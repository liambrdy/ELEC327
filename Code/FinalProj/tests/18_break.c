// expected: 3
// Sums i=1,2 then breaks when i==3.  1+2=3.
int main() {
    int i = 1;
    int s = 0;
    while (i <= 10) {
        if (i == 3) break;
        s = s + i;
        i = i + 1;
    }
    return s;
}
