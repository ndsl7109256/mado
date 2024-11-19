/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twin_private.h"

/* angles are measured from -2048 .. 2048 */

twin_fixed_t twin_sin(twin_angle_t a)
{
    twin_fixed_t sin_val = 0;
    twin_sincos(a, &sin_val, NULL);
    return sin_val;
}

twin_fixed_t twin_cos(twin_angle_t a)
{
    twin_fixed_t cos_val = 0;
    twin_sincos(a, NULL, &cos_val);
    return cos_val;
}

twin_fixed_t twin_tan(twin_angle_t a)
{
    twin_fixed_t s, c;
    twin_sincos(a, &s, &c);

    if (c == 0) {
        if (s > 0)
            return TWIN_FIXED_MAX;
        return TWIN_FIXED_MIN;
    }
    if (s == 0)
        return 0;
    return ((s << 15) / c) << 1;
}

static inline twin_fixed_t sin_poly(twin_angle_t x)
{
    /* S(x) = x * 2^(-n) * (A1 - 2 ^ (q-p) * x * (2^-n) * x * 2^(-n) * (B1 - 2 ^
     * (-r) * x * 2 ^ (-n) * C1 * x)) * 2 ^ (a-q)
     * @n: the angle scale
     * @A: the amplitude
     * @p,q,r: the scaling factor
     *
     * A1 = 2^q * a5, B1 = 2 ^ p * b5, C1 = 2 ^ (r+p-n) * c5
     * where a5, b5, c5 are the coefficients for 5th-order polynomial
     * a5 = 4 * (3 / pi - 9 / 16)
     * b5 = 2 * a5 - 5 / 2
     * c5 = a5 - 3 / 2
     */
    const uint64_t A = 16, n = 10, p = 32, q = 31, r = 3;
    const uint64_t A1 = 3370945099, B1 = 2746362156, C1 = 2339369;
    uint64_t y = (C1 * x) >> n;
    y = B1 - ((x * y) >> r);
    y = x * (y >> n);
    y = x * (y >> n);
    y = A1 - (y >> (p - q));
    y = x * (y >> n);
    y = (y + (1UL << (q - A - 1))) >> (q - A); /* Rounding */
    return y;
}

void twin_sincos(twin_angle_t a, twin_fixed_t *sin, twin_fixed_t *cos)
{
    twin_fixed_t sin_val = 0, cos_val = 0;

    /* limit to [0..360) */
    a = a & (TWIN_ANGLE_360 - 1);
    int c = a > TWIN_ANGLE_90 && a < TWIN_ANGLE_270;
    /* special case for 90 degrees */
    if ((a & ~(TWIN_ANGLE_180)) == TWIN_ANGLE_90) {
        sin_val = TWIN_FIXED_ONE;
        cos_val = 0;
    } else {
        /* mirror second and third quadrant values across y axis */
        if (a & TWIN_ANGLE_90)
            a = TWIN_ANGLE_180 - a;
        twin_angle_t x = a & (TWIN_ANGLE_90 - 1);
        if (sin)
            sin_val = sin_poly(x);
        if (cos)
            cos_val = sin_poly(TWIN_ANGLE_90 - x);
    }
    if (sin) {
        /* mirror third and fourth quadrant values across x axis */
        if (a & TWIN_ANGLE_180)
            sin_val = -sin_val;
        *sin = sin_val;
    }
    if (cos) {
        /* mirror first and fourth quadrant values across y axis */
        if (c)
            cos_val = -cos_val;
        *cos = cos_val;
    }
}

const twin_angle_t atan_table[] = {
    0x0200,  // arctan(2^0)  = 45° -> 512
    0x0130,  // arctan(2^-1) = 26.565° -> 303
    0x009B,  // arctan(2^-2) = 14.036° -> 155
    0x004F,  // arctan(2^-3) = 7.125° -> 79
    0x0027,  // arctan(2^-4) = 3.576° -> 39
    0x0014,  // arctan(2^-5) = 1.790° -> 20
    0x000A,  // arctan(2^-6) = 0.895° -> 10
    0x0005,  // arctan(2^-7) = 0.448° -> 5
    0x0002,  // arctan(2^-8) = 0.224° -> 2
    0x0001,  // arctan(2^-9) = 0.112° -> 1
    0x0001,  // arctan(2^-10) = 0.056° -> 1
    0x0000,  // arctan(2^-11) = 0.028° -> 0
};

/*
twin_angle_t twin_atan2(twin_fixed_t y, twin_fixed_t x) {
    if (x == 0 && y == 0) return 0;

    twin_fixed_t current_x = x;
    twin_fixed_t current_y = y;
    twin_angle_t angle = 0;

    // 處理第二、三象限
    if (x < 0) {
        current_x = -current_x;
        current_y = -current_y;
        angle = (y >= 0) ? 2048 : -2048; // 180° -> 2048
    }

    for (int i = 0; i < 12; i++) {
        twin_fixed_t temp_x, temp_y;
        if (current_y > 0) {
            temp_x = current_x + (current_y >> i);
            temp_y = current_y - (current_x >> i);
            angle += (i < sizeof(atan_table)/sizeof(atan_table[0])) ? atan_table[i] : 0;
        } else {
            temp_x = current_x - (current_y >> i);
            temp_y = current_y + (current_x >> i);
            angle -= (i < sizeof(atan_table)/sizeof(atan_table[0])) ? atan_table[i] : 0;
        }
        current_x = temp_x;
        current_y = temp_y;
    }

    // 確保角度在 0-1024 範圍內
    if (angle < 0) angle = -angle;
    if (angle > 1024) angle = 2048 - angle;

    return angle;
}

twin_angle_t twin_atan2(twin_fixed_t y, twin_fixed_t x) {
    if (x == 0 && y == 0) return 0;

    twin_fixed_t current_x = x;
    twin_fixed_t current_y = y;
    twin_angle_t angle = 0;
	twin_angle_t plus = 0;
	int quad = 0;
	if( x >= 0 ){
		if(y >= 0){
			quad = 1;
		}else{
			quad = 4;
			current_y = -current_y;
		}
	}else{
		current_x = -current_x;
		if( y>=0){
			quad = 2;
		}else{
			quad = 3;
			current_y = -current_y;
		}
	}

    // CORDIC 迭代
    for (int i = 0; i < 12; i++) {
        twin_fixed_t temp_x, temp_y;
        if (current_y > 0) {
            temp_x = current_x + (current_y >> i);
            temp_y = current_y - (current_x >> i);
            angle += (i < sizeof(atan_table)/sizeof(atan_table[0])) ? atan_table[i] : 0;
        } else {
            temp_x = current_x - (current_y >> i);
            temp_y = current_y + (current_x >> i);
            angle -= (i < sizeof(atan_table)/sizeof(atan_table[0])) ? atan_table[i] : 0;
        }
        current_x = temp_x;
        current_y = temp_y;
    }

	if(quad == 1){
    	return angle;
	}else if(quad == 2){
    	return 2048 - angle;
	}else if(quad == 3){
    	return 2048 + angle;
	}else if(quad == 4){
    	return 4096 - angle;
	}
}

*/

twin_angle_t twin_atan2_first_quadrant(twin_fixed_t y, twin_fixed_t x) {
    if (x == 0 && y == 0) return 0;
    if (x == 0) return 1024;  // 90度
    if (y == 0) return 0;     // 0度

    twin_fixed_t current_x = x;
    twin_fixed_t current_y = y;
    twin_angle_t angle = 0;

    // CORDIC 迭代
    for (int i = 0; i < 12; i++) {
        twin_fixed_t temp_x, temp_y;
        if (current_y > 0) {
            temp_x = current_x + (current_y >> i);
            temp_y = current_y - (current_x >> i);
            angle += atan_table[i];
        } else {
            temp_x = current_x - (current_y >> i);
            temp_y = current_y + (current_x >> i);
            angle -= atan_table[i];
        }
        current_x = temp_x;
        current_y = temp_y;
    }

    return angle;
}

twin_angle_t twin_atan2(twin_fixed_t y, twin_fixed_t x) {
    // 特殊情況處理
    if (x == 0 && y == 0) return 0;
    if (x == 0) return (y > 0) ? 1024 : 3072;  // 90° or 270°
    if (y == 0) return (x > 0) ? 0 : 2048;     // 0° or 180°

    // 記錄原始象限
    int quadrant;
    twin_fixed_t abs_x = x;
    twin_fixed_t abs_y = y;

    // 確定象限並轉換到第一象限
    if (x >= 0 && y >= 0) {
        quadrant = 1;
    } else if (x < 0 && y >= 0) {
        quadrant = 2;
        abs_x = -x;
    } else if (x < 0 && y < 0) {
        quadrant = 3;
        abs_x = -x;
        abs_y = -y;
    } else {  // x >= 0 && y < 0
        quadrant = 4;
        abs_y = -y;
    }

    // 在第一象限計算角度
    twin_angle_t angle = twin_atan2_first_quadrant(abs_y, abs_x);
	/*
	switch (quadrant) {
			case 1:  
			log_info("1 %d %d %d",y, x, angle);
			break;
			case 2:  
			log_info("2 %d %d %d",y, x, 2048 - angle);
			break;
			case 3:  
			log_info("3 %d %d %d",y, x, 2048 + angle);
			break;
			case 4:  
			log_info("4 %d %d %d",y, x, 4096 - angle);
			break;
	}
	*/
			// 根據原始象限調整角度
    switch (quadrant) {
        case 1:  return angle;
        case 2:  return 2048 - angle;
        case 3:  return 2048 + angle;
        case 4:  return 4096 - angle;
        default: return 0;
    }
}


twin_angle_t twin_acos(twin_fixed_t x) {
    // 確保 x 在 [-1, 1] 範圍內
    if (x <= -TWIN_FIXED_ONE) return 2048;      // 180度
    if (x >= TWIN_FIXED_ONE) return 0;          // 0度

    // 計算 √(1-x²)
    twin_fixed_t y = twin_fixed_sqrt(TWIN_FIXED_ONE - twin_fixed_mul(x, x));

    // 使用 atan2 計算結果
    twin_angle_t angle;

    // 第一象限
    if (x >= 0) {
        angle = twin_atan2_first_quadrant(y, x);
    }
    // 第二象限
    else {
        angle = 2048 - twin_atan2_first_quadrant(y, -x);
    }

    return angle;
}

