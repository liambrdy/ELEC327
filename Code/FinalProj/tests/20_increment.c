// expected: 10
// i++ and i-- used as for-update and standalone statements.
int main() {
    int i = 0;
    int j = 5;
    for (i = 0; i < 5; i++) {
        j++;
    }
    return j;
}
