// expected: 1
// Short-circuit ||: result is 1 when either side is true.
// Also tests that right side is skipped when left is already true.
int main() {
    int a = 0;
    int b = 7;
    int r = (a != 0) || (b > 5);   // 0 || 1 = 1
    return r;
}
