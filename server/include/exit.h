/*Header for server/exit.c file
Exit codes*/
typedef enum exit_code {
    OK, //0
    NOT_ROOT, //1
    BIND_ERR, //2
    LISTEN_ERR, //3
    ABORT, //4
    SSL_KEYS_ERR, //5
    CONFIG_ERR, //6
    BROKEN_META, //7
    WALK_FAIL //8
} exit_code;

void exitm(exit_code e);
void strExit(char buff[], exit_code e);
