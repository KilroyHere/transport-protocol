const int RWND_BYTES = 51200;
const int RWND = 10;
const int INIT_SERVER_SEQ_NUM = 4321;
const int INIT_CLIENT_SEQ_NUM = 0;
const int MAX_TIMEOUT_WAIT = 10; //seconds
const int MAX_PACKET_LENGTH = 524;
const int MAX_PAYLOAD_LENGTH = 512;
const int MAX_SEQ_NUM = 102400;
const int MAX_ACK_NUM = 102400;
const int HEADER_LEN = 12;


// Connection States 
int const FIN_RECEIVED = -1;
int const AWAITING_ACK = 0;
int const CONNECTION_ESTABLISHED = 1;

//TCP CONSTANTS
