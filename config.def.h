/* rirc configuration header
 *
 * Colours can be set [0, 255], Any other value (e.g. -1) will set
 * the default terminal foreground/background */

/* Default comma separated set of Nicks to try on connection
 *   String
 *   ("": defaults to effective user id name)
 */
#define DEFAULT_NICK_SET ""

/* Default Username and Realname sent during connection
 *   String
 *   ("": defaults to effective user id name)
 */
#define DEFAULT_USERNAME ""
#define DEFAULT_REALNAME ""

/* User count in channel before filtering message types
 *   Integer
 *   (0: no filtering) */
#define JOIN_THRESHOLD 0
#define PART_THRESHOLD 0
#define QUIT_THRESHOLD 0
#define ACCOUNT_THRESHOLD 0

/* Message sent for PART and QUIT by default */
#define DEFAULT_QUIT_MESG "rirc v" VERSION
#define DEFAULT_PART_MESG "rirc v" VERSION

#define BUFFER_LINE_HEADER_FG_NEUTRAL 239

#define BUFFER_LINE_HEADER_FG_PINGED  250
#define BUFFER_LINE_HEADER_BG_PINGED  1

#define BUFFER_LINE_TEXT_FG_NEUTRAL 250
#define BUFFER_LINE_TEXT_FG_GREEN   113

/* Number of buffer lines to keep in history, must be power of 2 */
#define BUFFER_LINES_MAX (1 << 10)

/* Colours used for nicks */
#define NICK_COLOURS {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

/* Colours for nav channel names in response to activity, in order of precedence */
#define ACTIVITY_COLOURS {    \
    239, /* Default */        \
    242, /* Join/Part/Quit */ \
    247, /* Chat */           \
    3    /* Ping */           \
};

#define NAV_CURRENT_CHAN 255

/* Characters */
#define QUOTE_CHAR '>'
#define HORIZONTAL_SEPARATOR "-"
#define VERTICAL_SEPARATOR "~"

/* Prefix string for the input line and colours */
#define INPUT_PREFIX " >>> "
#define INPUT_PREFIX_FG 239
#define INPUT_PREFIX_BG -1

/* Input line text colours */
#define INPUT_FG 250
#define INPUT_BG -1

/* BUFFER_PADDING:
 * How the buffer line headers will be padded, options are 0, 1
 *
 * 0 (Unpadded):
 *   12:34 alice ~ hello
 *   12:34 bob ~ is there anybody in there?
 *   12:34 charlie ~ just nod if you can hear me
 *
 * 1 (Padded):
 *   12:34   alice ~ hello
 *   12:34     bob ~ is there anybody in there?
 *   12:34 charlie ~ just nod if you can hear me
 */
#define BUFFER_PADDING 1

/* Raise terminal bell when pinged in chat */
#define BELL_ON_PINGED 1

/* [NETWORK] */

#define CA_CERT_PATH "/etc/ssl/certs/"

/* Seconds before displaying ping
 *   Integer, [0, 150, 86400]
 *   (0: no ping handling) */
#define IO_PING_MIN 150

/* Seconds between refreshing ping display
 *   Integer, [0, 5, 86400]
 *   (0: no ping handling) */
#define IO_PING_REFRESH 5

/* Seconds before timeout reconnect
 *   Integer, [0, 300, 86400]
 *   (0: no ping timeout reconnect) */
#define IO_PING_MAX 300

/* Reconnect backoff base delay
 *   Integer, [1, 4, 86400] */
#define IO_RECONNECT_BACKOFF_BASE 4

/* Reconnect backoff growth factor
 *   Integer, [1, 2, 32] */
#define IO_RECONNECT_BACKOFF_FACTOR 2

/* Reconnect backoff maximum
 *   Integer, [1, 86400, 86400] */
#define IO_RECONNECT_BACKOFF_MAX 86400
