from cobs import cobs
import numpy as np
import cv2
import time
import crcmod
import serial

class Spresense:
    BAUD_RATE   = 115200  # ボーレート
    TYPE_INFO   = 0
    TYPE_IMAGE  = 1
    TYPE_FINISH = 2
    TYPE_ERROR  = 3

    def __init__(self, ser) -> None:
        self.ser = ser

    def get_packet(self, timeout=3):
        buf = bytearray()
        start = time.time()
        while True:
            val = self.ser.read()
            if val == b'\x00':
                break
            elif time.time() - start > timeout:
                print("get_packet timeout")
                return False, None, None, None
            buf += val
        decoded = cobs.decode(buf)

        crc8_func = crcmod.predefined.mkCrcFun('crc-8-maxim')
        crc = crc8_func(decoded[0:-1])
        is_crc_valid = (crc == decoded[-1])
        packet_type = decoded[0]
        index = int(decoded[1]) * 1000 + int(decoded[2]) * 100 + int(decoded[3]) * 10 + int(decoded[4])
        payload = decoded[5:-1]
        return is_crc_valid, packet_type, index, payload

    def send_request_image(self):
        self.ser.write(str.encode('S\n'))

    def send_request_thumbnail(self):
        self.ser.write(str.encode('V\n'))

    def send_complete_image(self):
        self.ser.write(str.encode('E\n'))

    def send_request_resend(self, index):
        self.ser.write(str.encode(f'R{index}\n'))

    def get_image(self, thumbnail=False):
        start = time.time()

        if thumbnail:
            send_request = self.send_request_thumbnail
        else:
            send_request = self.send_request_image

        try:
            # 1. 送信要求
            send_request()

            # 2. INFOパケット待ち
            ret, code, index, data = self.get_packet()
            if ret and (code == self.TYPE_INFO):
                max_index = index
                buf = [None] * (max_index + 1)
                print("INFO:", max_index)
            else:
                print("Don't receive INFO paket")
                return False, None

            # 3. 画像データ受信
            for i in range(len(buf)):
                ret, code, index, data = self.get_packet()
                print("IMAGE:", ret, code, index, len(data))
                if ret:
                    if (code == self.TYPE_IMAGE) or (code == self.TYPE_FINISH):
                        buf[index] = data

            # 4. 再送要求（１回まで）
            for i in range(len(buf)):
                if buf[i] is None:
                    print("RETRY:", i)
                    self.send_request_resend(i)
                    ret, code, index, data = self.get_packet()
                    if ret:
                        if (code == self.TYPE_IMAGE) or (code == self.TYPE_FINISH):
                            buf[i] = data

            # 5. 終了送信
            self.send_complete_image()
            end = time.time()
            print("Elapsed:", end - start)

            img = bytearray(b''.join(buf))
            img = cv2.imdecode(np.frombuffer(img, dtype='uint8'), cv2.IMREAD_COLOR)
            if img is None:
                return False, None
            else:
                return True, img
        except TypeError:
            return False, None
        except cv2.error:
            return False, None
        except serial.SerialException:
            return False, None
