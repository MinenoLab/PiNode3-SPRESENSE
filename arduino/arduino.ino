#include <Camera.h>
#include <iostream>

// ------------------------------------------
// 定義
// ------------------------------------------
#define BAUDRATE  (115200)        // シリアル通信速度
#define CODE_SIZE (4)             // パケット内のコード長（Byte）
#define MAIN_SIZE (100)           // パケット内のデータ長（Byte）
#define CRC       (true)          // パケットにCRCを含めるか

// 状態定義
enum class State
{
    Standby,            // 待機状態
	  Capture,            // 撮影状態
	  ImageSending,       // 画像送信状態
	  TerminationStandby, // 画像送信終了待機状態
};

// 送信タイプ定義
enum class SendType
{
	  Info,   // 情報
	  Image,  // 画像
	  Finish, // 画像送信終了
	  Error,  // エラー
};

// ------------------------------------------
// グローバル変数
// ------------------------------------------
static CamImage g_image;    // 画像撮影データ

// ------------------------------------------
// CRC8 (Dallas poly:0x31, init=0x00)を計算
//  Args:
//    data(uint8_t*): 入力バイト配列
//    length(size_t): 入力バイト配列の長さ
//  Return:
//    uint8_t: CRC
// ------------------------------------------
uint8_t crc8_dallas(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ------------------------------------------
// COBSエンコード処理
//  Args:
//    out(uint8_t*): 出力先バイト配列
//    in (uint8_t*): 入力バイト配列
//    length(size_t): 入力バイト配列の長さ
//  Return:
//    size_t: 出力バイト配列の長さ
// ------------------------------------------
size_t cobsEncode(uint8_t *out, const uint8_t *in, size_t length) {
    const uint8_t *end = in + length;
    uint8_t *start = out;
    uint8_t *code_ptr = out++;  // 最初のコードバイトの位置
    uint8_t code = 1;

    while (in < end) {
        if (*in == 0) {
            *code_ptr = code;
            code_ptr = out++;
            code = 1;
        } else {
            *out++ = *in;
            code++;
            if (code == 0xFF) {
                *code_ptr = code;
                code_ptr = out++;
                code = 1;
            }
        }
        in++;
    }

    *code_ptr = code;  // 最後のコードバイト
    return out - start;
}

// ------------------------------------------
// パケット生成して送信する
//  Args:
//    type(SendType): パケットの送信タイプ
//    code(uint16_t): コード
//    byte_array(uint8_t*): 送信データ
//    array_size(size_t): 送信データのサイズ
//  Return:
//    なし
// ------------------------------------------
void send_packet(SendType type, uint16_t code, const uint8_t* byte_array, size_t array_size)
{
    uint8_t p_in[1 + CODE_SIZE + array_size + 1];

    // 送信データの準備
    p_in[0] = static_cast<uint8_t>(type);
    extractDigits(code, p_in[1], p_in[2], p_in[3], p_in[4]);

    if (array_size > 0 ) {
      std::copy(byte_array, byte_array + array_size, p_in + 5);
    }

    size_t len = sizeof(p_in);
    if (CRC) {
        uint8_t crc = crc8_dallas(p_in, len - 1);
        p_in[1 + CODE_SIZE + array_size] = crc; // 末尾にCRCを追加
    } else {
        len = len - 1;  // CRCなしの場合は配列長を1減らす
    }

    uint8_t p_out[len + 1 + 8];  // 最大出力サイズ = 入力サイズ + (入力サイズ/256(切り上げ))+ 1 + 余裕
    len = cobsEncode(p_out, p_in, len);

    // データ送信
    for (size_t i = 0; i < len; ++i) {
      Serial.write(p_out[i]);
    }
    Serial.write('\0'); // パケットの終端を送信
}

// カメラ設定のエラー
void printError(enum CamErr err)
{
    switch (err)
    {
        case CAM_ERR_NO_DEVICE:
            Serial.println("No Device");
            break;
        case CAM_ERR_ILLEGAL_DEVERR:
            Serial.println("Illegal device error");
            break;
        case CAM_ERR_ALREADY_INITIALIZED:
            Serial.println("Already initialized");
            break;
        case CAM_ERR_NOT_INITIALIZED:
            Serial.println("Not initialized");
            break;
        case CAM_ERR_NOT_STILL_INITIALIZED:
            Serial.println("Still picture not initialized");
            break;
        case CAM_ERR_CANT_CREATE_THREAD:
            Serial.println("Failed to create thread");
            break;
        case CAM_ERR_INVALID_PARAM:
            Serial.println("Invalid parameter");
            break;
        case CAM_ERR_NO_MEMORY:
            Serial.println("No memory");
            break;
        case CAM_ERR_USR_INUSED:
            Serial.println("Buffer already in use");
            break;
        case CAM_ERR_NOT_PERMITTED:
            Serial.println("Operation not permitted");
            break;
        default:
            break;
    }
}

// 設定初期化
void initCamera()
{
    CamErr err;
    err = theCamera.begin(0);
    if (err != CAM_ERR_SUCCESS) { printError(err); }

    // HDRの設定
    err = theCamera.setHDR(CAM_HDR_MODE_ON);
    if (err != CAM_ERR_SUCCESS) { printError(err); }

    // 静止画フォーマットの設定
    err = theCamera.setStillPictureImageFormat(
        CAM_IMGSIZE_QUADVGA_H,
        CAM_IMGSIZE_QUADVGA_V,
        CAM_IMAGE_PIX_FMT_JPG,
        3
    );
    if (err != CAM_ERR_SUCCESS) { printError(err); }

    // ISOの設定
    err = theCamera.setAutoISOSensitivity(true);
    if (err != CAM_ERR_SUCCESS) { printError(err); }

    // 露出の設定
    err = theCamera.setAutoExposure(true);
    if (err != CAM_ERR_SUCCESS) { printError(err); }

    // jpeg品質の設定
    err = theCamera.setJPEGQuality(70);
    if (err != CAM_ERR_SUCCESS) { printError(err); }
}

// 4桁(CODE_SIZE)のsize_tを4つのuint8_tに格納
void extractDigits(size_t value, uint8_t& digit1, uint8_t& digit2, uint8_t& digit3, uint8_t& digit4) {
    // 4桁(CODE_SIZE)の文字列に変換
    String valueStr = String(value);
    while (valueStr.length() < CODE_SIZE) {
        valueStr = "0" + valueStr;
    }

    // 文字列から1桁ずつ数字を取り出してuint8_tに変換
    digit1 = valueStr.charAt(0) - '0';
    digit2 = valueStr.charAt(1) - '0';
    digit3 = valueStr.charAt(2) - '0';
    digit4 = valueStr.charAt(3) - '0';
}

// SPRESENSEで画像を撮影
bool take_picture(CamImage& img)
{
    for (int i = 0; i < 3; i++) {
        img = theCamera.takePicture();
        if (img.isAvailable()) {
            return true;
        }
    }
    Serial.println("cant take picture....");
    return false;
}

// 最大インデックス取得
size_t get_max_index(CamImage img)
{
    return img.getImgSize() / MAIN_SIZE;
}

// 指定インデックスの画像データをパケットにして送信
void send_image_by_index(CamImage img, size_t index) {
    uint8_t *buf = img.getImgBuff();
    size_t max_index = get_max_index(img);

    size_t start = index * MAIN_SIZE;
    if (index == max_index) {
        // 最終パケットを送信
        size_t send_size = img.getImgSize() % MAIN_SIZE;
        send_packet(SendType::Finish, index, buf + start, send_size);
    } else {
        send_packet(SendType::Image, index, buf + start, MAIN_SIZE);
    }
}

// 画像データをパケットに分割して送信
void send_image(CamImage img)
{
    size_t max_index = get_max_index(img);

    for (size_t i = 0; i <= max_index; ++i) {
        send_image_by_index(img, i);
    }
}

void setup()
{
    Serial.begin(BAUDRATE);
    while(!Serial) {};
    initCamera();

    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
}

void loop()
{
    static State state = State::Standby;

    // 待機状態
    if (state == State::Standby) {
        String val = Serial.readStringUntil('\n');
        if (val.substring(0, 1) == "S") {
            state = State::Capture;
            digitalWrite(LED0, HIGH);
        }
    }
    // 画像撮影状態
    else if (state == State::Capture) {
        take_picture(g_image);
        // INFOパケットは画像データの最大インデックスを送信することに注意
        // 送信データ長がLの場合(L-1)がインデックスとして送信される
        uint16_t code = (uint16_t)get_max_index(g_image);
        send_packet(SendType::Info, code, {0}, 0);
        state = State::ImageSending;
        digitalWrite(LED1, HIGH);
    }
    // 送信状態
    else if (state == State::ImageSending) {
        send_image(g_image);
        state = State::TerminationStandby;
        digitalWrite(LED2, HIGH);
    }
    // 画像送信終了待機状態
    else if (state == State::TerminationStandby) {
        String val = Serial.readStringUntil('\n');
        if (val.substring(0, 1) == "R") {
            // 再送するインデックス番号を取得
            size_t resend_index = static_cast<size_t>(val.substring(1).toInt());
            send_image_by_index(g_image, resend_index);
        } else if (val.substring(0, 1) == "E") {
            // 終了
            state = State::Standby;
            digitalWrite(LED0, LOW);
            digitalWrite(LED1, LOW);
            digitalWrite(LED2, LOW);
        }
    }
}
