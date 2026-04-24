// expected: 2
// (3<5)=1, (3==5)=0, (3!=5)=1  ->  1+0+1 = 2
int main() {
    int a = 3;
    int b = 5;
    return (a < b) + (a == b) + (a != b);
}
