// expected: 7
// arr[i]-- with a variable index.
int main() {
    int arr[3] = {2, 5, 1};
    int i = 1;
    arr[i]--;
    return arr[0] + arr[i] + arr[2];
}
