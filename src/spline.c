/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Carl Worth <cworth@cworth.org>
 * All rights reserved.
 */

#include "twin_private.h"

typedef struct _twin_spline {
    twin_spoint_t a, b, c, d;
} twin_spline_t;

/*
 * Linearly interpolate between points 'a' and 'b' with a 'shift' factor.
 * The 'shift' factor determines the position between 'a' and 'b'.
 * The result is stored in 'result'.
 */
static void _lerp(const twin_spoint_t *a,
                  const twin_spoint_t *b,
                  int shift,
                  twin_spoint_t *result)
{
    result->x = a->x + ((b->x - a->x) >> shift);
    result->y = a->y + ((b->y - a->y) >> shift);
}

/*
 * Perform the de Casteljau algorithm to split a spline at a given 'shift'
 * factor. The spline is split into two new splines 's1' and 's2'.
 */
static void _de_casteljau(twin_spline_t *spline,
                          int shift,
                          twin_spline_t *s1,
                          twin_spline_t *s2)
{
    twin_spoint_t ab, bc, cd, abbc, bccd, final;

    _lerp(&spline->a, &spline->b, shift, &ab);
    _lerp(&spline->b, &spline->c, shift, &bc);
    _lerp(&spline->c, &spline->d, shift, &cd);
    _lerp(&ab, &bc, shift, &abbc);
    _lerp(&bc, &cd, shift, &bccd);
    _lerp(&abbc, &bccd, shift, &final);

    s1->a = spline->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = spline->d;
}

/*
 * Return an upper bound on the distance (squared) that could result from
 * approximating a spline with a line segment connecting the two endpoints,
 * which is based on the Convex Hull Property of Bézier Curves: The Bézier Curve
 * lies completely in the convex hull of the given control points. Therefore, we
 * can use control points B and C to approximate the actual spline.
 */
static twin_dfixed_t _twin_spline_distance_squared(twin_spline_t *spline)
{
    twin_dfixed_t bdist, cdist;

    bdist = _twin_distance_to_line_squared(&spline->b, &spline->a, &spline->d);
    cdist = _twin_distance_to_line_squared(&spline->c, &spline->a, &spline->d);

    if (bdist > cdist)
        return bdist;
    return cdist;
}

/*
 * Check if a spline is flat enough by comparing the distance against the
 * tolerance.
 */
static bool is_flat(twin_spline_t *spline, twin_dfixed_t tolerance_squared)
{
    return _twin_spline_distance_squared(spline) <= tolerance_squared;
}

/*
 * Decomposes a spline into a series of flat segments and draws them to a path.
 * Uses iterative approach to avoid deep recursion.
 * See https://keithp.com/blogs/iterative-splines/
 */
static void _twin_spline_decompose(twin_path_t *path,
                                   twin_spline_t *spline,
                                   twin_dfixed_t tolerance_squared)
{
    /* Draw starting point */
    _twin_path_sdraw(path, spline->a.x, spline->a.y);
    /*
     * It on average requires over two shift attempts per iteration to find the
     * optimal value. To reduce redundancy in shift 1, adjust the initial 't'
     * value from 0.5 to 0.25 by applying an initial shift of 2. As spline
     * rendering progresses, the shift amount decreases. Store the last shift
     * value as a global variable to use directly in the next iteration,
     * avoiding a reset to an initial shift of 2.
     */
    int shift = 2;
    while (!is_flat(spline, tolerance_squared)) {
        twin_spline_t left, right;

        while (true) {
            _de_casteljau(spline, shift, &left, &right);
            if (is_flat(&left, tolerance_squared)) {
                /* Limiting the scope of 't' may overlook optimal points with
                 * maximum curvature. Therefore, dynamically reduce the shift
                 * amount to a minimum of 1. */
                if (shift > 1)
                    shift--;
                break;
            }
            shift++;
        }

        /* Draw the left segment */
        _twin_path_sdraw(path, left.d.x, left.d.y);

        /* Update spline to the right segment */
        memcpy(spline, &right, sizeof(twin_spline_t));
    }

    /* Draw the ending point */
    _twin_path_sdraw(path, spline->d.x, spline->d.y);
}

void _twin_path_scurve(twin_path_t *path,
                       twin_sfixed_t x1,
                       twin_sfixed_t y1,
                       twin_sfixed_t x2,
                       twin_sfixed_t y2,
                       twin_sfixed_t x3,
                       twin_sfixed_t y3)
{
    if (path->npoints == 0)
        _twin_path_smove(path, 0, 0);

    twin_spline_t spline = {
        .a = path->points[path->npoints - 1],
        .b = {.x = x1, .y = y1},
        .c = {.x = x2, .y = y2},
        .d = {.x = x3, .y = y3},
    };
    _twin_spline_decompose(path, &spline,
                           TWIN_SFIXED_TOLERANCE * TWIN_SFIXED_TOLERANCE);
}

void twin_path_curve(twin_path_t *path,
                     twin_fixed_t x1,
                     twin_fixed_t y1,
                     twin_fixed_t x2,
                     twin_fixed_t y2,
                     twin_fixed_t x3,
                     twin_fixed_t y3)
{
    return _twin_path_scurve(path, _twin_matrix_x(&path->state.matrix, x1, y1),
                             _twin_matrix_y(&path->state.matrix, x1, y1),
                             _twin_matrix_x(&path->state.matrix, x2, y2),
                             _twin_matrix_y(&path->state.matrix, x2, y2),
                             _twin_matrix_x(&path->state.matrix, x3, y3),
                             _twin_matrix_y(&path->state.matrix, x3, y3));
}



typedef struct _twin_quad_spline {
    twin_spoint_t p0;  // 起點
    twin_spoint_t p1;  // 控制點
    twin_spoint_t p2;  // 終點
} twin_quad_spline_t;

/*
 * 二次貝茲曲線的 de Casteljau 演算法
 * 將曲線在給定的 shift 處分割成兩段
 */
static void _quad_de_casteljau(twin_quad_spline_t *spline,
                              int shift,
                              twin_quad_spline_t *s1,
                              twin_quad_spline_t *s2)
{
    twin_spoint_t p01, p12, p012;

    // 計算中間點
    _lerp(&spline->p0, &spline->p1, shift, &p01);
    _lerp(&spline->p1, &spline->p2, shift, &p12);
    _lerp(&p01, &p12, shift, &p012);

    // 分割成兩段
    s1->p0 = spline->p0;
    s1->p1 = p01;
    s1->p2 = p012;

    s2->p0 = p012;
    s2->p1 = p12;
    s2->p2 = spline->p2;
}

/*
 * 計算點到線段的距離平方，用於判斷曲線是否足夠平坦
 */
static twin_dfixed_t _quad_spline_distance_squared(twin_quad_spline_t *spline)
{
    // 計算控制點到起點和終點連線的距離
    return _twin_distance_to_line_squared(&spline->p1, &spline->p0, &spline->p2);
}

/*
 * 判斷曲線是否足夠平坦
 */
static bool _quad_is_flat(twin_quad_spline_t *spline, twin_dfixed_t tolerance_squared)
{
    return _quad_spline_distance_squared(spline) <= tolerance_squared;
}

/*
 * 分解二次貝茲曲線為一系列線段
 */
static void _twin_quad_spline_decompose(twin_path_t *path,
                                      twin_quad_spline_t *spline,
                                      twin_dfixed_t tolerance_squared)
{
    // 繪製起點
    _twin_path_sdraw(path, spline->p0.x, spline->p0.y);

    // 初始 shift 值
    int shift = 2;

    while (!_quad_is_flat(spline, tolerance_squared)) {
        twin_quad_spline_t left, right;

        while (true) {
            _quad_de_casteljau(spline, shift, &left, &right);
            if (_quad_is_flat(&left, tolerance_squared)) {
                if (shift > 1)
                    shift--;
                break;
            }
            shift++;
        }

        // 繪製左側段的終點
        _twin_path_sdraw(path, left.p2.x, left.p2.y);

        // 更新為右側段繼續處理
        *spline = right;
    }

    // 繪製終點
    _twin_path_sdraw(path, spline->p2.x, spline->p2.y);
}

/*
 * 在路徑中添加二次貝茲曲線（使用固定點座標）
 */
void _twin_path_squad_curve(twin_path_t *path,
                          twin_sfixed_t x1,
                          twin_sfixed_t y1,
                          twin_sfixed_t x2,
                          twin_sfixed_t y2)
{
    if (path->npoints == 0)
        _twin_path_smove(path, 0, 0);

    twin_quad_spline_t spline = {
        .p0 = path->points[path->npoints - 1],
        .p1 = {.x = x1, .y = y1},
        .p2 = {.x = x2, .y = y2}
    };

    _twin_quad_spline_decompose(path, &spline,
                               TWIN_SFIXED_TOLERANCE * TWIN_SFIXED_TOLERANCE);
}

/*
 * 公開的二次貝茲曲線函數
 */
void twin_path_quad_curve(twin_path_t *path,
                         twin_fixed_t x1,
                         twin_fixed_t y1,
                         twin_fixed_t x2,
                         twin_fixed_t y2)
{
    return _twin_path_squad_curve(path,
        _twin_matrix_x(&path->state.matrix, x1, y1),
        _twin_matrix_y(&path->state.matrix, x1, y1),
        _twin_matrix_x(&path->state.matrix, x2, y2),
        _twin_matrix_y(&path->state.matrix, x2, y2));
}

