// Integers
#define PORT 42155
#define FAIL -1
#define FILE_CHUNK 2048
#define SOCK_CHUNK 16000
// Socket settings
#define LOCALHOST "127.0.0.1"
// Path and file names
#define APP_DATA "/var/lib/zecure"
#define ID_DIR "/var/lib/zecure/login"
#define ID_FNAME "id.txt"
#define MKDIR_STR "/var/lib/zecure,/var/lib/zecure/login"
// Request strings
#define UPLOAD_REQ "UPLOAD"
#define DOWNLOAD_REQ "DOWNLOAD"
#define ISDIR_REQ "ISDIR"
#define AUTH_REQ "AUTH"
#define TREE_A_REQ "TREEA"
#define DELETE_REQ "DEL"
// Response codes
#define ACCESS_OK "00"
#define ACCESS_WP "01"
#define NEW_USER "02"
#define FOLDER_EXISTS "03"
#define ACCESS_FAIL "90"
#define LISTENING "10"
// Walk script
#define WALK "walk"
