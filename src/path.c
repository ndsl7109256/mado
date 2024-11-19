/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>
#include <math.h>

#include "twin_private.h"

static int _twin_current_subpath_len(twin_path_t *path)
{
    int start;

    if (path->nsublen)
        start = path->sublen[path->nsublen - 1];
    else
        start = 0;
    return path->npoints - start;
}

twin_spoint_t _twin_path_current_spoint(twin_path_t *path)
{
    if (!path->npoints)
        twin_path_move(path, 0, 0);
    return path->points[path->npoints - 1];
}

twin_spoint_t _twin_path_subpath_first_spoint(twin_path_t *path)
{
    int start;

    if (!path->npoints)
        twin_path_move(path, 0, 0);

    if (path->nsublen)
        start = path->sublen[path->nsublen - 1];
    else
        start = 0;

    return path->points[start];
}

void _twin_path_sfinish(twin_path_t *path)
{
    switch (_twin_current_subpath_len(path)) {
    case 1:
        path->npoints--;
    case 0:
        return;
    }

    if (path->nsublen == path->size_sublen) {
        int size_sublen;
        int *sublen;

        if (path->size_sublen > 0)
            size_sublen = path->size_sublen * 2;
        else
            size_sublen = 1;
        if (path->sublen)
            sublen = realloc(path->sublen, size_sublen * sizeof(int));
        else
            sublen = malloc(size_sublen * sizeof(int));
        if (!sublen)
            return;
        path->sublen = sublen;
        path->size_sublen = size_sublen;
    }
    path->sublen[path->nsublen] = path->npoints;
    path->nsublen++;
}

void _twin_path_smove(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    switch (_twin_current_subpath_len(path)) {
    default:
        _twin_path_sfinish(path);
        fallthrough;
    case 0:
        _twin_path_sdraw(path, x, y);
        break;
    case 1:
        path->points[path->npoints - 1].x = x;
        path->points[path->npoints - 1].y = y;
        break;
    }
}

void _twin_path_sdraw(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y)
{
    if (_twin_current_subpath_len(path) > 0 &&
        path->points[path->npoints - 1].x == x &&
        path->points[path->npoints - 1].y == y)
        return;
    if (path->npoints == path->size_points) {
        int size_points;
        twin_spoint_t *points;

        if (path->size_points > 0)
            size_points = path->size_points * 2;
        else
            size_points = 16;
        if (path->points)
            points = realloc(path->points, size_points * sizeof(twin_spoint_t));
        else
            points = malloc(size_points * sizeof(twin_spoint_t));
        if (!points)
            return;
        path->points = points;
        path->size_points = size_points;
    }
    path->points[path->npoints].x = x;
    path->points[path->npoints].y = y;
    path->npoints++;
}

void twin_path_move(twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
	path->cur_x = x;
	path->cur_y = y;
    _twin_path_smove(path, _twin_matrix_x(&path->state.matrix, x, y),
                     _twin_matrix_y(&path->state.matrix, x, y));
}

void twin_path_rmove(twin_path_t *path, twin_fixed_t dx, twin_fixed_t dy)
{
    twin_spoint_t here = _twin_path_current_spoint(path);
    _twin_path_smove(path,
                     here.x + _twin_matrix_dx(&path->state.matrix, dx, dy),
                     here.y + _twin_matrix_dy(&path->state.matrix, dx, dy));
}

void twin_path_draw(twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
	path->cur_x = x;
	path->cur_y = y;
    _twin_path_sdraw(path, _twin_matrix_x(&path->state.matrix, x, y),
                     _twin_matrix_y(&path->state.matrix, x, y));
}

static void twin_path_draw_polar(twin_path_t *path, twin_angle_t deg)
{
    twin_fixed_t s, c;
    twin_sincos(deg, &s, &c);
    twin_path_draw(path, c, s);
}

void twin_path_rdraw(twin_path_t *path, twin_fixed_t dx, twin_fixed_t dy)
{
    twin_spoint_t here = _twin_path_current_spoint(path);
    _twin_path_sdraw(path,
                     here.x + _twin_matrix_dx(&path->state.matrix, dx, dy),
                     here.y + _twin_matrix_dy(&path->state.matrix, dx, dy));
}

void twin_path_close(twin_path_t *path)
{
    twin_spoint_t f;

    switch (_twin_current_subpath_len(path)) {
    case 0:
    case 1:
        break;
    default:
        f = _twin_path_subpath_first_spoint(path);
        _twin_path_sdraw(path, f.x, f.y);
        break;
    }
}


void twin_path_circle_(twin_path_t *path,
                      twin_fixed_t x,
                      twin_fixed_t y,
                      twin_fixed_t radius)
{
    twin_spoint_t current = _twin_path_current_spoint(path);
	log_info("%d %d", current.x, current.y);
    twin_path_ellipse(path, x, y, radius, radius);
}

void twin_path_circle(twin_path_t *path,
                      twin_fixed_t x,
                      twin_fixed_t y,
                      twin_fixed_t radius)
{
    twin_spoint_t current = _twin_path_current_spoint(path);
    twin_path_ellipse(path, x, y, radius, radius);
}

void twin_path_ellipse(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t x_radius,
                       twin_fixed_t y_radius)
{
    twin_path_move(path, x + x_radius, y);
    twin_path_arc(path, x, y, x_radius, y_radius, 0, TWIN_ANGLE_360);
    twin_path_close(path);
}

#define twin_fixed_abs(f) ((f) < 0 ? -(f) : (f))

static twin_fixed_t _twin_matrix_max_radius(twin_matrix_t *m)
{
    return (twin_fixed_abs(m->m[0][0]) + twin_fixed_abs(m->m[0][1]) +
            twin_fixed_abs(m->m[1][0]) + twin_fixed_abs(m->m[1][1]));
}

void twin_path_arc(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius,
                   twin_angle_t start,
                   twin_angle_t extent)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_scale(path, x_radius, y_radius);

    twin_fixed_t max_radius = _twin_matrix_max_radius(&path->state.matrix);
    int32_t sides = max_radius / twin_sfixed_to_fixed(TWIN_SFIXED_TOLERANCE);
    if (sides > 1024)
        sides = 1024;

    /* Calculate the nearest power of 2 that is >= sides. */
    int32_t n = (sides > 1) ? (31 - twin_clz(sides) + 1) : 2;

    twin_angle_t step = TWIN_ANGLE_360 >> n;
    twin_angle_t inc = step;
    twin_angle_t epsilon = 1;
    if (extent < 0) {
        inc = -inc;
        epsilon = -1;
    }

    twin_angle_t first = (start + inc - epsilon) & ~(step - 1);
    twin_angle_t last = (start + extent - inc + epsilon) & ~(step - 1);

    if (first != start)
        twin_path_draw_polar(path, start);

    for (twin_angle_t a = first; a != last; a += inc){
        twin_path_draw_polar(path, a);
	}

    if (last != start + extent)
        twin_path_draw_polar(path, start + extent);

    twin_path_set_matrix(path, save);
}

twin_fixed_t twin_fixed_distance(twin_fixed_t x1, twin_fixed_t y1, twin_fixed_t x2, twin_fixed_t y2){
	twin_fixed_t dx = x2 - x1;
    twin_fixed_t dy = y2 - y1;
    twin_fixed_t d = twin_fixed_sqrt(twin_fixed_mul(dx, dx) +
                                    twin_fixed_mul(dy, dy));
	return d;
}

void twin_path_arc_circle(twin_path_t *path,
                         bool large_arc,
                         bool sweep,
                         twin_fixed_t radius,
						 twin_fixed_t cur_x,
						 twin_fixed_t cur_y,
                         twin_fixed_t target_x,
                         twin_fixed_t target_y)
{
    // 獲取當前點位置
    twin_spoint_t current = _twin_path_current_spoint(path);
    twin_fixed_t x1 = cur_x;
    twin_fixed_t y1 = cur_y;
	twin_fixed_t tx = target_x;
	twin_fixed_t ty = target_y;

    // 計算兩點之間的距離
    twin_fixed_t dx = tx - x1;
    twin_fixed_t dy = ty - y1;
	twin_fixed_t d = twin_fixed_distance(x1, y1, tx, ty);
	/*
    twin_fixed_t d = twin_fixed_sqrt(twin_fixed_mul(dx, dx) +
                                    twin_fixed_mul(dy, dy));
	*/
	twin_fixed_t half_d = twin_fixed_mul(d, TWIN_FIXED_HALF);
	//log_info("%d %d", twin_fixed_mul(dx, dx), twin_fixed_mul(dy, dy));
	log_info("p0 (%f,%f) p1( %f, %f) %f", x1/65536.0, y1/65536.0, tx/65536.0, ty/65536.0, d/65536.0);
    if (d == 0) return; // 起點終點相同

    // 如果距離大於直徑,調整半徑
    if (d > twin_fixed_mul(radius, twin_int_to_fixed(2))) {
        radius = half_d;
    }

    // 計算圓心到中點的距離
    twin_fixed_t h = twin_fixed_sqrt(twin_fixed_mul(radius, radius) -
                                    twin_fixed_mul(half_d, half_d));

    // 中點
    //twin_fixed_t mx = x1 + dx/2;
    twin_fixed_t mx = x1 + twin_fixed_mul(dx, TWIN_FIXED_HALF);
    //twin_fixed_t my = y1 + dy/2;
    twin_fixed_t my = y1 + twin_fixed_mul(dy, TWIN_FIXED_HALF);

    // 根據 sweep 決定圓心位置
    twin_fixed_t x0, y0;
    if (sweep ^ large_arc) {
        //x0 = mx - h * dy / d;
        x0 = mx + twin_fixed_div(twin_fixed_mul(h ,dy) ,d);
        //y0 = my + h * dx / d;
        y0 = my - twin_fixed_div(twin_fixed_mul(h ,dx) ,d);
    } else {
        //x0 = mx + h * dy / d;
        x0 = mx - twin_fixed_div(twin_fixed_mul(h ,dy) ,d);
        //y0 = my - h * dx / d;
        y0 = my + twin_fixed_div(twin_fixed_mul(h ,dx) ,d);
    }

    // 使用 arcsin 計算角度
    // 對於起始點
    twin_fixed_t rx = x1 - x0;
    twin_fixed_t ry = y1 - y0;
    twin_angle_t start_angle;

    // 計算 sin 值
    //twin_fixed_t sin_theta = ry / radius;
	/*
    twin_fixed_t sin_theta = twin_fixed_div(ry, radius);
    // 確保 sin_theta 在 [-1,1] 範圍內
    if (sin_theta > TWIN_FIXED_ONE) sin_theta = TWIN_FIXED_ONE;
    if (sin_theta < -TWIN_FIXED_ONE) sin_theta = -TWIN_FIXED_ONE;
	*/

    start_angle = twin_atan2(ry, rx);
	log_info("rx %f, ry %f center :( %f, %f )angle %d",rx/65536.0, ry/65536.0, x0/65536.0, y0/65536.0, start_angle * 90 / 1024);
    // 根據 x 座標調整角度
	/*
    if (rx < 0) {
        start_angle = TWIN_ANGLE_180 - start_angle;
    }
	*/

    // 對於終點
    rx = target_x - x0;
    ry = target_y - y0;
    twin_angle_t end_angle;

    //sin_theta = ry / radius;
	/*
	sin_theta = twin_fixed_div(ry, radius);
    if (sin_theta > TWIN_FIXED_ONE) sin_theta = TWIN_FIXED_ONE;
    if (sin_theta < -TWIN_FIXED_ONE) sin_theta = -TWIN_FIXED_ONE;
	*/
    end_angle = twin_atan2(ry, rx);
	log_info("center :( %f, %f )angle %d-> %d", x0/65536.0, y0/65536.0, start_angle * 90 / 1024, end_angle * 90 / 1024);
	/*
	if (rx < 0) {
        end_angle = TWIN_ANGLE_180 - end_angle;
    }
	*/
    // 計算角度範圍
    twin_angle_t angle_diff = end_angle - start_angle;
	twin_angle_t extent = 0;
	if(angle_diff < 0)
		angle_diff += TWIN_ANGLE_360;
	
	if (large_arc) {
        if (angle_diff < TWIN_ANGLE_180) {
            // 大弧且角度差小於180度
            extent = TWIN_ANGLE_360 - angle_diff;
        } else {
            // 大弧且角度差大於等於180度
            extent = angle_diff;
        }
    } else {
        if (angle_diff > TWIN_ANGLE_180) {
            // 小弧且角度差大於180度
            extent = angle_diff - TWIN_ANGLE_360; 
        } else {
            // 小弧且角度差小於等於180度
            extent = angle_diff;
        }
    }	
	if(sweep && extent > 0)
		extent = -extent;
	
    // 使用 twin_path_arc 繪製圓弧
	log_info("%d %d ", start_angle * 90 / 1024, extent * 90 / 1024);
    twin_path_arc(path, x0, y0, radius, radius, start_angle, extent);
}

#define twin_fixed_abs(f) ((f) < 0 ? -(f) : (f))

twin_angle_t at(twin_fixed_t y, twin_fixed_t x){
	float yf  = y / 65535.0;
	float xf  = x / 65535.0;
	float radian = atan2(yf, xf);
	float degree = radian * (180.0f / M_PI);
	return degree * 1024 / 90;
}

/*
static twin_angle_t vector_angle(twin_fixed_t ux, twin_fixed_t uy, twin_fixed_t vx, twin_fixed_t vy)
{
	return at(uy, ux) - at(vy, vx);	
	//return twin_atan2(uy, ux) - twin_atan2(vy, vx);
}
*/

twin_angle_t vector_angle(twin_fixed_t ux, twin_fixed_t uy, twin_fixed_t vx, twin_fixed_t vy) {
    // 計算向量的點積
    twin_fixed_t dot = twin_fixed_mul(ux, vx) + twin_fixed_mul(uy, vy);
    
    // 計算向量長度
    twin_fixed_t ua = twin_fixed_sqrt(twin_fixed_mul(ux, ux) + twin_fixed_mul(uy, uy));
    twin_fixed_t va = twin_fixed_sqrt(twin_fixed_mul(vx, vx) + twin_fixed_mul(vy, vy));
    
    // 計算 cos(theta) = dot / (|u| * |v|)
    twin_fixed_t cos_theta = twin_fixed_div(dot, twin_fixed_mul(ua, va));
    
    // 計算叉積的符號來確定角度的方向
    twin_fixed_t cross = twin_fixed_mul(ux, vy) - twin_fixed_mul(uy, vx);
    
    // 使用 acos 計算角度
    twin_angle_t angle = twin_acos(cos_theta);
    
    // 根據叉積確定角度的符號
    return (cross < 0) ? -angle : angle;
}


typedef struct {
	twin_fixed_t cx, cy;
	twin_angle_t start, extent;
} twin_ellipse_para_t;

twin_ellipse_para_t get_center_parameters(
    twin_fixed_t x1, twin_fixed_t y1,
    twin_fixed_t x2, twin_fixed_t y2,
    bool fa, bool fs,
    twin_fixed_t rx, twin_fixed_t ry,
    twin_fixed_t phi)
{
	fs = !fs;
	//log_info("Initial points: (%.4f, %.4f) to (%.4f, %.4f)", 
    //         x1/65536.0, y1/65536.0, x2/65536.0, y2/65536.0);
    twin_fixed_t sin_phi = twin_sin(phi);
    twin_fixed_t cos_phi = twin_cos(phi);

    // Step 1: simplify through translation/rotation
    twin_fixed_t x = twin_fixed_mul(cos_phi, twin_fixed_mul(x1 - x2, TWIN_FIXED_HALF)) +
                     twin_fixed_mul(sin_phi, twin_fixed_mul(y1 - y2, TWIN_FIXED_HALF));

    twin_fixed_t y = twin_fixed_mul(-sin_phi, twin_fixed_mul(x1 - x2, TWIN_FIXED_HALF)) +
                     twin_fixed_mul(cos_phi, twin_fixed_mul(y1 - y2, TWIN_FIXED_HALF));

	//log_info("After translation/rotation: (%.4f, %.4f)", x/65536.0, y/65536.0);
    // Calculate squares
    twin_fixed_t px = twin_fixed_mul(x, x);
    twin_fixed_t py = twin_fixed_mul(y, y);
    twin_fixed_t prx = twin_fixed_mul(rx, rx);
    twin_fixed_t pry = twin_fixed_mul(ry, ry);
	//log_info("Squares: px=%.4f py=%.4f prx=%.4f pry=%.4f",
    //         px/65536.0, py/65536.0, prx/65536.0, pry/65536.0);

    // Correct out-of-range radii
    twin_fixed_t L = twin_fixed_div(px, prx) + twin_fixed_div(py, pry);

	//log_info("L value: %.4f", L/65536.0);
    if (L > TWIN_FIXED_ONE) {
        twin_fixed_t sqrt_L = twin_fixed_sqrt(L);
        rx = twin_fixed_mul(sqrt_L, twin_fixed_abs(rx));
        ry = twin_fixed_mul(sqrt_L, twin_fixed_abs(ry));
		//log_info("Corrected radii: rx=%.4f ry=%.4f", rx/65536.0, ry/65536.0);
    } else {
        rx = twin_fixed_abs(rx);
        ry = twin_fixed_abs(ry);
    }

    // Step 2 + 3: compute center
    twin_fixed_t sign = (fa == fs) ? -1 : 1;

    twin_fixed_t numerator = twin_fixed_mul(prx, pry) - twin_fixed_mul(prx, py) - twin_fixed_mul(pry, px);

    twin_fixed_t denominator = twin_fixed_mul(prx, py) + twin_fixed_mul(pry, px);
	//log_info("Center calculation: num=%.4f den=%.4f", 
    //         numerator/65536.0, denominator/65536.0);
    twin_fixed_t M = sign * twin_fixed_sqrt(twin_fixed_div(numerator, denominator));
	//log_info("M value: %.4f", M/65536.0);
    twin_fixed_t _cx = twin_fixed_mul(M, twin_fixed_div(twin_fixed_mul(rx, y), ry));
    twin_fixed_t _cy = twin_fixed_mul(M, twin_fixed_div(twin_fixed_mul(-ry, x), rx));
    //log_info("Intermediate center: (%.4f, %.4f)", _cx/65536.0, _cy/65536.0);

	twin_ellipse_para_t ret;
    ret.cx = twin_fixed_mul(cos_phi, _cx) - twin_fixed_mul(sin_phi, _cy) +
          twin_fixed_mul(x1 + x2, TWIN_FIXED_HALF);

    ret.cy = twin_fixed_mul(sin_phi, _cx) + twin_fixed_mul(cos_phi, _cy) +
          twin_fixed_mul(y1 + y2, TWIN_FIXED_HALF);
    
    //log_info("Final center: (%.4f, %.4f)", ret.cx/65536.0, ret.cy/65536.0);

    // Step 4: compute θ and dθ
    ret.start = vector_angle(
        TWIN_FIXED_ONE, 0,
        twin_fixed_div(x - _cx, rx),
        twin_fixed_div(y - _cy, ry)
    );
	//ret.start = ret.start * sign;

    twin_angle_t _dTheta = vector_angle(
        twin_fixed_div(x - _cx, rx),
        twin_fixed_div(y - _cy, ry),
        twin_fixed_div(-x - _cx, rx),
        twin_fixed_div(-y - _cy, ry)
    );

    //log_info("Angles before adjustment: start=%.4f dTheta=%.4f", 
    //         ret.start*90.0/1024.0, _dTheta*90.0/1024.0);
    if (!fs && _dTheta > TWIN_ANGLE_0)
        _dTheta -= TWIN_ANGLE_360;
    if (fs && _dTheta < TWIN_ANGLE_0)
        _dTheta += TWIN_ANGLE_360;
	/*
	if((fs && _dTheta > 0) || (!fs && _dTheta < 0))
		_dTheta = -_dTheta;
	*/
	ret.start %= TWIN_ANGLE_360;
	_dTheta %= TWIN_ANGLE_360;
	//log_info("Final angles: start=%.4f extent=%.4f",
    //         ret.start*90.0/1024.0, _dTheta*90.0/1024.0);

	ret.extent = _dTheta;
	return ret;
}

void twin_path_arc_ellipse(twin_path_t *path,
                         bool large_arc,
                         bool sweep,
                         twin_fixed_t radius_x,
                         twin_fixed_t radius_y,
                         twin_fixed_t cur_x,
                         twin_fixed_t cur_y,
                         twin_fixed_t target_x,
                         twin_fixed_t target_y,
						 twin_angle_t rotation)
{
	twin_ellipse_para_t para;
	para = get_center_parameters(cur_x, cur_y, target_x, target_y, large_arc, sweep, radius_x, radius_y, rotation);
	//log_info("%d %d %d %d", para.cx/65535.0, para.cy/65535.0, para.start, para.extent);
	twin_matrix_t save = twin_path_current_matrix(path);
	twin_path_translate(path, para.cx, para.cy);
	twin_path_rotate(path, rotation);
	twin_path_translate(path, -para.cx, -para.cy);
	twin_path_arc(path, para.cx, para.cy, radius_x, radius_y, para.start, para.extent);

	twin_path_set_matrix(path, save);
}


void twin_path_arc_circle2(twin_path_t *path,
                         bool large_arc,
                         bool sweep,
                         twin_fixed_t radius,
                         twin_fixed_t cur_x,
                         twin_fixed_t cur_y,
                         twin_fixed_t target_x,
                         twin_fixed_t target_y)
{
	twin_path_arc_ellipse(path, large_arc, sweep, radius, radius, cur_x, cur_y, target_x, target_y, TWIN_ANGLE_0);
}

void twin_path_rectangle(twin_path_t *path,
                         twin_fixed_t x,
                         twin_fixed_t y,
                         twin_fixed_t w,
                         twin_fixed_t h)
{
    twin_path_move(path, x, y);
    twin_path_draw(path, x + w, y);
    twin_path_draw(path, x + w, y + h);
    twin_path_draw(path, x, y + h);
    twin_path_close(path);
}

void twin_path_rounded_rectangle(twin_path_t *path,
                                 twin_fixed_t x,
                                 twin_fixed_t y,
                                 twin_fixed_t w,
                                 twin_fixed_t h,
                                 twin_fixed_t x_radius,
                                 twin_fixed_t y_radius)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_move(path, 0, y_radius);
    twin_path_arc(path, x_radius, y_radius, x_radius, y_radius, TWIN_ANGLE_180,
                  TWIN_ANGLE_90);
    twin_path_draw(path, w - x_radius, 0);
    twin_path_arc(path, w - x_radius, y_radius, x_radius, y_radius,
                  TWIN_ANGLE_270, TWIN_ANGLE_90);
    twin_path_draw(path, w, h - y_radius);
    twin_path_arc(path, w - x_radius, h - y_radius, x_radius, y_radius,
                  TWIN_ANGLE_0, TWIN_ANGLE_90);
    twin_path_draw(path, x_radius, h);
    twin_path_arc(path, x_radius, h - y_radius, x_radius, y_radius,
                  TWIN_ANGLE_90, TWIN_ANGLE_90);
    twin_path_close(path);
    twin_path_set_matrix(path, save);
}

void twin_path_lozenge(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t w,
                       twin_fixed_t h)
{
    twin_fixed_t radius;

    if (w > h)
        radius = h / 2;
    else
        radius = w / 2;
    twin_path_rounded_rectangle(path, x, y, w, h, radius, radius);
}

void twin_path_tab(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t w,
                   twin_fixed_t h,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius)
{
    twin_matrix_t save = twin_path_current_matrix(path);

    twin_path_translate(path, x, y);
    twin_path_move(path, 0, y_radius);
    twin_path_arc(path, x_radius, y_radius, x_radius, y_radius, TWIN_ANGLE_180,
                  TWIN_ANGLE_90);
    twin_path_draw(path, w - x_radius, 0);
    twin_path_arc(path, w - x_radius, y_radius, x_radius, y_radius,
                  TWIN_ANGLE_270, TWIN_ANGLE_90);
    twin_path_draw(path, w, h);
    twin_path_draw(path, 0, h);
    twin_path_close(path);
    twin_path_set_matrix(path, save);
}

void twin_path_set_matrix(twin_path_t *path, twin_matrix_t matrix)
{
    path->state.matrix = matrix;
}

twin_matrix_t twin_path_current_matrix(twin_path_t *path)
{
    return path->state.matrix;
}

void twin_path_identity(twin_path_t *path)
{
    twin_matrix_identity(&path->state.matrix);
}

void twin_path_translate(twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_matrix_translate(&path->state.matrix, tx, ty);
}

void twin_path_scale(twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_matrix_scale(&path->state.matrix, sx, sy);
}

void twin_path_rotate(twin_path_t *path, twin_angle_t a)
{
    twin_matrix_rotate(&path->state.matrix, a);
}

void twin_path_set_font_size(twin_path_t *path, twin_fixed_t font_size)
{
    path->state.font_size = font_size;
}

twin_fixed_t twin_path_current_font_size(twin_path_t *path)
{
    return path->state.font_size;
}

void twin_path_set_font_style(twin_path_t *path, twin_style_t font_style)
{
    path->state.font_style = font_style;
}

twin_style_t twin_path_current_font_style(twin_path_t *path)
{
    return path->state.font_style;
}

void twin_path_set_cap_style(twin_path_t *path, twin_cap_t cap_style)
{
    path->state.cap_style = cap_style;
}

twin_cap_t twin_path_current_cap_style(twin_path_t *path)
{
    return path->state.cap_style;
}

void twin_path_empty(twin_path_t *path)
{
    path->npoints = 0;
    path->nsublen = 0;
}

void twin_path_bounds(twin_path_t *path, twin_rect_t *rect)
{
    twin_sfixed_t left = TWIN_SFIXED_MAX;
    twin_sfixed_t top = TWIN_SFIXED_MAX;
    twin_sfixed_t right = TWIN_SFIXED_MIN;
    twin_sfixed_t bottom = TWIN_SFIXED_MIN;
    int i;

    for (i = 0; i < path->npoints; i++) {
        twin_sfixed_t x = path->points[i].x;
        twin_sfixed_t y = path->points[i].y;
        if (x < left)
            left = x;
        if (x > right)
            right = x;
        if (y < top)
            top = y;
        if (y > bottom)
            bottom = y;
    }
    if (left >= right || top >= bottom)
        left = right = top = bottom = 0;
    rect->left = twin_sfixed_trunc(left);
    rect->top = twin_sfixed_trunc(top);
    rect->right = twin_sfixed_trunc(twin_sfixed_ceil(right));
    rect->bottom = twin_sfixed_trunc(twin_sfixed_ceil(bottom));
}

void twin_path_append(twin_path_t *dst, twin_path_t *src)
{
    for (int p = 0, s = 0; p < src->npoints; p++) {
        if (s < src->nsublen && p == src->sublen[s]) {
            _twin_path_sfinish(dst);
            s++;
        }
        _twin_path_sdraw(dst, src->points[p].x, src->points[p].y);
    }
}

twin_state_t twin_path_save(twin_path_t *path)
{
    return path->state;
}

void twin_path_restore(twin_path_t *path, twin_state_t *state)
{
    path->state = *state;
}

twin_path_t *twin_path_create(void)
{
    twin_path_t *path;

    path = malloc(sizeof(twin_path_t));
    path->npoints = path->size_points = 0;
    path->nsublen = path->size_sublen = 0;
    path->points = 0;
    path->sublen = 0;
    twin_matrix_identity(&path->state.matrix);
    path->state.font_size = TWIN_FIXED_ONE * 15;
    path->state.font_style = TwinStyleRoman;
    path->state.cap_style = TwinCapRound;
    return path;
}

void twin_path_destroy(twin_path_t *path)
{
    free(path->points);
    free(path->sublen);
    free(path);
}

void twin_composite_path(twin_pixmap_t *dst,
                         twin_operand_t *src,
                         twin_coord_t src_x,
                         twin_coord_t src_y,
                         twin_path_t *path,
                         twin_operator_t operator)
{
    twin_rect_t bounds;
    twin_path_bounds(path, &bounds);
    if (bounds.left >= bounds.right || bounds.top >= bounds.bottom)
        return;

    twin_coord_t width = bounds.right - bounds.left;
    twin_coord_t height = bounds.bottom - bounds.top;
    twin_pixmap_t *mask = twin_pixmap_create(TWIN_A8, width, height);
    if (!mask)
        return;

    twin_fill_path(mask, path, -bounds.left, -bounds.top);
    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = mask};
    twin_composite(dst, bounds.left, bounds.top, src, src_x + bounds.left,
                   src_y + bounds.top, &msk, 0, 0, operator, width, height);
    twin_pixmap_destroy(mask);
}

void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_composite_path(dst, &src, 0, 0, path, TWIN_OVER);
}

void twin_composite_stroke(twin_pixmap_t *dst,
                           twin_operand_t *src,
                           twin_coord_t src_x,
                           twin_coord_t src_y,
                           twin_path_t *stroke,
                           twin_fixed_t pen_width,
                           twin_operator_t operator)
{
    twin_path_t *pen = twin_path_create();
    twin_path_t *path = twin_path_create();
    twin_matrix_t m = twin_path_current_matrix(stroke);

    m.m[2][0] = 0;
    m.m[2][1] = 0;
    twin_path_set_matrix(pen, m);
    twin_path_set_cap_style(path, twin_path_current_cap_style(stroke));
    twin_path_circle(pen, 0, 0, pen_width / 2);
    twin_path_convolve(path, stroke, pen);
    twin_composite_path(dst, src, src_x, src_y, path, operator);
    twin_path_destroy(path);
    twin_path_destroy(pen);
}

void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width)
{
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = argb};
    twin_composite_stroke(dst, &src, 0, 0, stroke, pen_width, TWIN_OVER);
}
