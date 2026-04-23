// expected: 42
// Array name passed as pointer to a function; function reads element 0.

int read_first(int *p) {
    return p[0];
}

int main() {
    int arr[3];
    arr[0] = 42;
    arr[1] = 7;
    arr[2] = 99;
    return read_first(arr);
}
