from __future__ import print_function

import argparse
import sys
import fcntl
import os.path
import random
import struct
import io
import traceback
import mmap

if sys.version_info >= (3, 0):
    import urllib.request as urllib_request
    import http.server as http_server
else:
    import urllib2 as urllib_request
    import BaseHTTPServer as http_server

import numpy

# some base addresses for AXI bridges
H2F_LW_AXI_MASTER = 0xff200000
H2F_AXI_MASTER = 0xc0000000

# mmap only works on page boundaries, so here's a helper
def kludgy_mmap(fno, size, offset):
    page = mmap.ALLOCATIONGRANULARITY
    p_offset = int(numpy.floor(offset / page)) * page
    offset_diff = offset - p_offset
    assert offset_diff >= 0
    assert p_offset % page == 0
    
    new_size = size + offset_diff
    p_size = int(numpy.ceil(new_size / page)) * page
    assert p_size >= size
    assert p_size % page == 0

    whole_buf = mmap.mmap(fno, p_size, offset=p_offset)
    return whole_buf, offset_diff

# some ioctl nonsense
# http://code.activestate.com/recipes/578225-linux-ioctl-numbers-in-python/

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

# to use numpy.packbits, we need a way to quickly swap LSB with MSB in a byte
# so, use a table.
# this is horrible, but (hilariously) faster than other methods
swaptable = numpy.array([int('{:08b}'.format(n)[::-1], 2) for n in range(256)], dtype=numpy.uint8)

def sysfs_property(name, type=int):
    def getter(self):
        v = getattr(self, '_' + name, None)
        if v is not None:
            return v
        v = self.get_sysfs(name, type=type)
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

class SamplerPlayerBase(object):
    def read(self):
        # read fresh stuff
        outputs = self.read_raw()
        outputs = numpy.reshape(outputs, (self.time_length, self.sample_length))
        outputs = swaptable[outputs]
        outputs = numpy.unpackbits(outputs, axis=1)
        return outputs[:,:self.sample_width]

    def write(self, inputs):
        time, samps = inputs.shape
        if time > self.time_length or samps > self.sample_width:
            raise ValueError('too much data to write')

        inputs = numpy.packbits(inputs.astype(int), axis=1)
        inputs = swaptable[inputs]
        time, samps = inputs.shape
        inputs = numpy.pad(inputs, [(0, self.time_length - time), (0, self.sample_length - samps)], 'constant')

        self.write_raw(inputs)

class DriverSamplerPlayer(SamplerPlayerBase):
    def __init__(self, path):
        if not path.startswith('/dev/'):
            raise ValueError('not a valid device')
        self.name = path[len('/dev/'):]
        self.device = None

        if not os.path.exists('/dev/' + self.name):
            raise RuntimeError('device /dev/' + self.name + ' does not exist')
        if not os.path.exists('/sys/block/' + self.name + '/device/sample_width'):
            raise RuntimeError('device /dev/' + self.name + ' is not a sampler or player.')

        self.reload()

    def reload(self):
        if self.device:
            self.device.close()
        if self.type == 'player':
            self.device = open('/dev/' + self.name, 'wb', buffering=0)
        elif self.type == 'sampler':
            self.device = open('/dev/' + self.name, 'rb', buffering=0)
        else:
            raise RuntimeError('unknown type ' + self.type)

    def read_raw(self):
        self.reload()
        self.device.seek(0)
        return numpy.fromfile(self.device, dtype=numpy.uint8, count=self.length)

    def write_raw(self, inputs):
        self.device.seek(0)
        inputs.tofile(self.device)
        self.reload()

    def get_sysfs(self, attr, type=int):
        with open('/sys/block/' + self.name + '/device/' + attr) as f:
            return type(f.read().strip())

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
    type = sysfs_property('type', type=str)

    enabled = ioctl_property(GET_ENABLED, SET_ENABLED)
    done = ioctl_property(GET_DONE)

class MemSamplerPlayer(SamplerPlayerBase):
    def __init__(self, typ, csr_addr, base_addr, sample_width, time_bits):
        self.type = typ
        self.sample_width = sample_width
        self.time_bits = time_bits

        # calculated
        self.sample_bits = int(numpy.ceil(numpy.log(sample_width / 32.0) / numpy.log(2))) + 2
        self.sample_length = 1 << self.sample_bits
        self.time_length = 1 << self.time_bits
        self.bits = self.time_bits + self.sample_bits
        self.length = self.time_length * self.sample_length

        with open('/dev/mem', 'r+b') as f:
            self.csr, self.csr_start = kludgy_mmap(f.fileno(), 4, offset=csr_addr)
            self.data, self.data_start = kludgy_mmap(f.fileno(), self.length, offset=base_addr)

    def read_raw(self):
        return numpy.frombuffer(self.data[self.data_start:self.data_start + self.length], dtype=numpy.uint8, count=self.length)

    def write_raw(self, inputs):
        self.data[self.data_start:self.data_start + self.length] = inputs

    def get_enabled(self):
        return bool(self.csr[self.csr_start] & 0x1)

    def set_enabled(self, enabled):
        self.csr[self.csr_start] = (self.csr[self.csr_start] & (~0x1)) | (0x1 if enabled else 0)

    enabled = property(get_enabled, set_enabled)

    @property
    def done(self):
        return bool(self.csr[self.csr_start] & 0x2)

class Sampler(DriverSamplerPlayer):
    def __init__(self, device):
        super(Sampler, self).__init__(device)
        if not self.type == 'sampler':
            raise RuntimeError('device is not a sampler')

class Player(DriverSamplerPlayer):
    def __init__(self, device):
        super(Player, self).__init__(device)
        if not self.type == 'player':
            raise RuntimeError('device is not a player')

class MemSampler(MemSamplerPlayer):
    def __init__(self, *args, **kwargs):
        super(MemSampler, self).__init__('sampler', *args, **kwargs)

class MemPlayer(MemSamplerPlayer):
    def __init__(self, *args, **kwargs):
        super(MemPlayer, self).__init__('player', *args, **kwargs)
    
class SPPair(object):
    def __init__(self, sampler, player):
        if isinstance(sampler, str):
            self.samp = Sampler(sampler)
        else:
            self.samp = sampler
        if isinstance(player, str):
            self.play = Player(player)
        else:
            self.play = player

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

server_size_field = struct.Struct('>I')

def server_pack(arr):
    arr = numpy.array(arr)
    with io.BytesIO() as f:
        f.write(server_size_field.pack(arr.shape[0]))
        f.write(server_size_field.pack(arr.shape[1]))
        arr = numpy.packbits(arr, axis=1)
        f.write(memoryview(arr))
        return f.getvalue()

def server_unpack(arr):
    with io.BytesIO(arr) as f:
        size1 = server_size_field.unpack(f.read(server_size_field.size))[0]
        size2 = server_size_field.unpack(f.read(server_size_field.size))[0]
        arr = numpy.frombuffer(f.read(), dtype=numpy.uint8)
        if size1:
            othersize = len(arr) // size1
        else:
            othersize = 0
        arr = numpy.reshape(arr, (size1, othersize))
        arr = numpy.unpackbits(arr, axis=1)
        arr = arr[:,:size2]
    return arr

class SPClient:
    def __init__(self, host, port=8000):
        self.host = host
        self.port = port

    def run(self, inputs):
        url = 'http://{}:{}/run'.format(self.host, self.port)

        with urllib_request.urlopen(url, server_pack(inputs)) as resp:
            outputs = server_unpack(resp.read())
        
        return outputs

class SPServer(http_server.HTTPServer):
    class RequestHandler(http_server.BaseHTTPRequestHandler):
        def do_POST(self):
            try:
                if self.path == '/run':
                    self.handle_run()
                else:
                    self.send_error(404)
            except Exception as e:
                self.send_response(500, str(e))
                self.send_header('Content-Type', 'text/plain; charset=utf-8')
                self.end_headers()
                self.wfile.write(str(e).encode('utf-8'))

        def handle_run(self):
            inputs = server_unpack(self.rfile.read(int(self.headers['Content-Length'])))
            outputs = server_pack(self.server.pair.run(inputs))
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.end_headers()
            self.wfile.write(outputs)
    
    def __init__(self, pair, host='', port=8000):
        self.pair = pair
        super(SPServer, self).__init__((host, port), self.RequestHandler)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('sampler')
    parser.add_argument('player')
    parser.add_argument('--server', action='store_true', default=False, help='run a sampler/player server, instead of feeding a pulse')
    parser.add_argument('--port', type=int, default=8000)
    parser.add_argument('--host', type=str, default='')

    args = parser.parse_args()
    pair = SPPair(args.sampler, args.player)

    if args.server:
        server = SPServer(pair, host=args.host, port=args.port)
        server.serve_forever()

    inputs = numpy.array([[0], [1], [0]])
    outputs = pair.run(inputs)
    for t in range(outputs.shape[0]):
        print(''.join(str(b) for b in outputs[t, :]))
