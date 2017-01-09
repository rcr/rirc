/* Rirc configuration header */

/* Colors, 0..255 */
#define BUFFER_LINE_HEADER_FG_NEUTRAL 239

#define BUFFER_LINE_HEADER_FG_PINGED  250
#define BUFFER_LINE_HEADER_BG_PINGED  1

#define BUFFER_LINE_TEXT_FG_NEUTRAL 250
#define BUFFER_LINE_TEXT_FG_GREEN   113

#define INPUT_FG_NEUTRAL 250

static int nick_colours[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

/* Characters */
#define QUOTE_CHAR '>'
#define HORIZONTAL_SEPARATOR "-"
#define VERTICAL_SEPARATOR "~"

/* BUFFER_PAD:
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
 * */
#define BUFFER_PADDING 1

static int actv_cols[ACTIVITY_T_SIZE] = {239, 247, 3};
