#include <Camera.h>
#include <Packetizer.h>
#include <vector>
#include <iostream>

// ボーレート
#define BAUDRATE 115200
// コード情報を入れるバイトサイズ
#define CODE_SIZE 4
// メインの情報を入れるバイトサイズ
#define MAIN_SIZE 100


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

// 画像撮影データ
CamImage img;
// 現在の状態
State state = State::Standby;

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


// 送信データサイズ確認
size_t get_send_image_size(CamImage img, size_t send_index)
{
    // 送信サイズ
    int send_size = img.getImgSize() - send_index * MAIN_SIZE;
    // 送信サイズ確認
    if (send_size > MAIN_SIZE) {
        send_size = MAIN_SIZE;
    }
    return send_size;
}

// 最大インデックス取得
size_t get_max_index(CamImage img)
{
    return img.getImgSize() / MAIN_SIZE;
}

// データ送信
void send_packet(SendType type, size_t code, size_t send_size)
{
    uint8_t p_in[send_size + CODE_SIZE];
    // コードをコピー
    extractDigits(code, p_in[0], p_in[1], p_in[2], p_in[3]);

    // 送信データをCOBSでエンコード
    auto& p_buff = Packetizer::encode(static_cast<int>(type), p_in, sizeof(p_in));
    // データ送信
    for (const auto& p: p_buff.data) {
        Serial.write(p);
    }
}

// データ送信
void send_packet(SendType type, size_t code, size_t send_size, CamImage img)
{
    uint8_t p_in[send_size + 4];
    // コードをコピー
    extractDigits(code, p_in[0], p_in[1], p_in[2], p_in[3]);
    // メインデータをコピー
    std::copy(img.getImgBuff()+(code*send_size), img.getImgBuff()+((code+1)*send_size), p_in+4);
    // 送信データをCOBSでエンコード
    auto& p_buff = Packetizer::encode(static_cast<int>(type), p_in, sizeof(p_in));
    // データ送信
    for (const auto& p: p_buff.data) {
        Serial.write(p);
    }
}

// 画像全データ送信
void send_image_data(CamImage img)
{
    // 送信インデックス
    size_t send_index = 0;
    while(true) {
        if (send_index == get_max_index(img)) {
            send_packet(SendType::Finish, send_index, get_send_image_size(img, send_index), img);
            break;
        } else {
            send_packet(SendType::Image, send_index, get_send_image_size(img, send_index), img);
            send_index += 1;
        }
    }
}

void setup()
{
    Serial.begin(BAUDRATE);
    while(!Serial) {};
    initCamera();
    state = State::Standby;
}

void loop()
{
    String val = Serial.readStringUntil('\n');

    // 待機状態
    if (state == State::Standby) {
        if (val.substring(0, 1) == "S") {
            state = State::Capture;
        }
    }
    // 画像撮影状態
    else if (state == State::Capture) {
        take_picture(img);
        send_packet(SendType::Info, get_max_index(img), CODE_SIZE);
        state = State::ImageSending;
    }
    // 送信状態
    else if (state == State::ImageSending) {
        send_image_data(img);
        state = State::TerminationStandby;
    }
    // 画像送信終了待機状態
    else if (state == State::TerminationStandby) {
        if (val.substring(0, 1) == "R") {
            // 再送するインデックス番号をsize_t型で取得
            unsigned int resend_index = static_cast<size_t>(val.substring(1).toInt());
            send_packet(SendType::Image, resend_index, get_send_image_size(img, resend_index), img);
        } else if (val.substring(0, 1) == "E") {
            // 終了
            state = State::Standby;
        }
    }
}
