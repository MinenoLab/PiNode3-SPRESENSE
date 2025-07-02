#include <Camera.h>
#include <iostream>
#include <MPMutex.h>

// ------------------------------------------
// 定義
// ------------------------------------------
#define BAUDRATE    (115200)      // シリアル通信速度
#define CODE_SIZE   (4)           // パケット内のコード長（Byte）
#define PACKET_SIZE (100)         // パケット内のデータ長（Byte）
#define CRC         (true)        // パケットにCRCを含めるか

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
static CamImage g_image;        // 画像撮影データ
static State    g_state;        // 状態
MPMutex mutex(MP_MUTEX_ID0);    // セマフォ

// ------------------------------------------
// CRC8 (Dallas poly:0x31, init=0x00)を計算
//  Args:
//    data(uint8_t*): 入力バイト配列
//    length(size_t): 入力バイト配列の長さ
//  Return:
//    uint8_t: CRC
// ------------------------------------------
uint8_t crc8_dallas(const uint8_t* data, size_t length)
{
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
size_t cobsEncode(uint8_t *out, const uint8_t *in, size_t length)
{
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
//    code(uint16_t): コード or 送信パケットのインデックス
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

// ------------------------------------------
// バイト配列からJPGフッター（0xFF, 0xD9）の位置
// を検出する
//  Args:
//    buf(uint8_t*): JPGデータのバイト配列
//    len(size_t)  : バイト配列の長さ
//  Return:
//    int : フッターの位置
// ------------------------------------------
int find_jpegfooter(const uint8_t* buf, size_t len)
{
    if (len < 2) return -1;

    for (size_t i = 1; i < len; ++i) {
        if (buf[i - 1] == 0xFF && buf[i] == 0xD9) {
            return i;
        }
    }
    return -1;  // 見つからなかった場合
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

// 4桁(CODE_SIZE)のsize_tを4つのuint8_tに格納
void extractDigits(size_t value, uint8_t& digit1, uint8_t& digit2, uint8_t& digit3, uint8_t& digit4)
{
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
    for (size_t i = 0; i < 3; ++i) {
        img = theCamera.takePicture();
        if (img.isAvailable()) {
            return true;
        }
    }
    return false;
}

// 最大インデックス取得
size_t get_max_index(size_t size)
{
    return (size + PACKET_SIZE - 1) / PACKET_SIZE - 1;
}

// 指定インデックスの画像データをパケットにして送信
void send_image_by_index(const uint8_t* buf, size_t buf_size, size_t max_index, size_t idx)
{
    size_t start = idx * PACKET_SIZE;
    if (idx == max_index) {
        // 最終パケットを送信
        size_t packet_size = buf_size % PACKET_SIZE;
        send_packet(SendType::Finish, idx, buf + start, packet_size);
    } else {
        send_packet(SendType::Image, idx, buf + start, PACKET_SIZE);
    }
}

// Camera Callback
void CamCB(CamImage img)
{
    if (img.isAvailable() && g_state == State::Standby ) {
        // ストリームを画像データ用バッファへコピー
        uint8_t *buf = img.getImgBuff();
        size_t len = img.getImgSize();
        uint8_t *g_buf = g_image.getImgBuff();

        // バッファ末尾の0xffを削除
        int offset = 0;
        while (buf[(len - 1) - offset] == 0xFF) {
            offset++;
        }
        len -= offset;
        mutex.Trylock();
        std::copy(buf, buf + len, g_buf);
        // フッターを追加（付かない場合があるため）
        // 十分なバッファ長があるため範囲外チェックは省略
        g_buf[len] = 0xFF;
        g_buf[len + 1] = 0xD9;
        mutex.Unlock();
    }
}

// 画像取得モード設定
void setCameraMode()
{
    CamErr err;
    err = theCamera.begin(
        1,
        CAM_VIDEO_FPS_15,
        CAM_IMGSIZE_QQVGA_H,
        CAM_IMGSIZE_QQVGA_V,
        CAM_IMAGE_PIX_FMT_JPG,
        2
    );
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

    // !! バッファの確保
    // 使用メモリ削減のためStill画像のバッファを
    // Stream画像バッファと共有していることに注意
    take_picture(g_image);

    // Callbackの登録
    err = theCamera.startStreaming(true, CamCB);
    if (err != CAM_ERR_SUCCESS) { printError(err); }
}

void setup()
{
    Serial.begin(BAUDRATE);
    while(!Serial) {};
    setCameraMode();

    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);

    g_state = State::Standby;
}

void loop()
{
    static size_t send_size = 0;
    static size_t max_index = 0;
    static bool still_mode = false;

    // 待機状態
    if (g_state == State::Standby) {
        String val = Serial.readStringUntil('\n');
        if (val.substring(0, 1) == "S") {
            still_mode = true;
            g_state = State::Capture;
            digitalWrite(LED0, HIGH);
        }
        else if (val.substring(0, 1) == "V") {
            still_mode = false;
            g_state = State::Capture;
            digitalWrite(LED3, HIGH);
        } else {
          digitalWrite(LED0, LOW);
          digitalWrite(LED1, LOW);
          digitalWrite(LED2, LOW);
          digitalWrite(LED3, LOW);
        }
    }
    // 画像撮影状態
    else if (g_state == State::Capture) {
        // カメラコールバック中は待つ
        int ret;
        do {
            ret = mutex.Trylock();
        } while (ret != 0);

        if (still_mode) {
            take_picture(g_image);
        }

        uint8_t* buf = g_image.getImgBuff();
        int footer_idx = find_jpegfooter(buf, g_image.getImgBuffSize());

        if (footer_idx > 0) {
            send_size = (size_t)(footer_idx + 1);
            max_index = get_max_index(send_size);
            // INFOパケットは画像データの最大インデックスを送信することに注意
            send_packet(SendType::Info, max_index, {0}, 0);
            g_state = State::ImageSending;
            digitalWrite(LED1, HIGH);
        } else {
            g_state = State::Standby;
        }
    }
    // 送信状態
    else if (g_state == State::ImageSending) {
        uint8_t* buf = g_image.getImgBuff();
        for (size_t i=0; i <= max_index; ++i) {
            send_image_by_index(buf, send_size, max_index, i);
        }
        g_state = State::TerminationStandby;
        digitalWrite(LED2, HIGH);
    }
    // 画像送信終了待機状態
    else if (g_state == State::TerminationStandby) {
        String val = Serial.readStringUntil('\n');
        if (val.substring(0, 1) == "R") {
            // 再送するインデックス番号を取得
            size_t resend_index = static_cast<size_t>(val.substring(1).toInt());
            uint8_t* buf = g_image.getImgBuff();
            send_image_by_index(buf, send_size, max_index, resend_index);
        } else if (val.substring(0, 1) == "E") {
            // 終了
            g_state = State::Standby;
        }
    }
}
