#!python3
import sys
from ctypes import (windll, byref, c_int)

stdout_handle = windll.kernel32.GetStdHandle(-11)


def check_intialized(api):
    def _in_check(self, msg, end='\n'):
        if self.initialized:
            api(self, msg, end)
        else:
            print(msg, end=end, flush=True)
        sys.stdout.flush()

    return _in_check


class TERMINAL_MANAGER(object):
    def __init__(self) -> None:
        self.console_mode = 0
        self.initialized = self.initialize()

    def log(msg, end=''):
        print(msg, end=end)

    def get_console_mode(self) -> int:
        c_i = c_int()
        ret = windll.kernel32.GetConsoleMode(stdout_handle, byref(c_i))
        if ret == 0:
            raise Exception()
        return c_i.value

    def set_console_mode(self, mode: int) -> bool:
        return windll.kernel32.SetConsoleMode(stdout_handle, mode)

    def initialize(self) -> bool:
        try:
            self.console_mode = self.get_console_mode()
            return (0 != self.set_console_mode(self.console_mode | 4))
        except Exception as e:
            print(e)
            return False

    @check_intialized
    def banner(self, msg, end='\n'):
        length = len(msg)
        f = 128/length
        for i in range(length):
            print(
                f'\x1b[38;2;254;221;216;48;2;72;{int(i*f)};128m{msg[i]}\x1b[0m', end='')

        print('', end=end)

    def post(self, color, msg, end='\n'):
        r = color >> 16
        g = (color & 0xff00) >> 8
        b = color & 0xFf
        print(f"\x1b[38;2;{r};{g};{b}m{msg}\x1b[0m", end=end)

    @check_intialized
    def c_red(self, msg, end):
        self.post(0xFE8181, msg, end)

    @check_intialized
    def c_green(self, msg, end):
        self.post(0x8ff7a7, msg, end)

    @check_intialized
    def c_cyan(self, msg, end):
        self.post(0x21d0ff, msg, end)

    @check_intialized
    def c_yellow(self, msg, end):
        self.post(0xffbf00, msg, end)

    @check_intialized
    def c_gray(self, msg, end):
        self.post(0x695e53, msg, end)

    @check_intialized
    def c_white(self, msg, end):
        self.post(0xFDFDFD, msg, end)

    @check_intialized
    def c_magenta(self, msg, end):
        self.post(0xc495eb, msg, end)

    @check_intialized
    def c_light_gray(self, msg, end):
        self.post(0xc4aead, msg, end)


g_term_mgr = TERMINAL_MANAGER()

term_colors = [x[2:] for x in dir(g_term_mgr) if x.startswith('c_')]
for color in term_colors:
    globals()[color] = getattr(g_term_mgr, f'c_{color}')


def gradient(msg, end='\n'):
    g_term_mgr.banner(msg, end)
