# some ioctl nonsense
# http://code.activestate.com/recipes/578225-linux-ioctl-numbers-in-python/

import struct
import fcntl
import os.path

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8

# arch specific
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRMASK = (1 << _IOC_NRBITS) - 1
_IOC_TYPEMASK = (1 << _IOC_TYPEBITS) - 1
_IOC_SIZEMASK = (1 << _IOC_SIZEBITS) - 1
_IOC_DIRMASK = (1 << _IOC_DIRBITS) - 1

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
_IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
_IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir, type, nr, size):
    if isinstance(size, str):
        size = struct.calcsize(size)
    return dir << _IOC_DIRSHIFT | type << _IOC_TYPESHIFT | nr << _IOC_NRSHIFT | size << _IOC_SIZESHIFT

def _IO(type, nr): return _IOC(_IOC_NONE, type, nr, 0)
def _IOR(type, nr, size): return _IOC(_IOC_READ, type, nr, size)
def _IOW(type, nr, size): return _IOC(_IOC_WRITE, type, nr, size)
def _IORW(type, nr, size): return _IOC(_IOC_READ | _IOC_WRITE, type, nr, size)

# ok, now to define *our* ioctls

IOC_MAGIC = 0x9d

GET_ENABLED = _IO(IOC_MAGIC, 0)
SET_ENABLED = _IO(IOC_MAGIC, 1)

GET_DONE = _IO(IOC_MAGIC, 2)

def sysfs_property(name):
    def getter(self):
        return self.get_sysfs(name)
    def setter(self, v):
        self.set_sysfs(name, v)
    return property(getter, setter)

def ioctl_property(get, set=None):
    def getter(self):
        return fcntl.ioctl(self.device, get)
    def setter(self, v):
        fcntl.ioctl(self.device, set, v)
    if set:
        return property(getter, setter)
    return property(getter)

class SamplerPlayer:
    def __init__(self, path, write=False):
        if not path.startswith('/dev/'):
            raise ValueError('not a valid device')
        self.name = path[len('/dev/'):]
        self.is_player = write
        self.device = None

        if not os.path.exists('/dev/' + self.name):
            raise RuntimeError('device /dev/' + self.name + ' does not exist')
        if not os.path.exists('/sys/block/' + self.name + '/device/sample_width'):
            raise RuntimeError('device /dev/' + self.name + ' is not a sampler or player.')

        self.reload()

    def reload(self):
        if self.device:
            self.device.close()
        if self.is_player:
            self.device = open('/dev/' + self.name, 'wb', buffering=0)
        else:
            self.device = open('/dev/' + self.name, 'rb', buffering=0)

    def trigger(self):
        self.enabled = 0
        self.enabled = 1
        self.reload()

    def read(self):
        self.device.seek(0)
        return self.device.read(self.length)

    def write(self, buf):
        if len(buf) < self.length:
            buf += b'\00' * (self.length - len(buf))
        self.device.seek(0)
        self.device.write(buf)

    def get_sysfs(self, attr):
        with open('/sys/block/' + self.name + '/device/' + attr) as f:
            return int(f.read().strip())

    def set_sysfs(self, attr, val):
        with open('/sys/block/' + self.name + '/device/' + attr, 'w') as f:
            f.write(str(int(val)) + '\n')

    sample_width = sysfs_property('sample_width')
    sample_bits = sysfs_property('sample_bits')
    sample_length = sysfs_property('sample_length')
    time_bits = sysfs_property('time_bits')
    time_length = sysfs_property('time_length')
    bits = sysfs_property('bits')
    length = sysfs_property('length')

    enabled = ioctl_property(GET_ENABLED, SET_ENABLED)
    done = ioctl_property(GET_DONE)

s = SamplerPlayer('/dev/sampler0')
p = SamplerPlayer('/dev/player0', True)

s.enabled = 0
p.enabled = 0

p.write(b'\xff' * p.sample_length + b'\xbb' * p.sample_length)

fs = s.device.fileno()
fp = p.device.fileno()
ioctl = fcntl.ioctl
ioctl(fs, SET_ENABLED, 1)
ioctl(fp, SET_ENABLED, 1)

#s.enabled = 1
#p.enabled = 1

s.reload()
print(s.read()[0])
