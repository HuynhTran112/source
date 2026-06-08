/**
 * @file gps_nmea.c
 * @brief Hiện thực thư viện xử lý và bóc tách dữ liệu bản tin GPS chuẩn NMEA 0183
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hướng dẫn thuật toán chi tiết:
 * 1. Tính toán Checksum: Thực hiện phép XOR liên tiếp mọi ký tự nằm giữa '$' và '*' trong bản tin NMEA thô,
 *    sau đó so sánh kết quả với 2 ký tự mã Hex cuối chuỗi để đảm bảo dữ liệu không bị sai lệch khi truyền UART.
 * 2. Cắt chuỗi thành các trường dữ liệu bằng hàm tách dấu phẩy (Split).
 * 3. Nhận dạng bản tin:
 *    - RMC (Recommended Minimum Navigation Information): Trích xuất trạng thái Fix (A/V), vĩ độ, kinh độ và tốc độ di chuyển.
 *    - GGA (Global Positioning System Fix Data): Trích xuất số lượng vệ tinh đang kết nối, chất lượng Fix và sai số HDOP.
 *    - GSA (GPS DOP and Active Satellites): Trích xuất sai số vị trí PDOP, HDOP, VDOP.
 * 4. Quy đổi hệ tọa độ: Chuyển đổi tọa độ định dạng DDMM.MMMM (Độ Phút) sang định dạng Độ thập phân (Decimal Degrees)
 *    theo công thức: Dec_Degrees = Degrees + (Minutes / 60.0), đồng thời đảo dấu nếu ở hướng Nam (S) hoặc Tây (W).
 */

#include "gps_nmea.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Đổi một ký tự mã Hex (0-9, A-F) sang số nguyên tương ứng
 * @param c Ký tự đầu vào
 * @return int Giá trị số (0-15), hoặc -1 nếu ký tự không hợp lệ
 */
static int gps_nmea_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool watch_gps_nmea_has_valid_checksum(const char *line) {
    if (!line || line[0] != '$') return false;
    const char *star = strchr(line, '*');
    if (!star || star == line + 1) return false;

    /* Đọc giá trị Checksum đính kèm ở 2 ký tự sau dấu '*' */
    int hi = gps_nmea_hex_nibble(star[1]);
    int lo = gps_nmea_hex_nibble(star[2]);
    if (hi < 0 || lo < 0) return false;

    /* Tính toán mã Checksum thực tế bằng cách XOR các ký tự nằm giữa '$' và '*' */
    unsigned char calc = 0;
    for (const char *p = line + 1; p < star; p++) {
        calc ^= (unsigned char)*p;
    }

    return calc == (unsigned char)((hi << 4) | lo);
}

/**
 * @brief Xác minh checksum và loại bỏ phần đuôi checksum khỏi bản tin gốc để phục vụ tách chuỗi
 * @return true Checksum hợp lệ
 * @return false Checksum bị sai
 */
static bool gps_nmea_strip_valid_checksum(char *line) {
    char *star = strchr(line, '*');
    if (!star || star == line + 1) return false;

    if (!watch_gps_nmea_has_valid_checksum(line)) return false;

    *star = '\0'; // Kết thúc chuỗi tại dấu '*' để ẩn đuôi checksum đi
    return true;
}

/**
 * @brief Quy đổi tọa độ từ định dạng Độ Phút (DDMM.MMMM) sang Độ Thập phân (Decimal Degrees)
 * @param dm Chuỗi số chứa tọa độ dạng DDMM.MMMM (ví dụ: "2101.5432")
 * @param hemi Hướng la bàn chỉ định ('N': Bắc, 'S': Nam, 'E': Đông, 'W': Tây)
 * @return double Giá trị tọa độ thực tế dạng số thực (ví dụ: 21.02572)
 */
static double gps_nmea_degmin_to_decimal(const char *dm, const char hemi) {
    if (!dm || dm[0] == '\0') return 0.0;

    const double raw = atof(dm);
    const int deg = (int)(raw / 100.0); // Tách phần độ (trước hàng trăm)
    const double minutes = raw - ((double)deg * 100.0); // Tách phần phút dư
    double out = (double)deg + (minutes / 60.0);        // Đổi phút thành thập phân của độ (chia cho 60)
    
    if (hemi == 'S' || hemi == 'W') {
        out = -out; // Nếu ở Nam bán cầu hoặc Tây bán cầu: Mang dấu âm
    }
    return out;
}

/**
 * @brief So sánh xem đầu bản tin có khớp mã lệnh chỉ định (RMC, GGA, GSA)
 */
static bool gps_nmea_is_sentence(const char *line, const char *type) {
    return line && type && line[0] == '$' && strlen(line) >= 6 &&
           line[3] == type[0] && line[4] == type[1] && line[5] == type[2];
}

/**
 * @brief Tách bản tin NMEA bằng dấu phẩy ',' thành các trường con lưu vào mảng pointers
 * @return int Tổng số lượng trường dữ liệu phân tách được
 */
static int gps_nmea_split(char *line, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    fields[count++] = p;
    while (*p && count < max_fields) {
        if (*p == ',') {
            *p = '\0'; // Thay thế dấu phẩy bằng ký tự null để biến trường đó thành chuỗi độc lập
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

bool watch_gps_nmea_parse_line(char *line, watch_gps_nmea_data_t *out) {
    if (!line || !out || line[0] != '$') return false;
    memset(out, 0, sizeof(*out));
    if (!gps_nmea_strip_valid_checksum(line)) return false;

    char *fields[24];
    int num_fields = gps_nmea_split(line, fields, 24);
    if (num_fields < 1) return false;

    /* --- TRƯỜNG HỢP 1: BẢN TIN $GPRMC (Độ lệch tối thiểu) --- */
    if (gps_nmea_is_sentence(fields[0], "RMC")) {
        char status = 'V';
        char lat_s[20] = {0};
        char lat_h = 'N';
        char lon_s[20] = {0};
        char lon_h = 'E';
        char spd_s[16] = {0};

        if (num_fields > 2 && fields[2][0] != '\0') status = fields[2][0]; // Trạng thái khóa: 'A': Hợp lệ, 'V': Không hợp lệ
        if (num_fields > 3) strncpy(lat_s, fields[3], sizeof(lat_s) - 1);  // Chuỗi vĩ độ thô
        if (num_fields > 4 && fields[4][0] != '\0') lat_h = fields[4][0];   // Hướng vĩ độ (N/S)
        if (num_fields > 5) strncpy(lon_s, fields[5], sizeof(lon_s) - 1);  // Chuỗi kinh độ thô
        if (num_fields > 6 && fields[6][0] != '\0') lon_h = fields[6][0];   // Hướng kinh độ (E/W)
        if (num_fields > 7) strncpy(spd_s, fields[7], sizeof(spd_s) - 1);  // Tốc độ thô (đơn vị: Knots - hải lý/giờ)

        out->has_rmc = true;
        out->fix_valid = (status == 'A');
        
        if (out->fix_valid) {
            out->latitude_deg = gps_nmea_degmin_to_decimal(lat_s, lat_h);
            out->longitude_deg = gps_nmea_degmin_to_decimal(lon_s, lon_h);
            /* Quy đổi vận tốc từ Hải lý/giờ (Knots) sang km/h: 1 knot = 1.852 km/h */
            out->speed_kmh = (float)(atof(spd_s) * 1.852f);
        }
        return true;
    }

    /* --- TRƯỜNG HỢP 2: BẢN TIN $GPGGA (Bộ khóa hệ thống) --- */
    if (gps_nmea_is_sentence(fields[0], "GGA")) {
        char fix_q_s[8] = {0};
        char sats_s[8] = {0};
        char hdop_s[12] = {0};

        if (num_fields > 6) strncpy(fix_q_s, fields[6], sizeof(fix_q_s) - 1); // Chất lượng Fix
        if (num_fields > 7) strncpy(sats_s, fields[7], sizeof(sats_s) - 1);   // Số lượng vệ tinh
        if (num_fields > 8) strncpy(hdop_s, fields[8], sizeof(hdop_s) - 1);   // Sai số hình học HDOP

        out->has_gga = true;
        out->fix_quality = (unsigned int)atoi(fix_q_s);
        out->fix_valid = (out->fix_quality > 0);
        out->satellites = (unsigned int)atoi(sats_s);
        out->hdop = (float)atof(hdop_s);
        return true;
    }

    /* --- TRƯỜNG HỢP 3: BẢN TIN $GPGSA (Sai số hình học DOP) --- */
    if (gps_nmea_is_sentence(fields[0], "GSA")) {
        char fix_type_s[8] = {0};
        char pdop_s[12] = {0};
        char hdop_s[12] = {0};
        char vdop_s[12] = {0};

        if (num_fields > 2) strncpy(fix_type_s, fields[2], sizeof(fix_type_s) - 1); // Loại định vị (1: No fix, 2: 2D, 3: 3D)
        if (num_fields > 15) strncpy(pdop_s, fields[15], sizeof(pdop_s) - 1);       // PDOP tổng quát
        if (num_fields > 16) strncpy(hdop_s, fields[16], sizeof(hdop_s) - 1);       // HDOP mặt phẳng ngang
        if (num_fields > 17) strncpy(vdop_s, fields[17], sizeof(vdop_s) - 1);       // VDOP chiều dọc độ cao

        out->fix_valid = atoi(fix_type_s) >= 2;
        out->pdop = (float)atof(pdop_s);
        out->hdop = (float)atof(hdop_s);
        out->vdop = (float)atof(vdop_s);
        return true;
    }

    return false;
}
