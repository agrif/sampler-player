# some ioctl nonsense
# http://code.activestate.com/recipes/578225-linux-ioctl-numbers-in-python/

import struct
import fcntl
import os.path
import numpy

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
        v = getattr(self, '_' + name, None)
        if v is not None:
            return v
        v = self.get_sysfs(name)
        setattr(self, '_' + name, v)
        return v
    return property(getter)

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

    def read(self):
        # read fresh stuff
        self.reload()
        self.device.seek(0)
        outputs = numpy.fromfile(self.device, dtype=numpy.uint8, count=self.length)        
        outputs = numpy.reshape(outputs, (self.time_length, self.sample_length))
        outputs = numpy.unpackbits(outputs, axis=1)
        return outputs[:,:self.sample_width]

    def write(self, inputs):
        time, samps = inputs.shape
        if time >= self.time_length or samps >= self.sample_width:
            raise ValueError('too much data to write')

        inputs = numpy.packbits(inputs, axis=1)
        time, samps = inputs.shape
        inputs = numpy.pad(inputs, [(0, self.time_length - time), (0, self.sample_length - samps)], 'constant')
        
        self.device.seek(0)
        inputs.tofile(self.device)

        # flush to disk (a hack)
        #self.device.flush()
        self.reload()

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

class SPPair:
    def __init__(self, sampler, player):
        self.samp = SamplerPlayer(sampler)
        self.play = SamplerPlayer(player, True)

    def run(self, inputs):
        self.samp.enabled = 0
        self.play.enabled = 0

        self.play.write(inputs)

        # this will only really work if the player is tied to the sampler
        # with the enable line. otherwise, python is too slow.
        self.samp.enabled = 1
        self.play.enabled = 1
        
        while not self.samp.done:
            continue
        while not self.play.done:
            continue

        self.samp.enable = 0
        self.play.enable = 0

        return self.samp.read()

pair = SPPair('/dev/sampler0', '/dev/player0')

import random
import time

ITERS = 1
start = time.time()

inputs = numpy.array([[1, 1, 1, 1, 1, 1, 1, 1], [0, 0, 0, 0, 0, 0, 0, 0]])
for _ in range(ITERS):
    other = [random.choice([0, 1]) for _ in range(8)]
    inputs[1,:] = other
    outputs = pair.run(inputs)

end = time.time()

print('ran {} iters in {} seconds ({} per second)'.format(ITERS, end - start, ITERS / (end - start)))
print("0x{:02x}".format(*numpy.packbits(other)))
print(outputs)
print(outputs.shape)
