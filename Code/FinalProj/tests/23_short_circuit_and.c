// expected: 1
// Short-circuit &&: second operand must not be evaluated when first is false.
// If && were bitwise AND this would give wrong results for non-boolean values.
int main() {
    int a = 5;
    int b = 3;
    int r = (a > 2) && (b < 10);   // 1 && 1 = 1
    return r;
}
