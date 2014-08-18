class bcolors:
    """
    Black       0;30     Dark Gray     1;30
    Blue        0;34     Light Blue    1;34
    Green       0;32     Light Green   1;32
    Cyan        0;36     Light Cyan    1;36
    Red         0;31     Light Red     1;31
    Purple      0;35     Light Purple  1;35
    Brown       0;33     Yellow        1;33
    Light Gray  0;37     White         1;37
    """

    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    INFO = '\033[1;33m'  # yellow
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

    def disable(self):
        self.HEADER = ''
        self.OKBLUE = ''
        self.OKGREEN = ''
        self.WARNING = ''
        self.FAIL = ''
        self.ENDC = ''

def hprint(msg):
    print bcolors.INFO + msg + bcolors.ENDC

def eprint(msg):
    print bcolors.FAIL + msg + bcolors.ENDC
