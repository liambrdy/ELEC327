// expected: 10
// Designated array initializer using [i] = v syntax.
int main() {
    int arr[5] = {[1] = 3, [3] = 7};
    return arr[1] + arr[3];
}
