from cobs import cobs
import numpy as np
import cv2
import time
import crcmod

class Spresense:
    BAUD_RATE   = 115200  # ボーレート
    TYPE_INFO   = 0
    TYPE_IMAGE  = 1
    TYPE_FINISH = 2
    TYPE_ERROR  = 3

    def __init__(self, ser) -> None:
        super().__init__()
        self.ser = ser

    def get_packet(self):
        buf = bytearray()
        while True:
            val = self.ser.read()
            if val == b'\x00':
                break
            buf += val
        decoded = cobs.decode(buf)

        index = int(decoded[1]) * 1000 + int(decoded[2]) * 100 + int(decoded[3]) * 10 + int(decoded[4])
        crc8_func = crcmod.predefined.mkCrcFun('crc-8-maxim')
        crc = crc8_func(decoded[0:-1])
        return (crc == decoded[-1]), decoded[0], index, decoded[5:-1]

    def check_packet(self, data):
        # if len(data) != self.BUFF_SIZE:
        #     return False
        #TODO CRCチェックに変える
        # print(len(data))
        return True

    def send_request_image(self):
        self.ser.write(str.encode('S\n'))

    def send_complete_image(self):
        self.ser.write(str.encode('E\n'))

    def send_request_resend(self, index):
        self.ser.write(str.encode(f'R{index}\n'))

    def get_image_data(self):
        img = bytearray()
        resend_index_list = []
        finish_flag = False
        send_flg = []

        start = time.time()

        # 1. 送信要求
        self.send_request_image()

        # 2. INFOパケット待ち
        while True:
            crc, code, index, data = self.get_packet()
            if crc and (code == self.TYPE_INFO):
                max_index = index
                buf = [None] * (max_index + 1)
                print("INFO:", max_index)
                break
            else:
                # 再送要求
                self.send_request_image()

        # 3. 画像データ受信
        for i in range(len(buf)):
            crc, code, index, data = self.get_packet()
            if crc:
                if (code == self.TYPE_IMAGE) or (code == self.TYPE_FINISH):
                    buf[index] = data

        # 4. 再送要求（１回まで）
        for i in range(len(buf)):
            if buf[i] is None:
                print("RETRY:", i)
                self.send_request_resend(i)
                crc, code, index, data = self.get_packet()
                if crc:
                    if (code == self.TYPE_IMAGE) or (code == self.TYPE_FINISH):
                        buf[i] = data

        # 5. 終了送信
        self.send_complete_image()
        end = time.time()
        print("Elapsed:", end - start)

        if None in buf:
            return None
        else:
            img = bytearray(b''.join(buf))
            return img

    def read(self):
        try:
            img = self.get_image_data()
            img = cv2.imdecode(np.frombuffer(img, dtype='uint8'), cv2.IMREAD_COLOR)
            print("shoot image success")
            return True, img
        except TypeError:
            return False, None

if __name__ == '__main__':
    import serial
    ser = serial.Serial('COM9', 115200, timeout=1)
    print("connecting...")
    time.sleep(10)
    spresense = Spresense(ser)
    ret, img = spresense.read()
    if ret:
        cv2.imshow('img', img)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    ser.close()