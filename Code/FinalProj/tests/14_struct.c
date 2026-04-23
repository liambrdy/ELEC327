// expected: 15
typedef struct point_t {
    int x;
    int y;
} point_t;

int main() {
    point_t p;
    p.x = 7;
    p.y = 8;
    return p.x + p.y;
}
