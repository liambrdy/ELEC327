// expected: 15
// Designated struct initializer; s.ret is overwritten to RET_SUCCESS(15)
typedef struct ret_t {
    int ret;
    int code;
} ret_t;

typedef enum ret_code {
    RET_SUCCESS = 15,
    RET_FAIL,
} ret_code;

int main() {
    ret_t s = { .ret = 0, .code = 1 };
    s.ret = RET_SUCCESS;
    return s.ret;
}
