import struct
import io


def decode_value(data: io.BytesIO):
    (kind,) = struct.unpack("b", data.read(1))
    if kind == 0:
        (result,) = struct.unpack(">I", data.read(4))
        return result
    elif kind == 1:
        (result,) = struct.unpack(">Q", data.read(8))
        return result
    elif kind == 2:
        (string_length,) = struct.unpack(">I", data.read(4))
        result = data.read(string_length)
        return result.decode("utf-8")
    elif kind == 3:
        (array_length,) = struct.unpack(">I", data.read(4))
        result = []
        for i in range(array_length):
            value = decode_value(data)
            result.append(value)
        return result
    elif kind == 4:
        (map_length,) = struct.unpack(">I", data.read(4))
        result = dict()
        for i in range(map_length):
            key = decode_value(data)
            value = decode_value(data)
            result[key] = value
        return result

    raise "Unknown data type"
