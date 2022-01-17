/*
 * bresenham.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_BRESENHAM_H_
#define SRC_BRESENHAM_H_

#include "display.h"

void plotLine(display_t *, int x0, int y0, int x1, int y1, rgba_pixel_t col);
void plotRect(display_t *, int x0, int y0, int x1, int y1, rgba_pixel_t col);
void plotEllipse(display_t *, int xm, int ym, int a, int b, rgba_pixel_t col);
void plotOptimizedEllipse(display_t *, int xm, int ym, int a, int b, rgba_pixel_t col);
void plotCircle(display_t *, int xm, int ym, int r, rgba_pixel_t col);
void plotEllipseRect(display_t *, int x0, int y0, int x1, int y1, rgba_pixel_t col);
void plotQuadBezierSeg(display_t *, int x0, int y0, int x1, int y1, int x2, int y2, rgba_pixel_t col);
void plotQuadBezier(display_t *, int x0, int y0, int x1, int y1, int x2, int y2, rgba_pixel_t col);
void plotQuadRationalBezierSeg(display_t *, int x0, int y0, int x1, int y1, int x2, int y2, float w, rgba_pixel_t col);
void plotQuadRationalBezier(display_t *, int x0, int y0, int x1, int y1, int x2, int y2, float w, rgba_pixel_t col);
void plotRotatedEllipse(display_t *, int x, int y, int a, int b, float angle, rgba_pixel_t col);
void plotRotatedEllipseRect(display_t *, int x0, int y0, int x1, int y1, long zd, rgba_pixel_t col);
void plotCubicBezierSeg(display_t *, int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3, rgba_pixel_t col);
void plotCubicBezier(display_t *, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, rgba_pixel_t col);
void plotQuadSpline(display_t *, int n, int x[], int y[], rgba_pixel_t col);
void plotCubicSpline(display_t *, int n, int x[], int y[], rgba_pixel_t col);

#endif /* SRC_BRESENHAM_H_ */
