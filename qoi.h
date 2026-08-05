#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);

bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    // qoi-header part
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    // qoi-data part
    uint64_t px_num = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t px[4] = {0u, 0u, 0u, 255u};
    uint8_t prev[4] = {0u, 0u, 0u, 255u};
    int run = 0;

    for (uint64_t i = 0; i < px_num; ++i) {
        px[0] = QoiReadU8();
        px[1] = QoiReadU8();
        px[2] = QoiReadU8();
        if (channels == 4u) {
            px[3] = QoiReadU8();
        } else {
            px[3] = 255u;
        }

        if (px[0] == prev[0] && px[1] == prev[1] &&
            px[2] == prev[2] && px[3] == prev[3]) {
            ++run;
            if (run == 62) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            int idx = QoiColorHash(px[0], px[1], px[2], px[3]);
            if (history[idx][0] == px[0] && history[idx][1] == px[1] &&
                history[idx][2] == px[2] && history[idx][3] == px[3]) {
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(idx));
            } else {
                history[idx][0] = px[0];
                history[idx][1] = px[1];
                history[idx][2] = px[2];
                history[idx][3] = px[3];

                bool same_alpha = (channels != 4u) || (px[3] == prev[3]);
                if (same_alpha) {
                    int8_t vr = static_cast<int8_t>(px[0] - prev[0]);
                    int8_t vg = static_cast<int8_t>(px[1] - prev[1]);
                    int8_t vb = static_cast<int8_t>(px[2] - prev[2]);
                    if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
                        QoiWriteU8(QOI_OP_DIFF_TAG |
                                   static_cast<uint8_t>((vr + 2) << 4) |
                                   static_cast<uint8_t>((vg + 2) << 2) |
                                   static_cast<uint8_t>(vb + 2));
                    } else {
                        int8_t vgr = static_cast<int8_t>(vr - vg);
                        int8_t vgb = static_cast<int8_t>(vb - vg);
                        if (vgr > -9 && vgr < 8 && vgb > -9 && vgb < 8 &&
                            vg > -33 && vg < 32) {
                            QoiWriteU8(QOI_OP_LUMA_TAG |
                                       static_cast<uint8_t>(vg + 32));
                            QoiWriteU8(static_cast<uint8_t>((vgr + 8) << 4) |
                                       static_cast<uint8_t>(vgb + 8));
                        } else {
                            QoiWriteU8(QOI_OP_RGB_TAG);
                            QoiWriteU8(px[0]);
                            QoiWriteU8(px[1]);
                            QoiWriteU8(px[2]);
                        }
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(px[0]);
                    QoiWriteU8(px[1]);
                    QoiWriteU8(px[2]);
                    QoiWriteU8(px[3]);
                }
            }

            prev[0] = px[0];
            prev[1] = px[1];
            prev[2] = px[2];
            prev[3] = px[3];
        }
    }

    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
        run = 0;
    }

    // qoi-padding part
    for (int i = 0; i < static_cast<int>(sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0])); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    if (channels != 3u && channels != 4u) {
        return false;
    }
    if (colorspace != 0u && colorspace != 1u) {
        return false;
    }

    uint64_t px_num = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t px[4] = {0u, 0u, 0u, 255u};
    uint8_t prev[4] = {0u, 0u, 0u, 255u};

    uint64_t i = 0;
    while (i < px_num) {
        uint8_t tag = QoiReadU8();

        if (tag == QOI_OP_RGB_TAG) {
            px[0] = QoiReadU8();
            px[1] = QoiReadU8();
            px[2] = QoiReadU8();
            px[3] = prev[3];
        } else if (tag == QOI_OP_RGBA_TAG) {
            px[0] = QoiReadU8();
            px[1] = QoiReadU8();
            px[2] = QoiReadU8();
            px[3] = QoiReadU8();
        } else if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
            int run = (tag & 0x3f) + 1;
            px[0] = prev[0];
            px[1] = prev[1];
            px[2] = prev[2];
            px[3] = prev[3];
            for (int k = 0; k < run && i < px_num; ++k) {
                QoiWriteU8(px[0]);
                QoiWriteU8(px[1]);
                QoiWriteU8(px[2]);
                if (channels == 4u) QoiWriteU8(px[3]);
                ++i;
            }
            continue;
        } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
            int idx = tag & 0x3f;
            px[0] = history[idx][0];
            px[1] = history[idx][1];
            px[2] = history[idx][2];
            px[3] = history[idx][3];
        } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
            int dr = ((tag >> 4) & 0x3) - 2;
            int dg = ((tag >> 2) & 0x3) - 2;
            int db = (tag & 0x3) - 2;
            px[0] = static_cast<uint8_t>(prev[0] + dr);
            px[1] = static_cast<uint8_t>(prev[1] + dg);
            px[2] = static_cast<uint8_t>(prev[2] + db);
            px[3] = prev[3];
        } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
            int dg = (tag & 0x3f) - 32;
            uint8_t b2 = QoiReadU8();
            int vgr = ((b2 >> 4) & 0x0f) - 8;
            int vgb = (b2 & 0x0f) - 8;
            px[0] = static_cast<uint8_t>(prev[0] + dg + vgr);
            px[1] = static_cast<uint8_t>(prev[1] + dg);
            px[2] = static_cast<uint8_t>(prev[2] + dg + vgb);
            px[3] = prev[3];
        } else {
            return false;
        }

        int idx = QoiColorHash(px[0], px[1], px[2], px[3]);
        history[idx][0] = px[0];
        history[idx][1] = px[1];
        history[idx][2] = px[2];
        history[idx][3] = px[3];

        QoiWriteU8(px[0]);
        QoiWriteU8(px[1]);
        QoiWriteU8(px[2]);
        if (channels == 4u) QoiWriteU8(px[3]);

        prev[0] = px[0];
        prev[1] = px[1];
        prev[2] = px[2];
        prev[3] = px[3];
        ++i;
    }

    bool valid = true;
    for (int k = 0; k < static_cast<int>(sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0])); ++k) {
        if (QoiReadU8() != QOI_PADDING[k]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
