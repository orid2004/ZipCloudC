/*Header for client/exit.c file
Exit codes*/
typedef enum exit_code {
    OK,
    NOT_ROOT,
    CONNECT_ERR,
    WALK_FAIL,
    CONFIG_ERR,
    META_ERR
} exit_code;

void exitm(exit_code e);
void strExit(char buff[], exit_code err);
