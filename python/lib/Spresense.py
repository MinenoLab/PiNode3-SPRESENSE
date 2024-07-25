from cobs import cobs
import numpy as np
import cv2
import time

class Spresense:
    BAUD_RATE = 115200  # ボーレート
    BUFF_SIZE = 100  # 1回の通信で送られてくるデータサイズ
    TYPE_INFO = 0
    TYPE_IMAGE = 1
    TYPE_FINISH = 2
    TYPE_ERROR = 3

    def __init__(self, ser) -> None:
        super().__init__()
        self.ser = ser

    def get_packet(self):
        try:
            img = bytearray()
            while True:
                val = self.ser.read()
                if val == b'\x00':
                    break
                img += val
            decoded = cobs.decode(img)
            index = int(decoded[1]) * 1000 + int(decoded[2]) * 100 + int(decoded[3]) * 10 + int(decoded[4])
            return decoded[0], index, decoded[5:]
        except Exception as e:
            return self.TYPE_ERROR, 0, b''

    def check_packet(self, data):
        if len(data) != self.BUFF_SIZE:
            return False
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

        self.send_request_image()

        while True:
            code, index, data = self.get_packet()

            # データ取得
            if code == self.TYPE_INFO:
                img = bytearray(index * self.BUFF_SIZE)
                max_index = index
                send_flg = [False] * max_index
            elif code == self.TYPE_IMAGE:
                if self.check_packet(data):
                    img[index*self.BUFF_SIZE:(index+1)*self.BUFF_SIZE] = data
                    send_flg[index] = True
                    if index in resend_index_list:
                        resend_index_list.remove(index)
                else:
                    resend_index_list.append(index)
            elif code == self.TYPE_FINISH:
                img += data
                finish_flag = True
            elif code == self.TYPE_ERROR:
                print('cant get data')

            # 終了チェック
            if finish_flag:
                if False in send_flg:
                    print("resend", send_flg.index(False))
                    self.send_request_resend(send_flg.index(False))
                for index in resend_index_list:
                    self.send_request_resend(index)
                if (len(resend_index_list) == 0) and (not (False in send_flg)):
                    self.send_complete_image()
                    break
        return img

    def read(self):
        img = self.get_image_data()
        img = cv2.imdecode(np.frombuffer(img, dtype='uint8'), 1)
        print("shoot image success")
        return True, img

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