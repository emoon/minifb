#pragma once

#include <bios.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/exceptn.h>
#include <sys/farptr.h>

void gdb_start();
void gdb_checkpoint();

#ifdef GDB_IMPLEMENTATION
#ifdef NDEBUG
void gdb_start() {}
void gdb_checkpoint() {}
#else
void gdb_loop(int exception_number);
void gdb_tick_handler();
static unsigned char *gdb_read_packet();
static void gdb_write_packet(unsigned char *buffer);

#ifdef GDB_DEBUG_PRINT
#define gdb_debug(...) printf(__VA_ARGS__)
#else
#define gdb_debug(...)
#endif

#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

#define UART_LINE_CONTROL 3
#define UART_LCR_DIVISOR_LATCH 0x80
#define UART_DIVISOR_LATCH_WORD 0
#define UART_BPS_DIVISOR_115200 1
#define IO_BUFFER_SIZE 1024 * 1024

// clang-format off
enum gdb_register { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, EIP, EFLAGS, CS, SS, DS, ES, FS, GS, NUM_REGISTERS };
static char *register_names[] = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI", "EIP", "EFLAGS", "CS", "SS", "DS", "ES", "FS", "GS"};
static char hex_chars[] = "0123456789abcdef";
// clang-format on

typedef struct gdb_context {
  int registers[NUM_REGISTERS];
  char input_buffer[IO_BUFFER_SIZE];
  char output_buffer[IO_BUFFER_SIZE];
  int mem_error;
  void (*mem_error_callback)();
  int no_ack_mode;
  int was_interrupted;
  _go32_dpmi_seginfo old_tick_handler;
  _go32_dpmi_seginfo tick_handler;
} gdb_context;
gdb_context ctx = {0};
static int handler_mutex;

static void serial_port_init() {
  _bios_serialcom(_COM_INIT, 0,
                  (char)(_COM_9600 | _COM_NOPARITY | _COM_STOP1 | _COM_CHR8));
  unsigned int base = _farpeekw(0x0040, 0);
  outp(base + UART_LINE_CONTROL,
       inp(base + UART_LINE_CONTROL) | UART_LCR_DIVISOR_LATCH);
  outpw(base + UART_DIVISOR_LATCH_WORD, UART_BPS_DIVISOR_115200);
  outp(base + UART_LINE_CONTROL,
       inp(base + UART_LINE_CONTROL) & ~UART_LCR_DIVISOR_LATCH);
}

static void serial_port_putc(char c) { _bios_serialcom(_COM_SEND, 0, c); }

static int serial_port_getc() {
  return _bios_serialcom(_COM_RECEIVE, 0, 0) & 0xff;
}

static void set_mem_error(void) { ctx.mem_error = 1; }

static int hex_to_byte(char ch) {
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch - 'a' + 10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch - 'A' + 10);
  return (-1);
}

static int hex_to_int(char **ptr, int *intValue) {
  int numChars = 0;
  *intValue = 0;

  while (**ptr) {
    int hexValue = hex_to_byte(**ptr);
    if (hexValue >= 0) {
      *intValue = (*intValue << 4) | hexValue;
      numChars++;
    } else
      break;

    (*ptr)++;
  }

  return (numChars);
}

static int mem_get_byte(char *addr) { return *addr; }

static void mem_set_byte(char *addr, int val) { *addr = val; }

static char *mem_to_hex(char *mem, char *buf, int count, int may_fault) {
  if (may_fault)
    ctx.mem_error_callback = set_mem_error;
  for (int i = 0; i < count; i++) {
    unsigned char ch = mem_get_byte(mem++);
    gdb_debug("%x ", ch);
    if (may_fault && ctx.mem_error)
      return (buf);
    *buf++ = hex_chars[ch >> 4];
    *buf++ = hex_chars[ch % 16];
  }
  *buf = 0;
  if (may_fault)
    ctx.mem_error_callback = NULL;
  return (buf);
}

static char *hex_to_mem(char *buf, char *mem, int count, int may_fault) {
  if (may_fault)
    ctx.mem_error_callback = set_mem_error;
  for (int i = 0; i < count; i++) {
    unsigned char ch = hex_to_byte(*buf++) << 4;
    ch = ch + hex_to_byte(*buf++);
    gdb_debug("%x ", ch);
    mem_set_byte(mem++, ch);
    if (may_fault && ctx.mem_error)
      return (mem);
  }
  if (may_fault)
    ctx.mem_error_callback = NULL;
  return (mem);
}

static void exception_save_registers(void) {
  ctx.registers[EAX] = (int)__djgpp_exception_state->__eax;
  ctx.registers[ECX] = (int)__djgpp_exception_state->__ecx;
  ctx.registers[EDX] = (int)__djgpp_exception_state->__edx;
  ctx.registers[EBX] = (int)__djgpp_exception_state->__ebx;
  ctx.registers[ESP] = (int)__djgpp_exception_state->__esp;
  ctx.registers[EBP] = (int)__djgpp_exception_state->__ebp;
  ctx.registers[ESI] = (int)__djgpp_exception_state->__esi;
  ctx.registers[EDI] = (int)__djgpp_exception_state->__edi;
  ctx.registers[EIP] = (int)__djgpp_exception_state->__eip;
  ctx.registers[EFLAGS] = (int)__djgpp_exception_state->__eflags;
  ctx.registers[CS] = (int)__djgpp_exception_state->__cs;
  ctx.registers[SS] = (int)__djgpp_exception_state->__ss;
  ctx.registers[DS] = (int)__djgpp_exception_state->__ds;
  ctx.registers[ES] = (int)__djgpp_exception_state->__es;
  ctx.registers[FS] = (int)__djgpp_exception_state->__fs;
  ctx.registers[GS] = (int)__djgpp_exception_state->__gs;
}

extern void exception_return();
asm(".text");
asm(".globl _exception_return");
asm("_exception_return:");
asm("   movw _ctx+44, %ss");
asm("   movl _ctx+16, %esp");
asm("   movl _ctx+4, %ecx");
asm("   movl _ctx+8, %edx");
asm("   movl _ctx+12, %ebx");
asm("   movl _ctx+20, %ebp");
asm("   movl _ctx+24, %esi");
asm("   movl _ctx+28, %edi");
asm("   movw _ctx+48, %ds");
asm("   movw _ctx+52, %es");
asm("   movw _ctx+56, %fs");
asm("   movw _ctx+60, %gs");
asm("   movl _ctx+36, %eax");
asm("   pushl %eax");
asm("   movl _ctx+40, %eax");
asm("   pushl %eax");
asm("   movl _ctx+32, %eax");
asm("   pushl %eax");
asm("   movl _ctx, %eax");
asm("   iret");

static void exception_sigsegv_handler(int exception_number) {
  exception_save_registers();
  gdb_loop(exception_number);
  exception_return();
}

static void exception_handler(int exception_number) {
  exception_save_registers();
  gdb_loop(exception_number);
  exception_return();
}

static int exception_to_signal(int exception_number) {
  int signal_number;
  switch (exception_number) {
  case 0:
    signal_number = 8;
    break; /* divide by zero */
  case 1:
    signal_number = 5;
    break; /* debug exception */
  case 302:
  case 3:
    signal_number = 5;
    break; /* breakpoint */
  case 4:
    signal_number = 16;
    break; /* into instruction (overflow) */
  case 5:
    signal_number = 16;
    break; /* bound instruction */
  case 6:
    signal_number = 4;
    break; /* Invalid opcode */
  case 7:
    signal_number = 8;
    break; /* coprocessor not available */
  case 8:
    signal_number = 7;
    break; /* double fault */
  case 9:
    signal_number = 11;
    break; /* coprocessor segment overrun */
  case 10:
    signal_number = 11;
    break; /* Invalid TSS */
  case 11:
    signal_number = 11;
    break; /* Segment not present */
  case 12:
    signal_number = 11;
    break; /* stack exception */
  case 13:
    signal_number = 11;
    break; /* general protection */
  case 14:
    signal_number = 11;
    break; /* page fault */
  case 16:
    signal_number = 7;
    break; /* coprocessor error */
  default:
    signal_number = 7; /* "software generated"*/
  }
  return (signal_number);
}

static void exception_init() {
  _go32_dpmi_lock_data(register_names, sizeof(register_names));
  for (int i = 0; i < NUM_REGISTERS; i++)
    _go32_dpmi_lock_data(register_names[0], 3);
  _go32_dpmi_lock_data(hex_chars, sizeof(hex_chars));
  _go32_dpmi_lock_data(&ctx, sizeof(ctx));
  _go32_dpmi_lock_data(&handler_mutex, sizeof(handler_mutex));

  _go32_dpmi_lock_code(serial_port_init, 4096);
  _go32_dpmi_lock_code(serial_port_putc, 4096);
  _go32_dpmi_lock_code(serial_port_getc, 4096);
  _go32_dpmi_lock_code(set_mem_error, 4096);
  _go32_dpmi_lock_code(hex_to_byte, 4096);
  _go32_dpmi_lock_code(hex_to_int, 4096);
  _go32_dpmi_lock_code(mem_get_byte, 4096);
  _go32_dpmi_lock_code(mem_set_byte, 4096);
  _go32_dpmi_lock_code(mem_to_hex, 4096);
  _go32_dpmi_lock_code(hex_to_mem, 4096);
  _go32_dpmi_lock_code(exception_save_registers, 4096);
  _go32_dpmi_lock_code(exception_return, 4096);
  _go32_dpmi_lock_code(exception_sigsegv_handler, 4096);
  _go32_dpmi_lock_code(exception_handler, 4096);
  _go32_dpmi_lock_code(exception_to_signal, 4096);
  _go32_dpmi_lock_code(gdb_start, 4096);
  _go32_dpmi_lock_code(gdb_read_packet, 4096);
  _go32_dpmi_lock_code(gdb_write_packet, 4096);
  _go32_dpmi_lock_code(gdb_loop, 4096);
  _go32_dpmi_lock_code(gdb_checkpoint, 4096);

  signal(SIGSEGV, exception_sigsegv_handler);
  signal(SIGFPE, exception_handler);
  signal(SIGTRAP, exception_handler);
  signal(SIGILL, exception_handler);

  _go32_dpmi_get_protected_mode_interrupt_vector(0x1c, &ctx.old_tick_handler);
  ctx.tick_handler.pm_offset = (int)gdb_tick_handler;
  ctx.tick_handler.pm_selector = _go32_my_cs();
  _go32_dpmi_allocate_iret_wrapper(&ctx.tick_handler);
  _go32_dpmi_set_protected_mode_interrupt_vector(0x1c, &ctx.tick_handler);
}

static void exception_dispose() {
  signal(SIGSEGV, SIG_DFL);
  signal(SIGFPE, SIG_DFL);
  signal(SIGTRAP, SIG_DFL);
  signal(SIGILL, SIG_DFL);

  _go32_dpmi_set_protected_mode_interrupt_vector(0x1c, &ctx.old_tick_handler);
}

void gdb_start(void) {
  ((void)register_names[0]);
  serial_port_init();
  exception_init();
  atexit(exception_dispose);
  asm("int $3");
}

static unsigned char *gdb_read_packet() {
  register unsigned char *buffer = (unsigned char *)ctx.input_buffer;
  register unsigned char checksum;
  register unsigned char xmitcsum;
  register int count;
  register char ch;

  while (1) {
    while ((ch = serial_port_getc()) != '$')
      ;

  retry:
    checksum = 0;
    xmitcsum = -1;
    count = 0;

    while (count < IO_BUFFER_SIZE) {
      ch = serial_port_getc();
      if (ch == '$') {
        gdb_debug("Retrying\n");
        goto retry;
      }
      if (ch == '#') {
        gdb_debug("Found end of packet\n");
        break;
      }
      checksum = checksum + ch;
      buffer[count] = ch;
      count = count + 1;
    }
    buffer[count] = 0;

    if (ch == '#') {
      ch = serial_port_getc();
      xmitcsum = hex_to_byte(ch) << 4;
      ch = serial_port_getc();
      xmitcsum += hex_to_byte(ch);

      if (checksum != xmitcsum) {
        if (!ctx.no_ack_mode)
          serial_port_putc('-');
      } else {
        if (!ctx.no_ack_mode)
          serial_port_putc('+');
        if (buffer[2] == ':') {
          serial_port_putc(buffer[0]);
          serial_port_putc(buffer[1]);
          return &buffer[3];
        }
        return &buffer[0];
      }
    }
  }
}

static void gdb_write_packet(unsigned char *buffer) {
  unsigned char checksum;
  int count;
  char ch;

  do {
    serial_port_putc('$');
    checksum = 0;
    count = 0;

    while ((ch = buffer[count])) {
      serial_port_putc(ch);
      checksum += ch;
      count += 1;
    }

    serial_port_putc('#');
    serial_port_putc(hex_chars[checksum >> 4]);
    serial_port_putc(hex_chars[checksum % 16]);

    if (ctx.no_ack_mode)
      break;
  } while (serial_port_getc() != '+');
}

void gdb_loop(int exception_number) {
  if (handler_mutex)
    return;
  handler_mutex = 1;

  int stepping, addr, length;
  char *ptr;

  /* reply to host that an exception has occurred */
  int sigval = exception_to_signal(exception_number);
  gdb_debug("\n=== STOPPED: sig: %i, evec: %i, ip %p, [ip] %x\n", sigval,
            exception_number, (void *)ctx.registers[EIP],
            *(unsigned char *)ctx.registers[EIP]);
  for (int l = 0; l < NUM_REGISTERS; l++)
    gdb_debug("%s: %x ", register_names[l], ctx.registers[l]);
  gdb_debug("\n");

  ctx.output_buffer[0] = 'S';
  ctx.output_buffer[1] = hex_chars[sigval >> 4];
  ctx.output_buffer[2] = hex_chars[sigval % 16];
  ctx.output_buffer[3] = 0;

  gdb_write_packet((unsigned char *)ctx.output_buffer);

  stepping = 0;

  while (1 == 1) {
    ctx.output_buffer[0] = 0;
    ptr = (char *)gdb_read_packet();
    char cmd = *ptr++;
    switch (cmd) {
    case '?':
      gdb_debug("? (Query the reason the target halted on connect)\n");
      ctx.output_buffer[0] = 'S';
      ctx.output_buffer[1] = hex_chars[sigval >> 4];
      ctx.output_buffer[2] = hex_chars[sigval % 16];
      ctx.output_buffer[3] = 0;
      break;
    case 'D':
      gdb_debug("D (Detach)\n");
      exit(0);
      break;
    case 'H':
      gdb_debug("H (Set thread for subsequent operations)\n");
      strcpy(ctx.output_buffer, "OK");
      break;
    case 'q':
      if (!strcmp(ptr, "C")) {
        gdb_debug("qC (Return the current thread ID.)\n");
        ctx.output_buffer[0] = 'Q';
        ctx.output_buffer[1] = 'C';
        ctx.output_buffer[2] = '0';
        ctx.output_buffer[3] = 0;
      } else if (!strcmp(ptr, "Attached")) {
        gdb_debug("qAttached (Check if attached to existing or new process)\n");
        ctx.output_buffer[0] = '1';
        ctx.output_buffer[1] = 0;
      } else if (!strcmp(ptr, "fThreadInfo")) {
        gdb_debug("qfThreadInfo (Obtain a list of all active thread IDs)\n");
        ctx.output_buffer[0] = 'm';
        ctx.output_buffer[1] = '0';
        ctx.output_buffer[2] = 0;
      } else if (!strcmp(ptr, "sThreadInfo")) {
        gdb_debug("qsThreadInfo (Obtain a list of all active thread IDs, "
                  "subsequent)\n");
        ctx.output_buffer[0] = 'l';
        ctx.output_buffer[1] = 0;
      } else if (!strcmp(ptr, "Symbol::")) {
        gdb_debug("Symbol:: (Notify the target that GDB is prepared to serve "
                  "symbol lookup requests)\n");
        strcpy(ctx.output_buffer, "OK");
      } else if (!strcmp(ptr, "Supported")) {
        gdb_debug("qSupported");
        strcpy(ctx.output_buffer, "QStartNoAckMode+;PacketSize=1048576;");
      } else if (!strcmp(ptr, "Offsets")) {
        gdb_debug("qOffsets");
        strcpy(ctx.output_buffer, "Text=0;Data=0;Bss=0;");
      } else {
        gdb_debug("Unhandled: %c%s\n", cmd, ptr);
      }
      break;
    case 'Q':
      if (!strcmp(ptr, "StartNoAckMode")) {
        gdb_debug("QStartNoAckMode");
        strcpy(ctx.output_buffer, "OK");
        ctx.no_ack_mode = 1;
      } else {
        gdb_debug("Unhandled: %c%s\n", cmd, ptr);
      }
      break;
    case 'd':
      gdb_debug("d (Toggle debug flag)\n");
      break;
    case 'g':
      gdb_debug("g (Read general registers)\n");
      for (int l = 0; l < NUM_REGISTERS; l++)
        gdb_debug("%s: %x "
                  "",
                  register_names[l], ctx.registers[l]);
      gdb_debug("\n");
      mem_to_hex((char *)ctx.registers, ctx.output_buffer, NUM_REGISTERS * 4,
                 0);
      break;
    case 'G':
      gdb_debug("G (Write general registers)\n");
      hex_to_mem(ptr, (char *)ctx.registers, NUM_REGISTERS * 4, 0);
      strcpy(ctx.output_buffer, "OK");
      break;
    case 'P': {
      gdb_debug("P (Write register n with value r)\n");
      int register_number;

      if (hex_to_int(&ptr, &register_number) && *ptr++ == '=')
        if (register_number >= 0 && register_number < NUM_REGISTERS) {
          gdb_debug("set reg: %i, ", register_number);
          hex_to_mem(ptr, (char *)&ctx.registers[register_number], 4, 0);
          gdb_debug("\n");
          strcpy(ctx.output_buffer, "OK");
          break;
        }

      strcpy(ctx.output_buffer, "E01");
      break;
    }
    case 'm':
      gdb_debug("m (Read length addressable memory units starting at address "
                "addr)\n");
      if (hex_to_int(&ptr, &addr)) {
        gdb_debug("read, addr: %p, ", (void *)addr);
        if (*(ptr++) == ',') {
          if (hex_to_int(&ptr, &length)) {
            ptr = 0;
            ctx.mem_error = 0;
            mem_to_hex((char *)addr, ctx.output_buffer, length, 1);
            if (ctx.mem_error) {
              strcpy(ctx.output_buffer, "E03");
            }
          }
        }
      }
      gdb_debug("\n");
      if (ptr) {
        strcpy(ctx.output_buffer, "E01");
      }
      break;
    case 'M':
      gdb_debug("M (Write length addressable memory units starting at address "
                "addr)\n");
      if (hex_to_int(&ptr, &addr)) {
        gdb_debug("write, addr: %p, ", (void *)addr);
        if (*(ptr++) == ',') {
          if (hex_to_int(&ptr, &length))
            if (*(ptr++) == ':') {
              ctx.mem_error = 0;
              hex_to_mem(ptr, (char *)addr, length, 1);

              if (ctx.mem_error) {
                strcpy(ctx.output_buffer, "E03");
              } else {
                strcpy(ctx.output_buffer, "OK");
              }

              ptr = 0;
            }
        }
        gdb_debug("\n");
      }
      if (ptr) {
        strcpy(ctx.output_buffer, "E02");
      }
      break;

    case 's':
      stepping = 1;
    case 'c': {
      addr = 0;
      if (hex_to_int(&ptr, &addr)) {
        ctx.registers[EIP] = addr;
      }
      gdb_debug("%c, offset: %p, ip: %p (%s)\n", cmd, (void *)addr,
                (void *)ctx.registers[EIP], cmd == 'c' ? "Continue" : "Step");
      ctx.registers[EFLAGS] &= 0xfffffeff;
      if (stepping)
        ctx.registers[EFLAGS] |= 0x100;

      handler_mutex = 0;
      return;
    }
    case 'k':
      break;
    default:
      gdb_debug("Unhandled: %c%s\n", cmd, ptr);
    }

    gdb_write_packet((unsigned char *)ctx.output_buffer);
  }

  handler_mutex = 0;
  return;
}

void gdb_tick_handler(void) {
  int status = _bios_serialcom(_COM_STATUS, 0, 0);
  if (status & (1 << 8) && !handler_mutex) {
    ctx.was_interrupted = 1;
  }
}

void gdb_checkpoint() {
  if (ctx.was_interrupted) {
    ctx.was_interrupted = 0;
    asm("int $3");
  }
}
#endif
#endif